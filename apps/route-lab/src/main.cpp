#include "ecm/lab.hpp"
#include "ecm/forecast.hpp"

#include <algorithm>
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
        << "--race-schedule models control holds and scheduled overnight stops.\n"
        << "--weather baseline|posterior selects the weather model.\n"
        << "--forecast-report emits calibration diagnostics as JSON and exits.\n"
        << "Defaults: built-in outback route, 2000 trials, seed 2025, reference vehicle and segment model.\n"
        << "--pack-kwh overrides the selected vehicle's usable capacity.\n";
}

void forecast_report(std::uint64_t seed) {
    using namespace ecm::lab;
    const auto residuals = synthetic_weather_residuals();
    const auto result = calibrate_forecast(residuals, seed);
    auto& out = std::cout;
    out << std::fixed << std::setprecision(6) << "{\"seed\":\"" << seed
        << "\",\"observations\":" << residuals.size() << ",\"features\":{";
    const auto feature = [&](const char* key, double WeatherBias::*member, NormalPosterior exact) {
        double sum = 0.0;
        double squared = 0.0;
        std::size_t count = 0;
        for (const auto& chain : result.chains) {
            for (const auto sample : chain.samples) {
                sum += sample.*member;
                squared += (sample.*member) * (sample.*member);
                ++count;
            }
        }
        const double mean = sum / count;
        out << '"' << key << "\":{\"exact_mean\":" << exact.mean << ",\"exact_sd\":" << exact.sd
            << ",\"sample_mean\":" << mean << ",\"sample_sd\":" << std::sqrt(std::max(0.0, squared / count - mean * mean))
            << ",\"split_rhat\":" << result.split_rhat.*member << ",\"chains\":[";
        for (std::size_t c = 0; c < result.chains.size(); ++c) {
            if (c) out << ',';
            const auto& chain = result.chains[c];
            out << "{\"acceptance\":" << chain.acceptance << ",\"samples\":[";
            for (std::size_t i = 0; i < chain.samples.size(); i += 8) {
                if (i) out << ',';
                out << chain.samples[i].*member;
            }
            out << "]}";
        }
        out << "]}";
    };
    feature("solar", &WeatherBias::solar, result.solar_exact);
    out << ',';
    feature("wind", &WeatherBias::wind_mps, result.wind_exact);
    out << "},\"retained_per_chain\":" << result.chains.front().samples.size() << ",\"plot_stride\":8}\n";
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
        bool race_schedule = false;
        bool describe_forecast = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--help" || option == "-h") {
                help(std::cout);
                return 0;
            }
            if (option == "--race-schedule") {
                race_schedule = true;
                continue;
            }
            if (option == "--forecast-report") {
                describe_forecast = true;
                continue;
            }
            if (option != "--route" && option != "--trials" && option != "--seed" && option != "--pack-kwh" &&
                option != "--vehicle" && option != "--model" && option != "--trace-trial" && option != "--weather") {
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
            } else if (option == "--weather") {
                if (value == "posterior") experiment.weather = ecm::lab::WeatherModel::posterior;
                else if (value == "baseline") experiment.weather = ecm::lab::WeatherModel::baseline;
                else throw std::invalid_argument("weather must be baseline or posterior");
            } else {
                experiment.seed = unsigned_value(value);
            }
        }
        if (describe_forecast) {
            forecast_report(experiment.seed);
            return 0;
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
        config.race_schedule = race_schedule;
        const auto route = route_path ? ecm::lab::read_route(*route_path) : ecm::lab::default_route();
        ecm::lab::TrialTrace trace;
        if (traced_trial) {
            std::cerr << "policy,leg,distance_km,speed_kmh,soc_pct,solar_factor,headwind_mps,rolling_factor,"
                         "solar_wh,demand_wh,parked_wh,depleted,elapsed_hours,depart_soc_pct,spilled_wh,"
                         "irradiance_w_m2,drag_wh,rolling_wh,climb_wh,kinetic_wh,traction_wh,auxiliary_wh,"
                         "recovered_wh,race_depart_hours,race_arrive_hours,control_hours,overnight_hours\n"
                      << std::fixed << std::setprecision(6);
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
                std::cerr << ',' << leg.race_depart_hours << ',' << leg.race_arrive_hours << ',' << leg.control_hours
                    << ',' << leg.overnight_hours << '\n';
            }};
        }
        std::cout << "trial,policy,completed,reserve_breached,distance_km,drive_hours,finish_soc_pct,min_soc_pct,"
                     "demand_kwh,solar_kwh,spilled_kwh"
                  << (race_schedule ? ",race_hours\n" : "\n") << std::fixed << std::setprecision(6);
        ecm::lab::run_experiment(route, config, experiment, [race_schedule](const ecm::lab::TrialResult& trial) {
            const auto& result = trial.result;
            std::cout << trial.trial << ',' << ecm::lab::policy_name(trial.policy) << ',' << result.completed << ','
                << result.reserve_breached << ',' << result.distance_km << ',' << result.drive_hours << ','
                << 100.0 * result.finish_soc << ',' << 100.0 * result.min_soc << ',' << result.demand_wh / 1000.0
                << ',' << result.solar_wh / 1000.0 << ',' << result.spilled_wh / 1000.0;
            if (race_schedule) std::cout << ',' << result.race_hours;
            std::cout << '\n';
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
