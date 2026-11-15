#pragma once

#include "ecm_cont/route_energy.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ecm::lab {

struct Leg {
    int day;
    std::string destination;
    double distance_km;
    double grade_pct;
    double clear_sky_w_m2;
    double pace_cap_kmh;
    double control_minutes = 0.0;  // Stationary hold after arriving at this waypoint.
};

struct Weather {
    double solar_factor;
    double headwind_mps;
    double rolling_multiplier;
    double park_solar_factor;
};

using RouteEnergyModel = std::function<ecm_cont::RouteEnergy(
    const ecm_cont::RouteInput&, const ecm::VehicleParams&, const ecm::Drivetrain&, const ecm::SolarArray&)>;

enum class ReserveStrategy { tiers, energy_envelope };

struct StrategyConfig {
    double steady_kmh = 82.0;
    double push_kmh = 92.0;
    double reserve_fast_kmh = 92.0;
    double reserve_medium_kmh = 85.0;
    double reserve_slow_kmh = 75.0;
    double reserve_target_soc = 0.35;  // Tier buffer or minimum envelope charge.
    ReserveStrategy reserve_strategy = ReserveStrategy::tiers;
};

struct WeatherAdjustments {
    double solar_multiplier = 1.0;  // Multiplies driving and parked clear-sky fractions.
    double headwind_offset_mps = 0.0;
};

struct Config {
    ecm::VehicleParams vehicle{
        .rho = 1.18, .Cd = 0.100, .A = 1.0, .m = 225.0, .g = 9.80665, .Crr = 0.0042, .X = 65.0,
    };
    ecm::Drivetrain drivetrain{0.92, 0.60, 1400.0};
    ecm::SolarArray array{6.0, 0.24, 0.92};
    double capacity_wh = 5500.0;
    double start_soc = 0.95;
    double reserve_soc = 0.15;
    double parked_clear_sky_wh_m2 = 2200.0;
    RouteEnergyModel energy_model = ecm_cont::segment_route_energy;
    bool race_schedule = false;
    StrategyConfig strategy;
    WeatherAdjustments weather_adjustments;
};

[[nodiscard]] Config vehicle_config(std::string_view name);
[[nodiscard]] RouteEnergyModel route_energy_model(std::string_view name);

enum class Policy { steady, push, reserve_first };
inline constexpr std::array policies{Policy::steady, Policy::push, Policy::reserve_first};
[[nodiscard]] std::string_view policy_name(Policy policy);

struct Result {
    bool completed = true;
    bool reserve_breached = false;
    double distance_km = 0.0;
    double drive_hours = 0.0;
    double finish_soc = 0.0;
    double min_soc = 0.0;
    double demand_wh = 0.0;
    double solar_wh = 0.0;
    double spilled_wh = 0.0;
    double race_hours = 0.0;
};

enum class WeatherModel { baseline, posterior };

struct Experiment {
    std::uint64_t trials = 2000;
    std::uint64_t seed = 2025;
    WeatherModel weather = WeatherModel::baseline;
};

struct TrialResult {
    std::uint64_t trial;  // One-based, shared by all policies on a weather draw.
    Policy policy;
    Result result;
};

using TrialSink = std::function<void(const TrialResult&)>;

struct LegObservation {
    std::size_t leg;  // Zero-based route index.
    Weather weather;
    double distance_km;  // Cumulative, including a partly completed final leg.
    double cruise_kmh;
    double arrive_soc;
    double solar_wh;  // Energy collected while driving this leg.
    double demand_wh;
    double parked_wh;
    bool depleted;
    double elapsed_hours;  // Cumulative driving time, excluding parked time.
    double depart_soc;  // Charge after any parked solar credit.
    double spilled_wh;  // Driving and parked energy that exceeded capacity on this leg.
    double irradiance_w_m2;
    std::optional<ecm::EnergyBreakdown> components;
    double race_depart_hours;
    double race_arrive_hours;
    double control_hours;
    double overnight_hours;
    double mean_speed_kmh = 0.0;  // Distance over simulated driving time, including ramps.
    std::optional<double> planned_arrival_soc;  // Envelope target, possibly above capacity.
};

using LegSink = std::function<void(const LegObservation&)>;

struct TrialTrace {
    std::uint64_t trial = 0;  // One-based, required when a sink is supplied.
    std::function<void(Policy, const LegObservation&)> sink;
};

[[nodiscard]] std::vector<Leg> default_route();
[[nodiscard]] std::vector<Leg> read_route(const std::filesystem::path& path);
[[nodiscard]] std::vector<Leg> read_route(std::istream& csv, std::string_view source_name);
[[nodiscard]] std::vector<Weather> draw_weather(std::span<const Leg> route, std::mt19937_64& random);
[[nodiscard]] Result simulate(std::span<const Leg> route, std::span<const Weather> weather,
                              const Config& config, Policy policy, const LegSink& trace = {});

// Calls the sink synchronously for each result in trial and policy order.
// Exceptions from the model or sink stop the experiment and propagate.
void run_experiment(std::span<const Leg> route, const Config& config,
                    Experiment experiment, const TrialSink& sink, const TrialTrace& trace = {});

}  // namespace ecm::lab
