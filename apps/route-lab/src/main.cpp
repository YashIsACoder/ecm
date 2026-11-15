#include "ecm/lab.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

std::uint64_t unsigned_value(std::string_view value) {
    std::uint64_t parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("expected a nonnegative integer");
    }
    return parsed;
}

void help(std::ostream& output) {
    output << "Usage: solar-race [--route PATH] [--trials N] [--seed N] [--pack-kwh KWH]\n"
        << "                  [--vehicle reference|sr7|sr8] [--model segment|cycle|mapped] [--trace-trial N]\n"
        << "Writes one CSV row per trial and pacing policy to stdout.\n"
        << "--trace-trial writes that draw's leg-by-leg CSV to stderr.\n"
        << "Defaults: built-in outback route, 2000 trials, seed 2025, reference vehicle and segment model.\n"
        << "--pack-kwh overrides the selected vehicle's usable capacity.\n";
}

double pack_wh(std::string_view input) {
    const std::string value(input);
    std::size_t end = 0;
    const double capacity_wh = 1000.0 * std::stod(value, &end);
    if (end != value.size() || !std::isfinite(capacity_wh) || capacity_wh <= 0.0) {
        throw std::invalid_argument("pack capacity must be a positive finite number of kWh");
    }
    return capacity_wh;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::optional<std::filesystem::path> route_path;
        ecm::lab::Experiment experiment;
        std::string_view vehicle = "reference";
        std::string_view model = "segment";
        std::optional<double> capacity;
        std::optional<std::uint64_t> traced_trial;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--help" || option == "-h") {
                help(std::cout);
                return 0;
            }
            if (option != "--route" && option != "--trials" && option != "--seed" && option != "--pack-kwh" &&
                option != "--vehicle" && option != "--model" && option != "--trace-trial") {
                throw std::invalid_argument("unknown option " + std::string(option));
            }
            if (++i == argc) {
                throw std::invalid_argument("missing value for " + std::string(option));
            }
            const std::string_view value = argv[i];
            if (option == "--route") {
                route_path = value;
            } else if (option == "--trials") {
                experiment.trials = unsigned_value(value);
            } else if (option == "--pack-kwh") {
                capacity = pack_wh(value);
            } else if (option == "--vehicle") {
                vehicle = value;
            } else if (option == "--model") {
                model = value;
            } else if (option == "--trace-trial") {
                traced_trial = unsigned_value(value);
            } else {
                experiment.seed = unsigned_value(value);
            }
        }
        if (experiment.trials == 0) {
            throw std::invalid_argument("at least one trial is required");
        }
        if (traced_trial && (*traced_trial == 0 || *traced_trial > experiment.trials)) {
            throw std::invalid_argument("traced trial must be between 1 and the number of trials");
        }

        auto config = ecm::lab::vehicle_config(vehicle);
        config.energy_model = ecm::lab::route_energy_model(model);
        config.capacity_wh = capacity.value_or(config.capacity_wh);
        const auto route = route_path ? ecm::lab::read_route(*route_path) : ecm::lab::default_route();
        ecm::lab::TrialTrace trace;
        if (traced_trial) {
            std::cerr << "policy,leg,distance_km,speed_kmh,soc_pct,solar_factor,headwind_mps,rolling_factor,"
                         "solar_wh,demand_wh,parked_wh,depleted,elapsed_hours,depart_soc_pct,spilled_wh,"
                         "irradiance_w_m2,drag_wh,rolling_wh,climb_wh,kinetic_wh,traction_wh,auxiliary_wh,"
                         "recovered_wh\n" << std::fixed << std::setprecision(6);
            trace = {*traced_trial, [](ecm::lab::Policy policy, const ecm::lab::LegObservation& leg) {
                std::cerr << ecm::lab::policy_name(policy) << ',' << leg.leg << ',' << leg.distance_km << ','
                    << leg.cruise_kmh << ',' << 100.0 * leg.arrive_soc << ',' << leg.weather.solar_factor << ','
                    << leg.weather.headwind_mps << ',' << leg.weather.rolling_multiplier << ',' << leg.solar_wh << ','
                    << leg.demand_wh << ',' << leg.parked_wh << ',' << leg.depleted << ',' << leg.elapsed_hours << ','
                    << 100.0 * leg.depart_soc << ',' << leg.spilled_wh << ',' << leg.irradiance_w_m2;
                if (leg.components) {
                    const auto& work = *leg.components;
                    std::cerr << ',' << work.drag_wh << ',' << work.rolling_wh << ',' << work.climb_wh << ','
                        << work.kinetic_wh << ',' << work.traction_wh << ',' << work.auxiliary_wh << ','
                        << work.recovered_wh;
                } else {
                    std::cerr << ",,,,,,,";
                }
                std::cerr << '\n';
            }};
        }
        std::cout << "trial,policy,completed,reserve_breached,distance_km,drive_hours,finish_soc_pct,min_soc_pct,"
                     "demand_kwh,solar_kwh,spilled_kwh\n" << std::fixed << std::setprecision(6);
        ecm::lab::run_experiment(route, config, experiment, [](const ecm::lab::TrialResult& trial) {
            const auto& result = trial.result;
            std::cout << trial.trial << ',' << ecm::lab::policy_name(trial.policy) << ',' << result.completed << ','
                << result.reserve_breached << ',' << result.distance_km << ',' << result.drive_hours << ','
                << 100.0 * result.finish_soc << ',' << 100.0 * result.min_soc << ',' << result.demand_wh / 1000.0
                << ',' << result.solar_wh / 1000.0 << ',' << result.spilled_wh / 1000.0 << '\n';
        }, trace);
        if (!std::cout.flush()) {
            throw std::runtime_error("failed to write trial results");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "solar-race: " << error.what() << '\n';
        return 1;
    }
}
