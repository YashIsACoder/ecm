#include "ecm/lab.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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

PacedLeg pace_leg(const Config& config, const Leg& leg, const Weather& weather,
                  Policy policy, double stored_wh, double entry_kmh, bool end_of_day) {
    const auto evaluate = [&](double target_kmh) {
        const double speed = std::min(target_kmh, leg.pace_cap_kmh);
        return PacedLeg{speed, leg_energy(config, leg, weather, entry_kmh, speed, end_of_day)};
    };
    switch (policy) {
    case Policy::steady:
        return evaluate(82.0);
    case Policy::push:
        return evaluate(92.0);
    case Policy::reserve_first: {
        // A buffer above the reporting reserve pays for later legs whose weather is unknown.
        const double target_wh = config.capacity_wh * std::max(config.reserve_soc, 0.35);
        const auto fast = evaluate(92.0);
        if (fast.speed_kmh <= 75.0 || stored_wh - fast.energy.net_wh() >= target_wh) {
            return fast;
        }
        if (fast.speed_kmh > 85.0) {
            const auto medium = evaluate(85.0);
            if (stored_wh - medium.energy.net_wh() >= target_wh) {
                return medium;
            }
        }
        return evaluate(75.0);
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

Result simulate(std::span<const Leg> route, std::span<const Weather> weather, const Config& config,
                Policy policy) {
    if (route.empty() || route.size() != weather.size() || !std::isfinite(config.capacity_wh) ||
        config.capacity_wh <= 0.0 || !fraction(config.start_soc) || !fraction(config.reserve_soc) || !config.energy_model ||
        !std::isfinite(config.parked_clear_sky_wh_m2) || config.parked_clear_sky_wh_m2 < 0.0) {
        throw std::invalid_argument("invalid route, weather length or storage configuration");
    }

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
        const auto& conditions = weather[index];
        const bool new_day = index > 0 && leg.day != route[index - 1].day;
        const bool end_of_day = index + 1 == route.size() || leg.day != route[index + 1].day;
        if (new_day) {
            // A one-hour equivalent irradiance gives the prescribed Wh/m^2 exposure.
            const double parked_wh = ecm::solar_energy_wh(
                config.array, config.parked_clear_sky_wh_m2 * conditions.park_solar_factor, 3600.0);
            result.solar_wh += parked_wh;
            store(parked_wh);
        }
        const auto [speed, energy] = pace_leg(config, leg, conditions, policy, stored_wh, entry_kmh, end_of_day);
        const bool depleted = energy.net_wh() > stored_wh;
        // With constant average net draw within a leg, range until depletion is linear.
        const double portion = depleted ? stored_wh / energy.net_wh() : 1.0;
        result.distance_km += leg.distance_km * portion;
        result.drive_hours += energy.seconds * portion / 3600.0;
        result.demand_wh += energy.demand_wh * portion;
        result.solar_wh += energy.solar_wh * portion;
        store(-energy.net_wh() * portion);
        if (depleted) {
            result.completed = false;
            break;
        }
        entry_kmh = end_of_day ? 0.0 : speed;
    }

    result.finish_soc = stored_wh / config.capacity_wh;
    result.min_soc = min_wh / config.capacity_wh;
    result.reserve_breached = result.min_soc < config.reserve_soc;
    return result;
}

}  // namespace ecm::lab
