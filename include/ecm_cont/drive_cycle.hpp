#pragma once

#include "ecm/efficiency_map.hpp"
#include "ecm/segment.hpp"

#include <span>

namespace ecm_cont {

// Speed varies linearly over the interval. Road and solar inputs are constant.
struct SpeedInterval {
    double seconds;
    double initial_speed_mps;
    double final_speed_mps;
    double grade;  // Rise divided by horizontal run.
    double headwind_mps;  // Negative for a tailwind.
    double irradiance_w_m2;  // Plane-of-array irradiance.
};

struct DriveCycleEnergy {
    double seconds = 0.0;
    double distance_m = 0.0;
    double wheel_wh = 0.0;  // Signed net mechanical work at the wheels.
    double traction_wh = 0.0;  // Signed net DC traction energy.
    double auxiliary_wh = 0.0;
    double solar_wh = 0.0;

    [[nodiscard]] double net_dc_wh() const noexcept {
        return traction_wh + auxiliary_wh - solar_wh;
    }
};

// Adjacent intervals must share exactly the same boundary speed. Invalid inputs
// throw std::invalid_argument; non-finite results throw std::overflow_error.
[[nodiscard]] DriveCycleEnergy drive_cycle(const ecm::VehicleParams& vehicle,
                                           std::span<const SpeedInterval> intervals,
                                           const ecm::Drivetrain& drivetrain,
                                           const ecm::SolarArray& array = {});

// Wheel speed and torque select motoring efficiency from the map. Positive
// speed step (m/s) bounds the quadrature panel width. Regeneration remains
// governed by drivetrain.regen_efficiency and drivetrain.max_regen_power_w.
[[nodiscard]] DriveCycleEnergy drive_cycle_mapped(const ecm::VehicleParams& vehicle,
                                                  std::span<const SpeedInterval> intervals,
                                                  const ecm::Drivetrain& drivetrain,
                                                  const ecm::EfficiencyMap& map,
                                                  double max_speed_step_mps = 0.5,
                                                  const ecm::SolarArray& array = {});

}  // namespace ecm_cont
