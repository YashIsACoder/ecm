#include "ecm/lab.hpp"
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

void run_experiment(std::span<const Leg> route, const Config& config,
                    Experiment experiment, const TrialSink& sink, const TrialTrace& trace) {
    if (route.empty() || experiment.trials == 0 || !sink) {
        throw std::invalid_argument("an experiment requires a route, at least one trial and a result sink");
    }
    if (trace.sink && (trace.trial == 0 || trace.trial > experiment.trials)) {
        throw std::invalid_argument("traced trial must be between 1 and the number of trials");
    }

    if (!(experiment.rain_multiplier >= 0.0 && experiment.rain_multiplier <= 8.0)) {
        throw std::invalid_argument("rain exposure multiplier must be from 0 to 8");
    }
    const auto targets = detail::prepare_race(route, config);
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
        for (const auto policy : policies) {
            LegSink leg_sink;
            if (trace.sink && trace.trial == trial + 1) {
                leg_sink = [&](const LegObservation& leg) { trace.sink(policy, leg); };
            }
            sink({trial + 1, policy, detail::simulate_prepared(route, weather, config, policy, targets, leg_sink)});
        }
    }
}

}  // namespace ecm::lab
