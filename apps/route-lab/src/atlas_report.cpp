#include "atlas_report.hpp"

#include "ecm/fleet.hpp"
#include "ecm/lab_statistics.hpp"
#include "ecm/solar.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <utility>
#include <vector>

namespace {

using namespace ecm::lab;

void write_interval(std::ostream& out, Interval95 interval) {
    out << '[' << interval.estimate << ',' << interval.low << ',' << interval.high << ']';
}

void write_leg(std::ostream& out, const LegObservation& leg) {
    out << "{\"leg\":" << leg.leg << ",\"distance_km\":" << leg.distance_km
        << ",\"speed_kmh\":" << leg.cruise_kmh << ",\"mean_speed_kmh\":" << leg.mean_speed_kmh
        << ",\"soc_pct\":" << leg.arrive_soc * 100.0 << ",\"depart_soc_pct\":" << leg.depart_soc * 100.0
        << ",\"solar_wh\":" << leg.solar_wh << ",\"demand_wh\":" << leg.demand_wh
        << ",\"external_charge_wh\":" << leg.external_charge_wh << ",\"depleted\":" << leg.depleted
        << ",\"race_depart_hours\":" << leg.race_depart_hours << ",\"race_arrive_hours\":" << leg.race_arrive_hours
        << ",\"control_hours\":" << leg.control_hours << ",\"overnight_hours\":" << leg.overnight_hours
        << ",\"solar_factor\":" << leg.weather.solar_factor << ",\"headwind_mps\":" << leg.weather.headwind_mps
        << ",\"rain_mm_h\":" << leg.weather.rain_mm_h << ",\"road_wetness\":" << leg.weather.road_wetness << '}';
}

void write_sun(std::ostream& out, double coverage_hours) {
    using namespace std::chrono;
    // Six-day atlas begins on 24 August 2025 at 08:00 Australian Central Standard Time.
    const auto start = sys_days{2025y / August / 23} + hours{22} + minutes{30};
    constexpr int step_minutes = 10;
    const auto steps = static_cast<int>(std::ceil(coverage_hours * 60.0 / step_minutes));
    out << ",\"solar\":{\"start_utc\":\"2025-08-23T22:30:00Z\",\"acst_offset_hours\":9.5"
        << ",\"step_minutes\":" << step_minutes << ",\"vectors\":[";
    for (int index = 0; index <= steps; ++index) {
        if (index) out << ',';
        const auto sun = sun_direction(start + minutes{index * step_minutes});
        out << '[' << sun.x << ',' << sun.y << ',' << sun.z << ']';
    }
    out << "]}";
}

}  // namespace

void write_atlas_report(std::span<const ecm::lab::Leg> route, ecm::lab::Experiment experiment,
                        std::uint64_t traced_trial, std::string_view model_name, double gust_sd_mps,
                        std::optional<ecm::lab::FleetEntry> custom, std::ostream& out) {
    using namespace ecm::lab;
    const auto model = route_energy_model(model_name, gust_sd_mps);
    auto profiles = bwsc_2025_cruiser_profiles();
    if (custom) profiles.push_back({"custom", "Custom Cruiser", "Custom Cruiser", "Scenario", std::move(*custom)});
    const auto count = profiles.size();
    std::vector<FleetEntry> field;
    field.reserve(count);
    for (const auto& profile : profiles) {
        auto entry = profile.entry;
        entry.config.energy_model = model;
        field.push_back(std::move(entry));
    }

    std::vector<std::vector<LegObservation>> traces(count);
    std::vector<std::vector<double>> finish_hours(count);
    std::vector<PairedOutcomes> paired(count);
    bool reference_finished = false;
    run_fleet(route, field, experiment, [&](const FleetResult& result) {
        if (result.entry == 0) reference_finished = result.result.completed;
        paired[result.entry].add(result.result.completed, reference_finished);
        if (result.result.completed) finish_hours[result.entry].push_back(result.result.race_hours);
    }, {traced_trial, [&](std::size_t entry, const LegObservation& observation) {
        traces[entry].push_back(observation);
    }});

    out << std::fixed << std::setprecision(6) << "{\"trials\":" << experiment.trials
        << ",\"seed\":\"" << experiment.seed << "\",\"draw\":" << traced_trial
        << ",\"model\":" << std::quoted(model_name)
        << ",\"weather\":" << std::quoted(experiment.weather == WeatherModel::posterior ? "posterior" : "baseline")
        << ",\"rain_multiplier\":" << experiment.rain_multiplier << ",\"gust_sd_mps\":" << gust_sd_mps
        << ",\"points\":[";
    double distance = 0.0;
    for (std::size_t i = 0; i < route.size(); ++i) {
        if (i) out << ',';
        distance += route[i].distance_km;
        out << "{\"name\":" << std::quoted(route[i].destination) << ",\"km\":" << distance
            << ",\"control_minutes\":" << route[i].control_minutes << '}';
    }
    out << "],\"teams\":[";
    double horizon = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (index) out << ',';
        const auto& profile = profiles[index];
        const auto& config = field[index].config;
        auto& times = finish_hours[index];
        out << "{\"id\":" << std::quoted(profile.id) << ",\"team\":" << std::quoted(profile.team)
            << ",\"car\":" << std::quoted(profile.car) << ",\"region\":" << std::quoted(profile.region)
            << ",\"mass_kg\":" << config.vehicle.m << ",\"drag_area_m2\":" << config.vehicle.Cd * config.vehicle.A
            << ",\"rolling_coefficient\":" << config.vehicle.Crr << ",\"auxiliary_w\":" << config.vehicle.X
            << ",\"array_m2\":" << config.array.area_m2 << ",\"cell_efficiency\":" << config.array.cell_efficiency
            << ",\"pack_kwh\":" << config.capacity_wh / 1000.0
            << ",\"pace_ceiling_kmh\":" << config.strategy.reserve_fast_kmh << ",\"finish_rate\":";
        write_interval(out, wilson95(times.size(), experiment.trials));
        out << ",\"finish_delta\":";
        write_interval(out, paired[index].difference95());
        out << ",\"median_finish_hours\":";
        if (times.empty()) {
            out << "null";
        } else {
            std::sort(times.begin(), times.end());
            out << (times[times.size() / 2] + times[(times.size() - 1) / 2]) / 2.0;
        }
        out << ",\"legs\":[";
        for (std::size_t i = 0; i < traces[index].size(); ++i) {
            if (i) out << ',';
            write_leg(out, traces[index][i]);
        }
        out << "]}";
        horizon = std::max(horizon, traces[index].back().race_arrive_hours);
    }
    out << "],\"horizon_hours\":" << horizon;
    write_sun(out, horizon);
    out << "}\n";
}
