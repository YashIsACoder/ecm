#include <ecm/lab.hpp>
#include <ecm/lab_statistics.hpp>

#include <array>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    using namespace ecm::lab;
    try {
        if (argc > 2) throw std::invalid_argument("usage: bwsc-simulation [route.csv]");
        auto route = argc == 2 ? read_route(argv[1]) : default_route();
        auto config = vehicle_config("sr7");
        config.energy_model = route_energy_model("gust", 2.0);
        config.race_schedule = true;
        config.stage_timing = StageTiming::bwsc_2025;
        config.strategy.reserve_strategy = ReserveStrategy::energy_envelope;
        config.strategy.reserve_target_soc = .15;
        config.strategy.reserve_slow_kmh = 65;
        config.strategy.envelope_speed_samples = 17;

        // Replace these planning values with a forecast issued before the race.
        const std::vector outlook(route.size(), Weather{.75, 2.0, 1.0, .68});
        const PreparedRace race(std::move(route), config, outlook);
        constexpr Experiment experiment{.trials = 100, .seed = 2025,
                                        .weather = WeatherModel::posterior, .rain_multiplier = 1.0};

        struct Totals {
            std::uint64_t finishes = 0;
            std::uint64_t below_reserve = 0;
            std::uint64_t late_stage = 0;
        };
        std::array<Totals, policies.size()> totals{};
        PairedOutcomes finish_change, reserve_change;
        Result steady;
        run_experiment(race, experiment, [&](const TrialResult& trial) {
            auto& summary = totals[static_cast<std::size_t>(trial.policy)];
            summary.finishes += trial.result.completed;
            summary.below_reserve += trial.result.reserve_breached;
            summary.late_stage += trial.result.stage_penalty_hours > 0.0;
            if (trial.policy == Policy::steady) steady = trial.result;
            if (trial.policy == Policy::reserve_first) {
                finish_change.add(trial.result.completed, steady.completed);
                reserve_change.add(trial.result.reserve_breached, steady.reserve_breached);
            }
        });

        std::cout << std::fixed << std::setprecision(1);
        for (const Policy policy : policies) {
            const auto summary = totals[static_cast<std::size_t>(policy)];
            const auto finish = wilson95(summary.finishes, experiment.trials);
            const auto reserve = wilson95(summary.below_reserve, experiment.trials);
            std::cout << policy_name(policy) << " finishes " << finish.estimate * 100 << "% ("
                      << finish.low * 100 << "% to " << finish.high * 100 << "%), below reserve "
                      << reserve.estimate * 100 << "%, stage penalty in " << summary.late_stage
                      << '/' << experiment.trials << " draws\n";
        }
        const auto print_change = [](const char* name, const PairedOutcomes& paired) {
            const auto change = paired.difference95();
            std::cout << name << ' ' << change.estimate * 100 << " pp ("
                      << change.low * 100 << " to " << change.high * 100 << " pp)\n";
        };
        print_change("Reserve-first minus steady finish rate", finish_change);
        print_change("Reserve-first minus steady reserve-breach rate", reserve_change);
    } catch (const std::exception& error) {
        std::cerr << "bwsc-simulation: " << error.what() << '\n';
        return 1;
    }
}
