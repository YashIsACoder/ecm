#include "solar_race/race.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
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
    output << "Usage: solar-race [--route PATH] [--trials N] [--seed N]\n"
        << "Writes one CSV row per trial and pacing policy to stdout.\n"
        << "Defaults: data/outback-2025-style.csv, 2000 trials, seed 2025.\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path route_path = "data/outback-2025-style.csv";
        std::uint64_t trials = 2000;
        std::uint64_t seed = 2025;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--help" || option == "-h") {
                help(std::cout);
                return 0;
            }
            if (option != "--route" && option != "--trials" && option != "--seed") {
                throw std::invalid_argument("unknown option " + std::string(option));
            }
            if (++i == argc) {
                throw std::invalid_argument("missing value for " + std::string(option));
            }
            const std::string_view value = argv[i];
            if (option == "--route") {
                route_path = value;
            } else if (option == "--trials") {
                trials = unsigned_value(value);
            } else {
                seed = unsigned_value(value);
            }
        }
        if (trials == 0) {
            throw std::invalid_argument("at least one trial is required");
        }

        const auto route = solar_race::read_route(route_path);
        const solar_race::Config config;
        std::mt19937_64 random(seed);
        std::cout << "trial,policy,completed,reserve_breached,distance_km,drive_hours,finish_soc_pct,min_soc_pct,"
                     "demand_kwh,solar_kwh,spilled_kwh\n" << std::fixed << std::setprecision(6);
        for (std::uint64_t trial = 0; trial < trials; ++trial) {
            const auto weather = solar_race::draw_weather(route, random);
            for (const auto policy : solar_race::policies) {
                const auto result = solar_race::simulate(route, weather, config, policy);
                std::cout << trial + 1 << ',' << solar_race::policy_name(policy) << ',' << result.completed << ','
                    << result.reserve_breached << ',' << result.distance_km << ',' << result.drive_hours << ','
                    << 100.0 * result.finish_soc << ',' << 100.0 * result.min_soc << ',' << result.demand_wh / 1000.0
                    << ',' << result.solar_wh / 1000.0 << ',' << result.spilled_wh / 1000.0 << '\n';
            }
        }
        if (!std::cout.flush()) {
            throw std::runtime_error("failed to write trial results");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "solar-race: " << error.what() << '\n';
        return 1;
    }
}
