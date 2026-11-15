#include "ecm_cont/drive_cycle.hpp"

#include "../energy_inputs.hpp"
#include "ecm/forces.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace ecm_cont {
namespace {

double finite(double value) {
    if (!std::isfinite(value)) {
        throw std::overflow_error("drive cycle exceeds double precision range");
    }
    return value;
}

// Every initial bracket lies in a monotone piece of the function.
template <class Function>
void add_crossings(std::vector<double>& knots, const Function& function) {
    const auto count = knots.size();
    for (std::size_t i = 1; i < count; ++i) {
        double left = knots[i - 1];
        double right = knots[i];
        const double first = function(left);
        const double last = function(right);
        if (first == 0.0 || last == 0.0 || std::signbit(first) == std::signbit(last)) {
            continue;
        }

        double middle = std::midpoint(left, right);
        while (middle > left && middle < right) {
            const double value = function(middle);
            if (value == 0.0) {
                break;
            }
            if (std::signbit(value) == std::signbit(first)) {
                left = middle;
            } else {
                right = middle;
            }
            middle = std::midpoint(left, right);
        }
        if (middle > knots[i - 1] && middle < knots[i]) {
            knots.push_back(middle);
        }
    }
    std::ranges::sort(knots);
}

void integrate_traction(const ecm::VehicleParams& vehicle, const SpeedInterval& interval,
                        const ecm::Drivetrain& drivetrain, DriveCycleEnergy& energy) {
    const ecm::Forces forces(vehicle);
    const double acceleration = (interval.final_speed_mps - interval.initial_speed_mps) / interval.seconds;
    const double constant_force = finite(vehicle.m * acceleration +
        forces.rolling() * std::cos(std::atan(interval.grade)) + forces.grade(interval.grade));
    const double drag_coefficient = finite(0.5 * vehicle.rho * vehicle.Cd * vehicle.A);
    const double wind = interval.headwind_mps;
    const auto wheel_power = [=](double speed) {
        const double relative_air = speed + wind;
        return finite(speed * (constant_force + drag_coefficient * relative_air * std::abs(relative_air)));
    };
    const auto dc_power = [&](double wheel) {
        return wheel >= 0.0 ? finite(wheel / drivetrain.drive_efficiency)
                            : std::max(wheel * drivetrain.regen_efficiency, -drivetrain.max_regen_power_w);
    };

    const auto [first, last] = std::minmax(interval.initial_speed_mps, interval.final_speed_mps);
    const double hours = interval.seconds / ecm::detail::joules_per_wh;
    if (first == last) {
        const double wheel = wheel_power(first);
        energy.wheel_wh += hours * wheel;
        energy.traction_wh += hours * dc_power(wheel);
        return;
    }

    // A cut at zero relative airspeed separates the drag polynomials. The
    // other cut separates monotone pieces of the wheel power derivative.
    std::vector<double> knots{first, last};
    for (double cut : {-wind, -(2.0 / 3.0) * wind}) {
        if (cut > first && cut < last) {
            knots.push_back(cut);
        }
    }
    std::ranges::sort(knots);
    add_crossings(knots, [=](double speed) {
        const double relative_air = speed + wind;
        return finite(constant_force +
            drag_coefficient * std::abs(relative_air) * relative_air +
            2.0 * (drag_coefficient * std::abs(relative_air) * speed));
    });
    add_crossings(knots, wheel_power);
    if (drivetrain.regen_efficiency > 0.0 && drivetrain.max_regen_power_w > 0.0) {
        const double cap = drivetrain.max_regen_power_w;
        add_crossings(knots, [&](double speed) {
            return std::min(wheel_power(speed), 0.0) * drivetrain.regen_efficiency + cap;
        });
    }

    // Cubic polynomials are integrated exactly by two Gauss–Legendre points.
    constexpr double left_node = 0.21132486540518712;
    constexpr double right_node = 1.0 - left_node;
    for (std::size_t i = 1; i < knots.size(); ++i) {
        const double left = knots[i - 1];
        const double right = knots[i];
        const double duration_hours = hours * ((right - left) / (last - first));
        const double left_power = wheel_power(std::lerp(left, right, left_node));
        const double right_power = wheel_power(std::lerp(left, right, right_node));
        energy.wheel_wh += duration_hours * std::midpoint(left_power, right_power);
        energy.traction_wh += duration_hours * std::midpoint(dc_power(left_power), dc_power(right_power));
    }
}

}  // namespace

DriveCycleEnergy drive_cycle(const ecm::VehicleParams& vehicle, std::span<const SpeedInterval> intervals,
                             const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array) {
    ecm::detail::validate_drive(vehicle, drivetrain);
    if (intervals.empty()) {
        throw std::invalid_argument("drive cycle requires at least one speed interval");
    }

    DriveCycleEnergy result;
    double previous_speed = intervals.front().initial_speed_mps;
    for (const auto& interval : intervals) {
        if (!ecm::detail::positive(interval.seconds) ||
            !ecm::detail::nonnegative(interval.initial_speed_mps) ||
            !ecm::detail::nonnegative(interval.final_speed_mps) ||
            !std::isfinite(interval.grade) || !std::isfinite(interval.headwind_mps) ||
            interval.initial_speed_mps != previous_speed) {
            throw std::invalid_argument("invalid drive cycle interval or discontinuous speed");
        }

        const double hours = interval.seconds / ecm::detail::joules_per_wh;
        result.seconds += interval.seconds;
        result.distance_m += interval.seconds * std::midpoint(interval.initial_speed_mps, interval.final_speed_mps);
        result.auxiliary_wh += vehicle.X * hours;
        result.solar_wh += ecm::solar_energy_wh(array, interval.irradiance_w_m2, interval.seconds);
        integrate_traction(vehicle, interval, drivetrain, result);
        previous_speed = interval.final_speed_mps;
    }
    if (!std::isfinite(result.seconds) || !std::isfinite(result.distance_m) ||
        !std::isfinite(result.wheel_wh) || !std::isfinite(result.net_dc_wh())) {
        throw std::overflow_error("drive cycle totals exceed double precision range");
    }
    return result;
}

}  // namespace ecm_cont
