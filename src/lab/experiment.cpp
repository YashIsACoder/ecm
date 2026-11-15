#include "ecm/lab.hpp"
#include "default_route.hpp"

#include <sstream>
#include <stdexcept>

namespace ecm::lab {

std::vector<Leg> default_route() {
    std::istringstream csv(default_route_csv);
    return read_route(csv, "built-in outback route");
}

void run_experiment(std::span<const Leg> route, const Config& config,
                    Experiment experiment, const TrialSink& sink) {
    if (route.empty() || experiment.trials == 0 || !sink) {
        throw std::invalid_argument("an experiment requires a route, at least one trial and a result sink");
    }

    std::mt19937_64 random(experiment.seed);
    for (std::uint64_t trial = 0; trial < experiment.trials; ++trial) {
        const auto weather = draw_weather(route, random);
        for (const auto policy : policies) {
            sink({trial + 1, policy, simulate(route, weather, config, policy)});
        }
    }
}

}  // namespace ecm::lab
