#include "ecm/lab.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ecm::lab {
namespace {

std::vector<std::string> fields(std::string_view line) {
    std::vector<std::string> values;
    std::size_t cursor = 0;
    while (true) {
        std::string value;
        if (cursor < line.size() && line[cursor] == '"') {
            ++cursor;
            while (true) {
                const auto quote = line.find('"', cursor);
                if (quote == std::string_view::npos) {
                    throw std::runtime_error("unterminated quoted CSV field");
                }
                value.append(line.substr(cursor, quote - cursor));
                cursor = quote + 1;
                if (cursor == line.size() || line[cursor] != '"') {
                    break;
                }
                value.push_back('"');
                ++cursor;
            }
            if (cursor < line.size() && line[cursor] != ',') {
                throw std::runtime_error("expected a comma after a quoted CSV field");
            }
        } else {
            const auto end = std::min(line.find(',', cursor), line.size());
            const auto field = line.substr(cursor, end - cursor);
            if (field.find('"') != std::string_view::npos) {
                throw std::runtime_error("unexpected quote in unquoted CSV field");
            }
            value = field;
            cursor = end;
        }
        values.push_back(std::move(value));
        if (cursor == line.size()) {
            return values;
        }
        ++cursor;
    }
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
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open route " + path.string());
    }
    return read_route(input, path.string());
}

std::vector<Leg> read_route(std::istream& input, std::string_view source_name) {
    const std::string name(source_name);
    std::string line;
    constexpr std::array<std::string_view, 6> columns{
        "day", "destination", "distance_km", "grade_pct", "clear_sky_w_m2", "pace_cap_kmh",
    };
    bool timed_stops = false;
    bool rain_prior = false;
    try {
        std::getline(input, line);
        trim_cr(line);
        std::string_view header(line);
        constexpr std::string_view utf8_bom = "\xEF\xBB\xBF";
        if (header.starts_with(utf8_bom)) {
            header.remove_prefix(utf8_bom.size());
        }
        auto names = fields(header);
        if (names.size() > columns.size() && names.back() == "rain_day_probability") {
            rain_prior = true;
            names.pop_back();
        }
        if (names.size() == columns.size() + 1 && names.back() == "control_minutes") {
            timed_stops = true;
            names.pop_back();
        }
        if (!std::ranges::equal(names, columns)) {
            throw std::runtime_error("unexpected route CSV header");
        }
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(name + ":1: " + error.what());
    }

    std::vector<Leg> route;
    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        trim_cr(line);
        if (line.empty()) {
            continue;
        }
        try {
            auto values = fields(line);
            if (values.size() != columns.size() + timed_stops + rain_prior || values[1].empty()) {
                throw std::runtime_error("route row does not match the header or has an empty destination");
            }
            const double day = number(values[0]);
            const int previous_day = route.empty() ? 0 : route.back().day;
            if ((route.empty() && day != 1.0) || day < previous_day || day > previous_day + 1.0 ||
                day > std::numeric_limits<int>::max() ||
                std::trunc(day) != day) {
                throw std::runtime_error("days must start at 1 and be consecutive");
            }
            Leg leg{static_cast<int>(day), std::move(values[1]), number(values[2]), number(values[3]),
                    number(values[4]), number(values[5]), timed_stops ? number(values[6]) : 0.0};
            if (rain_prior) leg.rain_day_probability = number(values.back());
            if (leg.rain_day_probability < 0.0 || leg.rain_day_probability > 1.0) {
                throw std::runtime_error("wet-day probability must be from 0 to 1");
            }
            if (leg.control_minutes < 0.0) {
                throw std::runtime_error("control stop minutes must be nonnegative");
            }
            if (leg.distance_km <= 0.0 || std::abs(leg.grade_pct) > 100.0 ||
                leg.clear_sky_w_m2 < 0.0 || leg.pace_cap_kmh <= 0.0) {
                throw std::runtime_error("distance and pace cap must be positive; grade and irradiance must be valid");
            }
            route.push_back(std::move(leg));
        } catch (const std::exception& error) {
            throw std::runtime_error(name + ':' + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (route.empty() || input.bad()) {
        throw std::runtime_error("route is empty or unreadable: " + name);
    }
    return route;
}

}  // namespace ecm::lab
