#include "ecm/lab.hpp"
#include "ecm/fleet.hpp"
#include "ecm/forecast.hpp"
#include "default_route.hpp"
#include "race_plan.hpp"

#include <sstream>
#include <stdexcept>

namespace ecm::lab {

std::vector<Leg> default_route() {
    std::istringstream csv(default_route_csv);
    return read_route(csv, "built-in outback route");
}

namespace {

void validate_experiment(std::span<const Leg> route, Experiment experiment,
                         bool has_sink, bool has_trace, std::uint64_t trace_trial) {
    if (route.empty() || experiment.trials == 0 || !has_sink) {
        throw std::invalid_argument("an experiment requires a route, at least one trial and a result sink");
    }
    if (has_trace && (trace_trial == 0 || trace_trial > experiment.trials)) {
        throw std::invalid_argument("traced trial must be between 1 and the number of trials");
    }

    if (!(experiment.rain_multiplier >= 0.0 && experiment.rain_multiplier <= 8.0)) {
        throw std::invalid_argument("rain exposure multiplier must be from 0 to 8");
    }
}

void for_weather_draws(std::span<const Leg> route, Experiment experiment, const auto& consume) {
    std::mt19937_64 random(experiment.seed);
    // Rain has a separate stream so enabling it preserves the underlying paired weather draws.
    std::seed_seq rain_seed{static_cast<std::uint32_t>(experiment.seed),
                            static_cast<std::uint32_t>(experiment.seed >> 32), 0x7261696eU};
    std::mt19937_64 rain_random(rain_seed);
    std::optional<ForecastCalibration> calibration;
    if (experiment.weather == WeatherModel::posterior) {
        calibration = calibrate_forecast(synthetic_weather_residuals(), experiment.seed);
    }
    for (std::uint64_t trial = 0; trial < experiment.trials; ++trial) {
        auto weather = calibration ? draw_forecast_weather(route, *calibration, random)
                                   : draw_weather(route, random);
        if (experiment.rain_multiplier > 0.0) draw_rain(route, weather, experiment.rain_multiplier, rain_random);
        consume(trial + 1, weather);
    }
}

void run_trials(std::span<const Leg> route, Experiment experiment,
                const TrialSink& sink, const TrialTrace& trace, const auto& simulate_policy) {
    for_weather_draws(route, experiment, [&](std::uint64_t trial, std::span<const Weather> weather) {
        for (const auto policy : policies) {
            LegSink leg_sink;
            if (trace.sink && trace.trial == trial) {
                leg_sink = [&](const LegObservation& leg) { trace.sink(policy, leg); };
            }
            sink({trial, policy, simulate_policy(weather, policy, leg_sink)});
        }
    });
}

}  // namespace

void run_experiment(std::span<const Leg> route, const Config& config,
                    Experiment experiment, const TrialSink& sink, const TrialTrace& trace) {
    validate_experiment(route, experiment, static_cast<bool>(sink), static_cast<bool>(trace.sink), trace.trial);
    const auto targets = detail::prepare_race(route, config);
    run_trials(route, experiment, sink, trace, [&](std::span<const Weather> weather, Policy policy, const LegSink& legs) {
        return detail::simulate_prepared(route, weather, config, policy, targets, legs);
    });
}

void run_experiment(const PreparedRace& race, Experiment experiment, const TrialSink& sink, const TrialTrace& trace) {
    validate_experiment(race.route(), experiment, static_cast<bool>(sink), static_cast<bool>(trace.sink), trace.trial);
    run_trials(race.route(), experiment, sink, trace, [&](std::span<const Weather> weather, Policy policy, const LegSink& legs) {
        return race.simulate(weather, policy, legs);
    });
}

void run_fleet(std::span<const Leg> route, std::span<const FleetEntry> fleet,
               Experiment experiment, const FleetSink& sink, const FleetTrace& trace) {
    validate_experiment(route, experiment, static_cast<bool>(sink), static_cast<bool>(trace.sink), trace.trial);
    if (fleet.empty()) throw std::invalid_argument("a fleet requires at least one entry");

    std::vector<PreparedRace> racers;
    racers.reserve(fleet.size());
    for (const auto& entry : fleet) {
        racers.emplace_back(std::vector<Leg>(route.begin(), route.end()), entry.config);
    }
    for_weather_draws(route, experiment, [&](std::uint64_t trial, std::span<const Weather> weather) {
        for (std::size_t index = 0; index < fleet.size(); ++index) {
            LegSink leg_sink;
            if (trace.sink && trace.trial == trial) {
                leg_sink = [&](const LegObservation& leg) { trace.sink(index, leg); };
            }
            sink({trial, index, racers[index].simulate(weather, fleet[index].policy, leg_sink)});
        }
    });
}

}  // namespace ecm::lab
