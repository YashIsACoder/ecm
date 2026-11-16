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
    double rain_day_probability = .05;  // Climatological wet-day probability for the leg's region.
};

struct Weather {
    double solar_factor;
    double headwind_mps;
    double rolling_multiplier;
    double park_solar_factor;
    double rain_mm_h = 0.0;
    double road_wetness = 0.0;  // Dimensionless fraction, including residual wetness after rain.
    double rain_probability = 0.0;  // Marginal chance of rain in this leg.
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
    std::size_t envelope_speed_samples = 9;  // Inclusive endpoints, 2–65 samples.
};

struct WeatherAdjustments {
    double solar_multiplier = 1.0;  // Multiplies driving and parked clear-sky fractions.
    double headwind_offset_mps = 0.0;
};

enum class StageTiming { fixed, bwsc_2025 };
enum class EnergyRules { scenario, bwsc_2025_cruiser };

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
    StageTiming stage_timing = StageTiming::fixed;
    EnergyRules energy_rules = EnergyRules::scenario;
    double overnight_charge_power_w = 1500.0;  // Battery-side delivery limit in Cruiser mode.
};

[[nodiscard]] Config vehicle_config(std::string_view name);
[[nodiscard]] RouteEnergyModel route_energy_model(std::string_view name);
[[nodiscard]] RouteEnergyModel route_energy_model(std::string_view name, double gust_sd_mps);

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
    double stage_penalty_hours = 0.0;  // Sum of carried-over next-start penalties.
    double external_charge_wh = 0.0;  // Battery-side overnight top-ups.
};

enum class WeatherModel { baseline, posterior };

struct Experiment {
    std::uint64_t trials = 2000;
    std::uint64_t seed = 2025;
    WeatherModel weather = WeatherModel::baseline;
    double rain_multiplier = 0.0;  // Rain occurrence hazard multiplier, from 0 (disabled) to 8.
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
    double effective_pace_cap_kmh = 0.0;
    double stage_penalty_hours = 0.0;  // Delay caused by this planned overnight arrival.
    double external_charge_wh = 0.0;  // Charge from the overnight preceding this leg.
};

using LegSink = std::function<void(const LegObservation&)>;

struct TrialTrace {
    std::uint64_t trial = 0;  // One-based, required when a sink is supplied.
    std::function<void(Policy, const LegObservation&)> sink;
};

// Owns the itinerary and model and computes the envelope once for repeated simulations.
class PreparedRace {
public:
    // Optional weather is the planning forecast, not the later observed draws.
    // Supply one raw weather value per leg. An empty span uses the default planning scenario.
    PreparedRace(std::vector<Leg> route, Config config, std::span<const Weather> planning_weather = {});

    [[nodiscard]] std::span<const Leg> route() const noexcept { return route_; }
    // Required arrival charge in Wh, one value per leg for the envelope policy.
    [[nodiscard]] std::span<const double> arrival_targets_wh() const noexcept { return targets_; }
    [[nodiscard]] Result simulate(std::span<const Weather> weather, Policy policy, const LegSink& trace = {}) const;

private:
    std::vector<Leg> route_;
    Config config_;
    std::vector<double> targets_;
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
void run_experiment(const PreparedRace& race, Experiment experiment, const TrialSink& sink,
                    const TrialTrace& trace = {});

}  // namespace ecm::lab
