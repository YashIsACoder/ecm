#pragma once

#include "ecm_cont/drive_cycle.hpp"

#include <optional>
#include <vector>

namespace ecm_cont {

struct RouteInput {
    ecm::RoadSegment road;
    double irradiance_w_m2;
};

struct RampRates {
    double acceleration_mps2 = 0.6;
    double braking_mps2 = 1.0;
};

struct RouteEnergy {
    double seconds;
    double demand_wh;  // Signed DC traction plus auxiliaries.
    double solar_wh;
    std::optional<ecm::EnergyBreakdown> components = std::nullopt;

    [[nodiscard]] double net_wh() const noexcept { return demand_wh - solar_wh; }
};

// Build entry ramp, cruise and exit ramp with the requested boundary speeds.
// Short legs adjust or omit cruise. Infeasible endpoint speeds throw.
[[nodiscard]] std::vector<SpeedInterval> route_speed_profile(const RouteInput& input, RampRates rates = {});

[[nodiscard]] RouteEnergy segment_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                                const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array);
[[nodiscard]] RouteEnergy cycle_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                              const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array,
                                              RampRates rates = {});
[[nodiscard]] RouteEnergy mapped_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                               const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array,
                                               const ecm::EfficiencyMap& map, RampRates rates = {},
                                               double max_speed_step_mps = 0.5);

// Average the mapped cycle over a Gaussian longitudinal gust distribution.
[[nodiscard]] RouteEnergy gust_route_energy(const RouteInput& input, const ecm::VehicleParams& vehicle,
                                             const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array,
                                             const ecm::EfficiencyMap& map, double gust_sd_mps,
                                             RampRates rates = {}, double max_speed_step_mps = 0.5);

}  // namespace ecm_cont
