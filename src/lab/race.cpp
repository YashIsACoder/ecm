#include "race_plan.hpp"

#include "ecm/bwsc.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ecm::lab {
namespace {

struct PacedLeg {
    double speed_kmh;
    ecm_cont::RouteEnergy energy;
};

ecm_cont::RouteEnergy leg_energy(const Config& config, const Leg& leg, const Weather& weather,
                                 double entry_speed_kmh, double cruise_speed_kmh, bool end_of_day) {
    auto vehicle = config.vehicle;
    vehicle.Crr *= weather.rolling_multiplier;
    const ecm_cont::RouteInput input{{
        leg.distance_km * 1000.0, cruise_speed_kmh / 3.6, entry_speed_kmh / 3.6,
        end_of_day ? 0.0 : cruise_speed_kmh / 3.6, leg.grade_pct / 100.0, weather.headwind_mps,
    }, leg.clear_sky_w_m2 * weather.solar_factor};
    const auto energy = config.energy_model(input, vehicle, config.drivetrain, config.array);
    if (!std::isfinite(energy.seconds) || energy.seconds <= 0.0 || !std::isfinite(energy.net_wh()) ||
        !std::isfinite(energy.solar_wh) || energy.solar_wh < 0.0) {
        throw std::runtime_error("ECM returned invalid route energy for " + leg.destination);
    }
    return energy;
}

bool finishes_stage(std::span<const Leg> route, std::size_t index) {
    return index + 1 < route.size() && route[index].day != route[index + 1].day;
}

bool control_stop(const Leg& leg, const Config& config, bool final_leg) {
    return config.race_schedule && !final_leg && leg.control_minutes > 0.0;
}

Weather adjusted_weather(Weather weather, const WeatherAdjustments& adjustment) {
    if (adjustment.solar_multiplier != 1.0) {
        weather.solar_factor = std::min(weather.solar_factor * adjustment.solar_multiplier, 1.0);
        weather.park_solar_factor = std::min(weather.park_solar_factor * adjustment.solar_multiplier, 1.0);
    }
    if (!std::isfinite(weather.rain_mm_h) || weather.rain_mm_h < 0.0 ||
        !(weather.road_wetness >= 0.0 && weather.road_wetness <= 1.0)) {
        throw std::invalid_argument("rain must be nonnegative and road wetness must be a fraction");
    }
    weather.headwind_mps += adjustment.headwind_offset_mps;
    const double rain_transmission = 1.0 - .35 * (weather.rain_mm_h / (5.0 + weather.rain_mm_h));
    weather.solar_factor *= rain_transmission;
    weather.park_solar_factor *= rain_transmission;
    weather.rolling_multiplier *= 1.0 + .14 * weather.road_wetness;
    return weather;
}

double parked_generation(const Config& config, const Weather& weather) {
    // A one-hour equivalent irradiance gives the prescribed Wh/m² exposure.
    return ecm::solar_energy_wh(config.array, config.parked_clear_sky_wh_m2 * weather.park_solar_factor, 3600.0);
}

struct PaceGrid {
    double high;
    double low;
    std::size_t count;

