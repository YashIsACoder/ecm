#pragma once

#include "ecm/segment.hpp"

#include <cmath>
#include <stdexcept>

namespace ecm::detail {

inline constexpr double joules_per_wh = 3600.0;

inline bool nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0;
}

inline bool positive(double value) {
    return nonnegative(value) && value > 0.0;
}

inline bool efficiency(double value) {
    return nonnegative(value) && value <= 1.0;
}

inline void validate_drive(const VehicleParams& vehicle, const Drivetrain& drivetrain) {
    if (!positive(vehicle.m) || !positive(vehicle.g) || !positive(vehicle.rho) ||
        !nonnegative(vehicle.Cd) || !nonnegative(vehicle.A) || !nonnegative(vehicle.Crr) ||
        !nonnegative(vehicle.X) || !efficiency(drivetrain.drive_efficiency) ||
        drivetrain.drive_efficiency == 0.0 || !efficiency(drivetrain.regen_efficiency) ||
        !nonnegative(drivetrain.max_regen_power_w)) {
        throw std::invalid_argument("invalid vehicle or drivetrain parameters");
    }
}

}  // namespace ecm::detail
