#include "solar_race/race.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace solar_race {
namespace {

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> values;
    std::istringstream input(line);
    for (std::string value; std::getline(input, value, ',');) {
        values.push_back(std::move(value));
    }
    if (line.ends_with(',')) {
        values.emplace_back();
    }
    return values;
}

double number(const std::string& value) {
    std::size_t end = 0;
    const double parsed = std::stod(value, &end);
    if (end != value.size() || !std::isfinite(parsed)) {
        throw std::runtime_error("expected a finite number");
    }
    return parsed;
}

void trim_cr(std::string& line) {
    if (line.ends_with('\r')) {
        line.pop_back();
    }
}

}  // namespace

std::vector<Leg> read_route(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open route " + path.string());
    }
    std::string line;
    std::getline(input, line);
    trim_cr(line);
    if (line != "day,destination,distance_km,grade_pct,clear_sky_w_m2,pace_cap_kmh") {
        throw std::runtime_error("unexpected route CSV header in " + path.string());
    }

    std::vector<Leg> route;
    int line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        trim_cr(line);
        if (line.empty()) {
            continue;
        }
        try {
            auto values = fields(line);
            if (values.size() != 6 || values[1].empty()) {
                throw std::runtime_error("expected six fields and a destination");
            }
            const double day = number(values[0]);
            const int previous_day = route.empty() ? 0 : route.back().day;
            if ((route.empty() && day != 1.0) || day < previous_day || day > previous_day + 1 ||
                std::trunc(day) != day) {
                throw std::runtime_error("days must start at 1 and be consecutive");
            }
            Leg leg{static_cast<int>(day), std::move(values[1]), number(values[2]), number(values[3]),
                    number(values[4]), number(values[5])};
            if (leg.distance_km <= 0.0 || std::abs(leg.grade_pct) > 100.0 ||
                leg.clear_sky_w_m2 < 0.0 || leg.pace_cap_kmh <= 0.0) {
                throw std::runtime_error("distance and pace cap must be positive; grade and irradiance must be valid");
            }
            route.push_back(std::move(leg));
        } catch (const std::exception& error) {
            throw std::runtime_error(path.string() + ':' + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (route.empty() || input.bad()) {
        throw std::runtime_error("route is empty or unreadable: " + path.string());
    }
    return route;
}

}  // namespace solar_race
