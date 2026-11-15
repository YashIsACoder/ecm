#include "ecm/segment.hpp"

#include "ecm/forces.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ecm {
namespace {

constexpr double joules_per_wh = 3600.0;

bool nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

bool positive(double value) {
    return nonnegative(value) && value > 0.0;
}

bool efficiency(double value) {
    return nonnegative(value) && value <= 1.0;
}

}  // namespace

double SegmentEnergy::wheel_wh() const noexcept {
    return drag_wh + rolling_wh + climb_wh + kinetic_wh;
}

double SegmentEnergy::demand_wh() const noexcept {
    return traction_wh + auxiliary_wh;
}

SegmentEnergy drive_energy(const VehicleParams& vehicle, const RoadSegment& segment,
                           const Drivetrain& drivetrain) {
    if (!positive(vehicle.m) || !positive(vehicle.g) || !positive(vehicle.rho) ||
        !nonnegative(vehicle.Cd) || !nonnegative(vehicle.A) || !nonnegative(vehicle.Crr) ||
        !nonnegative(vehicle.X) || !efficiency(drivetrain.drive_efficiency) ||
        drivetrain.drive_efficiency == 0.0 || !efficiency(drivetrain.regen_efficiency) ||
        !nonnegative(drivetrain.max_regen_power_w)) {
        throw std::invalid_argument("invalid vehicle or drivetrain parameters");
    }
    if (!positive(segment.distance_m) || !positive(segment.cruise_speed_mps) ||
        !nonnegative(segment.entry_speed_mps) || !nonnegative(segment.exit_speed_mps) ||
        !std::isfinite(segment.grade) || !std::isfinite(segment.headwind_mps)) {
        throw std::invalid_argument("invalid road segment");
    }

    const Forces forces(vehicle);
    const double seconds = segment.distance_m / segment.cruise_speed_mps;
    const double airspeed = segment.cruise_speed_mps + segment.headwind_mps;
    const double distance_per_wh = segment.distance_m / joules_per_wh;
    const double kinetic_j = 0.5 * vehicle.m *
        (segment.exit_speed_mps * segment.exit_speed_mps - segment.entry_speed_mps * segment.entry_speed_mps);

    SegmentEnergy result{
        seconds,
        std::copysign(forces.aero(airspeed), airspeed) * distance_per_wh,
        forces.rolling() * std::cos(std::atan(segment.grade)) * distance_per_wh,
        forces.grade(segment.grade) * distance_per_wh,
        kinetic_j / joules_per_wh,
        0.0,
        vehicle.X * seconds / joules_per_wh,
    };
    const double wheel_wh = result.wheel_wh();
    const double recovery_cap_wh = drivetrain.max_regen_power_w * seconds / joules_per_wh;
    result.traction_wh = wheel_wh >= 0.0
        ? wheel_wh / drivetrain.drive_efficiency
        : std::max(wheel_wh * drivetrain.regen_efficiency, -recovery_cap_wh);
    if (!std::isfinite(wheel_wh) || !std::isfinite(result.demand_wh()) || !std::isfinite(seconds)) {
        throw std::overflow_error("drive energy or duration exceeds double precision range");
    }
    return result;
}

double solar_energy_wh(const SolarArray& array, double irradiance_w_m2, double seconds) {
    if (!nonnegative(array.area_m2) || !efficiency(array.cell_efficiency) ||
        !efficiency(array.system_efficiency) || !nonnegative(irradiance_w_m2) || !nonnegative(seconds)) {
        throw std::invalid_argument("invalid solar array or exposure");
    }
    const double energy = array.area_m2 * array.cell_efficiency * array.system_efficiency *
        irradiance_w_m2 * (seconds / joules_per_wh);
    if (!std::isfinite(energy)) {
        throw std::overflow_error("solar energy exceeds double precision range");
    }
    return energy;
}

}  // namespace ecm
