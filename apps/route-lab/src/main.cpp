#include "ecm/lab.hpp"
#include "ecm/forecast.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
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
        << "                  [--vehicle reference|sr7|sr8] [--model segment|cycle|mapped|gust] [--trace-trial N]\n"
        << "Writes one CSV row per trial and pacing policy to stdout.\n"
        << "--trace-trial writes that draw's leg-by-leg CSV to stderr.\n"
        << "--race-schedule models control holds and scheduled overnight stops.\n"
        << "--bwsc-2025-timing adds next-day penalties after late planned stage arrivals.\n"
        << "--weather baseline|posterior selects the weather model.\n"
        << "--forecast-report emits calibration diagnostics as JSON and exits.\n"
        << "Defaults: built-in outback route, 2000 trials, seed 2025, reference vehicle and segment model.\n"
        << "Pacing: --steady-kmh N --push-kmh N --reserve-strategy tiers|envelope\n"
        << "        --reserve-fast-kmh N --reserve-medium-kmh N --reserve-slow-kmh N --reserve-target-pct N\n"
        << "Solar:  --array-m2 N --cell-efficiency-pct N --system-efficiency-pct N --parked-wh-m2 N\n"
        << "Weather stress test: --solar-multiplier N --headwind-offset-mps N --rain-multiplier N\n"
        << "--gust-sd-mps adjusts the longitudinal wind spread for the gust model (default 2).\n"
        << "--overnight-charge-w sets the Cruiser battery-side charging power (default 1500).\n"
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

