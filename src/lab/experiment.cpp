#include "ecm/lab.hpp"
#include "ecm/forecast.hpp"
#include "default_route.hpp"

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

    std::mt19937_64 random(experiment.seed);
    std::optional<ForecastCalibration> calibration;
    if (experiment.weather == WeatherModel::posterior) {
        calibration = calibrate_forecast(synthetic_weather_residuals(), experiment.seed);
    }
    for (std::uint64_t trial = 0; trial < experiment.trials; ++trial) {
        const auto weather = calibration ? draw_forecast_weather(route, *calibration, random)
                                         : draw_weather(route, random);
        for (const auto policy : policies) {
            if (trace.sink && trace.trial == trial + 1) {
                sink({trial + 1, policy, simulate(route, weather, config, policy, [&](const LegObservation& leg) {
                    trace.sink(policy, leg);
                })});
            } else {
                sink({trial + 1, policy, simulate(route, weather, config, policy)});
            }
        }
    }
}

}  // namespace ecm::lab
