#pragma once

#include "solar_ecm/energy.hpp"

#include <array>
#include <filesystem>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace solar_race {

struct Leg {
    int day;
    std::string destination;
    double distance_km;
    double grade_pct;
    double clear_sky_w_m2;
    double pace_cap_kmh;
};

struct Weather {
    double solar_factor;
    double headwind_mps;
    double rolling_multiplier;
    double park_solar_factor;
};

struct Config {
    solar_ecm::Vehicle vehicle{225.0, 0.100, 0.0042, 0.92, 0.60, 1400.0, 65.0};
    solar_ecm::SolarArray array{6.0, 0.24, 0.92};
    double capacity_wh = 5500.0;
    double start_soc = 0.95;
    double reserve_soc = 0.15;
    double parked_clear_sky_wh_m2 = 2200.0;
};

enum class Policy { steady, push, reserve_first };
inline constexpr std::array policies{Policy::steady, Policy::push, Policy::reserve_first};
[[nodiscard]] std::string_view policy_name(Policy policy);

struct Result {
    bool completed = true;
    bool reserve_breached = false;
    double distance_km = 0.0;
    double drive_hours = 0.0;
    double finish_soc = 0.0;
    double min_soc = 0.0;
    double demand_wh = 0.0;
    double solar_wh = 0.0;
    double spilled_wh = 0.0;
};

[[nodiscard]] std::vector<Leg> read_route(const std::filesystem::path& path);
[[nodiscard]] std::vector<Weather> draw_weather(std::span<const Leg> route, std::mt19937_64& random);
[[nodiscard]] Result simulate(std::span<const Leg> route, std::span<const Weather> weather,
                              const Config& config, Policy policy);

}  // namespace solar_race