double number(std::string_view input, std::string_view option, double minimum, double maximum) {
    const std::string value(input);
    std::size_t end = 0;
    const double parsed = std::stod(value, &end);
    if (end != value.size() || !(parsed >= minimum && parsed <= maximum)) {
        throw std::invalid_argument("invalid value for " + std::string(option));
    }
    return parsed;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::optional<std::filesystem::path> route_path;
        ecm::lab::Experiment experiment;
        std::string_view vehicle = "reference";
        std::string_view model = "segment";
        std::optional<double> capacity, array_area, cell_efficiency, system_efficiency, parked_exposure;
        std::optional<double> overnight_charge_power;
        double gust_sd_mps = 2.0;
        ecm::lab::StrategyConfig strategy;
        ecm::lab::WeatherAdjustments weather_adjustments;
        std::optional<std::uint64_t> traced_trial;
        bool race_schedule = false;
        bool bwsc_timing = false;
        bool describe_forecast = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--help" || option == "-h") {
                help(std::cout);
                return 0;
            }
            if (option == "--race-schedule" || option == "--bwsc-2025-timing") {
                race_schedule = true;
                bwsc_timing |= option == "--bwsc-2025-timing";
                continue;
            }
            if (option == "--forecast-report") {
                describe_forecast = true;
                continue;
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
                capacity = 1000.0 * number(value, option, 0, std::numeric_limits<double>::max());
                if (!std::isfinite(*capacity) || *capacity <= 0.0) {
                    throw std::invalid_argument("pack capacity must be a positive finite number of kWh");
                }
            } else if (option == "--vehicle") {
                vehicle = value;
            } else if (option == "--model") {
                model = value;
            } else if (option == "--overnight-charge-w") {
                overnight_charge_power = number(value, option, 0, 250000);
            } else if (option == "--gust-sd-mps") {
                gust_sd_mps = number(value, option, 0, 15);
            } else if (option == "--trace-trial") {
                traced_trial = unsigned_value(value);
            } else if (option == "--weather") {
                if (value == "posterior") experiment.weather = ecm::lab::WeatherModel::posterior;
                else if (value == "baseline") experiment.weather = ecm::lab::WeatherModel::baseline;
                else throw std::invalid_argument("weather must be baseline or posterior");
            } else if (option == "--seed") {
                experiment.seed = unsigned_value(value);
            } else if (option == "--reserve-strategy") {
                if (value == "envelope") strategy.reserve_strategy = ecm::lab::ReserveStrategy::energy_envelope;
                else if (value == "tiers") strategy.reserve_strategy = ecm::lab::ReserveStrategy::tiers;
                else throw std::invalid_argument("reserve strategy must be tiers or envelope");
            } else if (option == "--steady-kmh") {
                strategy.steady_kmh = number(value, option, 20, 140);
            } else if (option == "--push-kmh") {
                strategy.push_kmh = number(value, option, 20, 140);
            } else if (option == "--reserve-fast-kmh") {
                strategy.reserve_fast_kmh = number(value, option, 20, 140);
            } else if (option == "--reserve-medium-kmh") {
                strategy.reserve_medium_kmh = number(value, option, 20, 140);
            } else if (option == "--reserve-slow-kmh") {
                strategy.reserve_slow_kmh = number(value, option, 20, 140);
            } else if (option == "--reserve-target-pct") {
                strategy.reserve_target_soc = number(value, option, 0, 100) / 100.0;
            } else if (option == "--array-m2") {
                array_area = number(value, option, 0, 30);
            } else if (option == "--cell-efficiency-pct") {
                cell_efficiency = number(value, option, .01, 100) / 100.0;
            } else if (option == "--system-efficiency-pct") {
                system_efficiency = number(value, option, .01, 100) / 100.0;
            } else if (option == "--parked-wh-m2") {
                parked_exposure = number(value, option, 0, 10000);
            } else if (option == "--solar-multiplier") {
                weather_adjustments.solar_multiplier = number(value, option, 0, 2);
            } else if (option == "--rain-multiplier") {
                experiment.rain_multiplier = number(value, option, 0, 8);
            } else if (option == "--headwind-offset-mps") {
                weather_adjustments.headwind_offset_mps = number(value, option, -25, 25);
            } else {
                throw std::invalid_argument("unknown option " + std::string(option));
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
        config.energy_model = ecm::lab::route_energy_model(model, gust_sd_mps);
        config.capacity_wh = capacity.value_or(config.capacity_wh);
        config.overnight_charge_power_w = overnight_charge_power.value_or(config.overnight_charge_power_w);
        config.race_schedule = race_schedule;
        if (bwsc_timing) config.stage_timing = ecm::lab::StageTiming::bwsc_2025;
        config.strategy = strategy;
        config.weather_adjustments = weather_adjustments;
        config.array.area_m2 = array_area.value_or(config.array.area_m2);
        config.array.cell_efficiency = cell_efficiency.value_or(config.array.cell_efficiency);
        config.array.system_efficiency = system_efficiency.value_or(config.array.system_efficiency);
        config.parked_clear_sky_wh_m2 = parked_exposure.value_or(config.parked_clear_sky_wh_m2);
        const bool cruiser = config.energy_rules == ecm::lab::EnergyRules::bwsc_2025_cruiser;
        const auto route = route_path ? ecm::lab::read_route(*route_path) : ecm::lab::default_route();
        ecm::lab::TrialTrace trace;
        if (traced_trial) {
            std::cerr << "policy,leg,distance_km,speed_kmh,soc_pct,solar_factor,headwind_mps,rolling_factor,"
                         "solar_wh,demand_wh,parked_wh,depleted,elapsed_hours,depart_soc_pct,spilled_wh,"
                         "irradiance_w_m2,drag_wh,rolling_wh,climb_wh,kinetic_wh,traction_wh,auxiliary_wh,"
                         "recovered_wh,race_depart_hours,race_arrive_hours,control_hours,overnight_hours,"
                         "mean_speed_kmh,planned_arrival_soc_pct,rain_mm_h,road_wetness,effective_pace_cap_kmh,rain_probability,stage_penalty_hours,external_charge_wh\n"
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
                    << ',' << leg.overnight_hours << ',' << leg.mean_speed_kmh << ',';
                if (leg.planned_arrival_soc) std::cerr << 100.0 * *leg.planned_arrival_soc;
                std::cerr << ',' << leg.weather.rain_mm_h << ',' << leg.weather.road_wetness
                    << ',' << leg.effective_pace_cap_kmh << ',' << leg.weather.rain_probability
                    << ',' << leg.stage_penalty_hours << ',' << leg.external_charge_wh << '\n';
            }};
        }
        std::cout << "trial,policy,completed,reserve_breached,distance_km,drive_hours,finish_soc_pct,min_soc_pct,"
                     "demand_kwh,solar_kwh,spilled_kwh"
                  << (race_schedule ? ",race_hours" : "")
                  << (bwsc_timing ? ",stage_penalty_hours" : "")
                  << (cruiser ? ",external_charge_kwh" : "") << '\n' << std::fixed << std::setprecision(6);
        ecm::lab::run_experiment(route, config, experiment, [race_schedule, bwsc_timing, cruiser](const ecm::lab::TrialResult& trial) {
            const auto& result = trial.result;
            std::cout << trial.trial << ',' << ecm::lab::policy_name(trial.policy) << ',' << result.completed << ','
                << result.reserve_breached << ',' << result.distance_km << ',' << result.drive_hours << ','
                << 100.0 * result.finish_soc << ',' << 100.0 * result.min_soc << ',' << result.demand_wh / 1000.0
                << ',' << result.solar_wh / 1000.0 << ',' << result.spilled_wh / 1000.0;
            if (race_schedule) std::cout << ',' << result.race_hours;
            if (bwsc_timing) std::cout << ',' << result.stage_penalty_hours;
            if (cruiser) std::cout << ',' << result.external_charge_wh / 1000.0;
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