    [[nodiscard]] double operator[](std::size_t index) const {
        return std::lerp(high, low, static_cast<double>(index) / (count - 1));
    }
};

PaceGrid pace_grid(const StrategyConfig& strategy, double cap) {
    const double high = std::min(strategy.reserve_fast_kmh, cap);
    return {high, std::min(strategy.reserve_slow_kmh, high), strategy.envelope_speed_samples};
}

double wet_pace_cap(const Leg& leg, const Weather& weather) {
    return leg.pace_cap_kmh * (1.0 - .14 * weather.road_wetness);
}

PacedLeg pace_leg(const Config& config, const Leg& leg, const Weather& weather,
                  Policy policy, double stored_wh, double entry_kmh, bool end_of_day, double pace_cap,
                  std::optional<double> arrival_target) {
    const auto evaluate = [&](double target_kmh) {
        const double speed = std::min(target_kmh, pace_cap);
        return PacedLeg{speed, leg_energy(config, leg, weather, entry_kmh, speed, end_of_day)};
    };
    const auto& strategy = config.strategy;
    switch (policy) {
    case Policy::steady:
        return evaluate(strategy.steady_kmh);
    case Policy::push:
        return evaluate(strategy.push_kmh);
    case Policy::reserve_first: {
        if (arrival_target) {
            const auto speeds = pace_grid(strategy, pace_cap);
            auto best = evaluate(speeds.high);
            if (speeds.high == speeds.low ||
                std::min(config.capacity_wh, stored_wh - best.energy.net_wh()) >= *arrival_target) return best;
            for (std::size_t i = 1; i < speeds.count; ++i) {
                const auto candidate = evaluate(speeds[i]);
                if (std::min(config.capacity_wh, stored_wh - candidate.energy.net_wh()) >= *arrival_target) {
                    return candidate;
                }
                if (candidate.energy.net_wh() < best.energy.net_wh()) best = candidate;
            }
            return best;
        }

        const double target_wh = config.capacity_wh * std::max(config.reserve_soc, strategy.reserve_target_soc);
        const auto fast = evaluate(strategy.reserve_fast_kmh);
        if (fast.speed_kmh <= strategy.reserve_slow_kmh || stored_wh - fast.energy.net_wh() >= target_wh) return fast;
        if (fast.speed_kmh > strategy.reserve_medium_kmh) {
            const auto medium = evaluate(strategy.reserve_medium_kmh);
            if (stored_wh - medium.energy.net_wh() >= target_wh) return medium;
        }
        return evaluate(strategy.reserve_slow_kmh);
    }
    }
    throw std::invalid_argument("unknown policy");
}

bool fraction(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

}  // namespace

std::string_view policy_name(Policy policy) {
    switch (policy) {
    case Policy::steady: return "steady";
    case Policy::push: return "push";
    case Policy::reserve_first: return "reserve-first";
    }
    throw std::invalid_argument("unknown policy");
}

std::vector<Weather> draw_weather(std::span<const Leg> route, std::mt19937_64& random) {
    std::normal_distribution<double> normal;
    std::vector<Weather> result;
    result.reserve(route.size());
    double day_solar = 0.0;
    double day_wind = 0.0;
    int previous_day = -1;

    for (const auto& leg : route) {
        if (leg.day != previous_day) {
            day_solar = std::clamp(0.83 + 0.19 * normal(random), 0.30, 1.0);
            day_wind = 0.6 + 2.4 * normal(random);
            previous_day = leg.day;
        }
        result.push_back({
            std::clamp(day_solar + 0.055 * normal(random), 0.25, 1.0),
            day_wind + 1.1 * normal(random),
            std::clamp(1.0 + 0.075 * normal(random), 0.78, 1.24),
            day_solar,
        });
    }
    return result;
}

std::vector<double> detail::prepare_race(std::span<const Leg> route, const Config& config, bool plan_reserve,
                                        std::span<const Weather> planning_weather) {
    if (route.empty() || !std::isfinite(config.capacity_wh) || config.capacity_wh <= 0.0 ||
        !fraction(config.start_soc) || !fraction(config.reserve_soc) || !config.energy_model ||
        !std::isfinite(config.parked_clear_sky_wh_m2) || config.parked_clear_sky_wh_m2 < 0.0) {
        throw std::invalid_argument("invalid route or storage configuration");
    }
    const auto& strategy = config.strategy;
    const auto valid_speed = [](double speed) { return speed >= 20.0 && speed <= 140.0; };
    if (!valid_speed(strategy.steady_kmh) || !valid_speed(strategy.push_kmh) ||
        !valid_speed(strategy.reserve_fast_kmh) || !valid_speed(strategy.reserve_medium_kmh) ||
        !valid_speed(strategy.reserve_slow_kmh) || strategy.reserve_fast_kmh < strategy.reserve_slow_kmh ||
        !fraction(strategy.reserve_target_soc) ||
        (strategy.reserve_strategy == ReserveStrategy::tiers &&
         (strategy.reserve_fast_kmh < strategy.reserve_medium_kmh || strategy.reserve_medium_kmh < strategy.reserve_slow_kmh))) {
        throw std::invalid_argument("strategy needs speeds from 20 to 140 km/h, ordered reserve tiers and a valid target");
    }
    const auto& adjustments = config.weather_adjustments;
    if (!(adjustments.solar_multiplier >= 0.0 && adjustments.solar_multiplier <= 2.0) ||
        !(std::abs(adjustments.headwind_offset_mps) <= 25.0)) {
        throw std::invalid_argument("weather needs a solar multiplier from 0 to 2 and wind offset within ±25 m/s");
    }
    if (config.stage_timing != StageTiming::fixed &&
        (config.stage_timing != StageTiming::bwsc_2025 || !config.race_schedule)) {
        throw std::invalid_argument("BWSC stage timing requires a scheduled race");
    }
    if (config.race_schedule && std::ranges::any_of(route, [](const Leg& leg) {
        return !std::isfinite(leg.control_minutes) || leg.control_minutes < 0.0;
    })) {
        throw std::invalid_argument("invalid control stop duration");
    }
    if (strategy.reserve_strategy == ReserveStrategy::tiers) return {};
    if (strategy.reserve_strategy != ReserveStrategy::energy_envelope) {
        throw std::invalid_argument("unknown reserve strategy");
    }

    if (strategy.envelope_speed_samples < 2 || strategy.envelope_speed_samples > 65) {
        throw std::invalid_argument("envelope speed samples must be from 2 to 65");
    }
    if (!plan_reserve) return {};
    if (!planning_weather.empty() && planning_weather.size() != route.size()) {
        throw std::invalid_argument("planning weather must cover every leg");
    }
    const auto forecast_at = [&](std::size_t index) {
        constexpr Weather fallback{.70, 3.0, 1.0, .65};
        return adjusted_weather(planning_weather.empty() ? fallback : planning_weather[index], adjustments);
    };
    const double floor = config.capacity_wh * std::max(config.reserve_soc, strategy.reserve_target_soc);
    std::vector<double> arrival_targets(route.size());
    double required_departure = floor;
    for (std::size_t index = route.size(); index-- > 0;) {
        const auto& leg = route[index];
        const bool final = index + 1 == route.size();
        const bool overnight = finishes_stage(route, index);
        const double charge = overnight && required_departure <= config.capacity_wh
            ? parked_generation(config, forecast_at(index + 1)) : 0.0;
        arrival_targets[index] = std::max(floor, required_departure - charge);
        // Entry speed at an ordinary waypoint is approximated by the candidate cruise speed.
        const bool standing = index == 0 || finishes_stage(route, index - 1) ||
                              control_stop(route[index - 1], config, false);
        const bool stop = final || overnight || control_stop(leg, config, false);
        const auto forecast = forecast_at(index);
        const auto speeds = pace_grid(strategy, wet_pace_cap(leg, forecast));
        const auto net_draw = [&](double speed) {
            return leg_energy(config, leg, forecast, standing ? 0.0 : speed, speed, stop).net_wh();
        };
        // The first leg has no earlier waypoint requiring its departure target.
        if (index == 0) break;
        double minimum = net_draw(speeds.high);
        if (speeds.high != speeds.low) {
            for (std::size_t i = 1; i < speeds.count; ++i) minimum = std::min(minimum, net_draw(speeds[i]));
        }
        required_departure = std::max(floor, arrival_targets[index] + minimum);
        if (!std::isfinite(required_departure)) throw std::overflow_error("route energy envelope exceeded numeric range");
    }
    return arrival_targets;
}

PreparedRace::PreparedRace(std::vector<Leg> route, Config config, std::span<const Weather> planning_weather)
    : route_(std::move(route)), config_(std::move(config)),
      targets_(detail::prepare_race(route_, config_, true, planning_weather)) {}

Result PreparedRace::simulate(std::span<const Weather> weather, Policy policy, const LegSink& trace) const {
    return detail::simulate_prepared(route_, weather, config_, policy, targets_, trace);
}

Result simulate(std::span<const Leg> route, std::span<const Weather> weather, const Config& config,
                Policy policy, const LegSink& trace) {
    const auto targets = detail::prepare_race(route, config, policy == Policy::reserve_first);
    return detail::simulate_prepared(route, weather, config, policy, targets, trace);
}

Result detail::simulate_prepared(std::span<const Leg> route, std::span<const Weather> weather,
                                 const Config& config, Policy policy, std::span<const double> arrival_targets,
                                 const LegSink& trace) {
    if (route.size() != weather.size()) throw std::invalid_argument("weather must cover every leg");

    Result result;
    double stored_wh = config.capacity_wh * config.start_soc;
    double min_wh = stored_wh;
    double entry_kmh = 0.0;
    const auto store = [&](double delta_wh) {
        const double next_wh = stored_wh + delta_wh;
        result.spilled_wh += std::max(0.0, next_wh - config.capacity_wh);
        stored_wh = std::clamp(next_wh, 0.0, config.capacity_wh);
        min_wh = std::min(min_wh, stored_wh);
    };

    for (std::size_t index = 0; index < route.size(); ++index) {
        const auto& leg = route[index];
        const auto conditions = adjusted_weather(weather[index], config.weather_adjustments);
        const bool new_day = index > 0 && leg.day != route[index - 1].day;
        const bool final_leg = index + 1 == route.size();
        const bool overnight = finishes_stage(route, index);
        const bool hold = control_stop(leg, config, final_leg);
        const bool stop_at_end = final_leg || overnight || hold;
        const double spilled_before = result.spilled_wh;
        double parked_wh = 0.0;
        if (new_day) {
            parked_wh = parked_generation(config, conditions);
            result.solar_wh += parked_wh;
            store(parked_wh);
        }
        const double depart_wh = stored_wh;
        const double race_depart = result.race_hours;
        const auto target = policy == Policy::reserve_first && !arrival_targets.empty()
            ? std::optional{arrival_targets[index]} : std::nullopt;
        const double pace_cap = wet_pace_cap(leg, conditions);
        const auto [speed, energy] =
            pace_leg(config, leg, conditions, policy, stored_wh, entry_kmh, stop_at_end, pace_cap, target);
        const bool depleted = energy.net_wh() > stored_wh;
        // With constant average net draw within a leg, range until depletion is linear.
        const double portion = depleted ? stored_wh / energy.net_wh() : 1.0;
        result.distance_km += leg.distance_km * portion;
        const double driving = energy.seconds * portion / 3600.0;
        result.drive_hours += driving;
        const double race_arrive = race_depart + driving;
        const double control = hold && !depleted ? leg.control_minutes / 60.0 : 0.0;
        const double race_after_control = race_arrive + control;
        double penalty = 0.0;
        double off_duty = 0.0;
        if (config.race_schedule && overnight && !depleted) {
            if (config.stage_timing == StageTiming::bwsc_2025) {
                penalty = bwsc_2025_stage_timing(leg.day, race_arrive).penalty_hours;
            }
            const double next_start = 24.0 * (route[index + 1].day - 1) + penalty;
            off_duty = std::max(0.0, next_start - race_after_control);
        }
        result.stage_penalty_hours += penalty;
        result.race_hours = race_after_control + off_duty;
        result.demand_wh += energy.demand_wh * portion;
        result.solar_wh += energy.solar_wh * portion;
        store(-energy.net_wh() * portion);
        if (trace) {
            auto components = energy.components;
            if (components) {
                auto& parts = *components;
                parts = {parts.drag_wh * portion, parts.rolling_wh * portion, parts.climb_wh * portion,
                         parts.kinetic_wh * portion, parts.traction_wh * portion, parts.auxiliary_wh * portion,
                         parts.recovered_wh * portion};
            }
            trace({index, conditions, result.distance_km, speed, stored_wh / config.capacity_wh,
                   energy.solar_wh * portion, energy.demand_wh * portion, parked_wh, depleted,
                   result.drive_hours, depart_wh / config.capacity_wh, result.spilled_wh - spilled_before,
                   leg.clear_sky_w_m2 * conditions.solar_factor, components,
                   race_depart, race_arrive, control, off_duty,
                   portion > 0.0 ? leg.distance_km / (energy.seconds / 3600.0) : 0.0,
                   target ? std::optional{*target / config.capacity_wh} : std::nullopt, pace_cap, penalty});
        }
        if (depleted) {
            result.completed = false;
            break;
        }
        entry_kmh = stop_at_end ? 0.0 : speed;
    }

    result.finish_soc = stored_wh / config.capacity_wh;
    result.min_soc = min_wh / config.capacity_wh;
    result.reserve_breached = result.min_soc < config.reserve_soc;
    return result;
}

}  // namespace ecm::lab
