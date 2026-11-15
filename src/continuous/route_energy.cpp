#include "ecm_cont/route_energy.hpp"

#include "detail/energy_inputs.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ecm_cont {
namespace {

RouteEnergy route_energy(const DriveCycleEnergy& energy) {
    return {energy.seconds, energy.traction_wh + energy.auxiliary_wh, energy.solar_wh};
}

}  // namespace

std::vector<SpeedInterval> route_speed_profile(const RouteInput& input, RampRates rates) {
    const auto& road = input.road;
    using ecm::detail::nonnegative;
    using ecm::detail::positive;
    if (!positive(road.distance_m) || !positive(road.cruise_speed_mps) ||
        !nonnegative(road.entry_speed_mps) || !nonnegative(road.exit_speed_mps) ||
        !positive(rates.acceleration_mps2) || !positive(rates.braking_mps2) ||
        !std::isfinite(road.grade) || !std::isfinite(road.headwind_mps) || !nonnegative(input.irradiance_w_m2)) {
        throw std::invalid_argument("invalid route, irradiance or ramp rates");
    }
    const auto ramp_seconds = [rates](double from, double to) {
        return to >= from ? (to - from) / rates.acceleration_mps2
                          : (from - to) / rates.braking_mps2;
    };
    const auto ramp_distance = [&](double from, double to) {
        return ramp_seconds(from, to) * std::midpoint(from, to);
    };
    const double entry = road.entry_speed_mps;
    const double exit = road.exit_speed_mps;
    const double length = road.distance_m;
    const double tolerance = 1e-12 * length;
    if (ramp_distance(entry, exit) - length > tolerance) {
        throw std::invalid_argument("leg is too short for its entry and exit speeds");
    }

    double cruise = road.cruise_speed_mps;
    if (ramp_distance(entry, cruise) + ramp_distance(cruise, exit) - length > tolerance) {
        const double acceleration = rates.acceleration_mps2;
        const double braking = rates.braking_mps2;
        const double divisor = 1.0 / acceleration + 1.0 / braking;
        if (!std::isfinite(divisor) || divisor <= 0.0) {
            throw std::overflow_error("ramp rates exceed double precision range");
        }
        if (cruise >= std::max(entry, exit)) {
            const double peak_squared = (2.0 * length + entry * entry / acceleration + exit * exit / braking) / divisor;
            if (!std::isfinite(peak_squared)) {
                throw std::overflow_error("peak speed exceeds double precision range");
            }
            cruise = std::max(std::max(entry, exit), std::sqrt(peak_squared));
        } else if (cruise <= std::min(entry, exit)) {
            const double trough_squared = (entry * entry / braking + exit * exit / acceleration - 2.0 * length) / divisor;
            if (!std::isfinite(trough_squared)) {
                throw std::overflow_error("trough speed exceeds double precision range");
            }
            cruise = std::min(std::min(entry, exit), std::sqrt(std::max(0.0, trough_squared)));
        }
    }
    const double remaining = length - ramp_distance(entry, cruise) - ramp_distance(cruise, exit);
    if (!std::isfinite(remaining) || remaining < -tolerance || (remaining > tolerance && cruise <= 0.0)) {
        throw std::overflow_error("cannot construct a finite route speed profile");
    }

    std::vector<SpeedInterval> intervals;
    intervals.reserve(3);
    const auto append = [&](double seconds, double start, double finish) {
        if (!std::isfinite(seconds)) {
            throw std::overflow_error("route duration exceeds double precision range");
        }
        if (seconds > 0.0) {
            intervals.push_back({seconds, start, finish, road.grade, road.headwind_mps, input.irradiance_w_m2});
        }
    };
    append(ramp_seconds(entry, cruise), entry, cruise);
    if (remaining > tolerance) {
        append(remaining / cruise, cruise, cruise);
    }
    append(ramp_seconds(cruise, exit), cruise, exit);
    if (intervals.empty()) {
        throw std::overflow_error("route duration is too small to represent");
    }
    return intervals;
}

RouteEnergy segment_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                  const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array) {
    const auto energy = ecm::drive_energy(vehicle, input.road, drivetrain);
    return {energy.seconds, energy.demand_wh(), ecm::solar_energy_wh(array, input.irradiance_w_m2, energy.seconds)};
}

RouteEnergy cycle_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array, RampRates rates) {
    const auto profile = route_speed_profile(input, rates);
    return route_energy(drive_cycle(vehicle, profile, drivetrain, array));
}

RouteEnergy mapped_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                 const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array,
                                 const ecm::EfficiencyMap& map, RampRates rates, double max_speed_step_mps) {
    const auto profile = route_speed_profile(input, rates);
    return route_energy(drive_cycle_mapped(vehicle, profile, drivetrain, map, max_speed_step_mps, array));
}

}  // namespace ecm_cont
