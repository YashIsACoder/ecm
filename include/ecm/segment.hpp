#pragma once

#include "ecm/params.hpp"

namespace ecm {

struct RoadSegment {
    double distance_m;
    double cruise_speed_mps;
    double entry_speed_mps;
    double exit_speed_mps;
    double grade;  // Rise divided by horizontal run.
    double headwind_mps;  // Negative for a tailwind.
};

struct Drivetrain {
    double drive_efficiency;
    double regen_efficiency;
    double max_regen_power_w;  // Electrical power recovered at the DC bus.
};

// Signed mechanical work at the wheels and electrical energy at the DC bus.
// Recovered energy is a positive magnitude already subtracted from traction.
struct EnergyBreakdown {
    double drag_wh;
    double rolling_wh;
    double climb_wh;
    double kinetic_wh;
    double traction_wh;
    double auxiliary_wh;
    double recovered_wh;
};

struct SegmentEnergy {
    double seconds;
    double drag_wh;
    double rolling_wh;
    double climb_wh;
    double kinetic_wh;
    double traction_wh;  // Negative during net regeneration.
    double auxiliary_wh;

    [[nodiscard]] double wheel_wh() const noexcept;
    [[nodiscard]] double demand_wh() const noexcept;
    [[nodiscard]] EnergyBreakdown breakdown() const noexcept;
};

struct SolarArray {
    double area_m2;
    double cell_efficiency;
    double system_efficiency;
};

// Cruise over the segment with a lumped net change in kinetic energy.
// Invalid inputs throw std::invalid_argument; overflow throws std::overflow_error.
[[nodiscard]] SegmentEnergy drive_energy(const VehicleParams& vehicle, const RoadSegment& segment,
                                        const Drivetrain& drivetrain);

// Plane-of-array irradiance in W/m² and exposure in seconds give DC energy in Wh.
[[nodiscard]] double solar_energy_wh(const SolarArray& array, double irradiance_w_m2, double seconds);

}  // namespace ecm
