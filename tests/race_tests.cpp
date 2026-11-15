#include "solar_race/race.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace solar_race;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void near(double actual, double expected, const char* message) {
    check(std::abs(actual - expected) < 1e-8, message);
}

int main() {
    const auto route = read_route(TEST_ROUTE);
    check(route.size() == 3 && route.back().day == 2 && route[1].destination == "Example roadhouse",
          "CSV loads contiguous days and names");

    std::mt19937_64 first(35);
    std::mt19937_64 second(35);
    const auto weather = draw_weather(route, first);
    const auto repeated = draw_weather(route, second);
    near(weather[1].solar_factor, repeated[1].solar_factor, "seeded weather is replayable");
    near(weather[0].park_solar_factor, weather[1].park_solar_factor, "consecutive legs share day solar state");

    const Config defaults;
    for (auto policy : policies) {
        const auto result = simulate(route, weather, defaults, policy);
        check(result.completed, "the small reference route is finishable");
        near(result.distance_km, 45.0, "distance on completion");
        near(result.finish_soc * defaults.capacity_wh,
             defaults.start_soc * defaults.capacity_wh + result.solar_wh - result.demand_wh - result.spilled_wh,
             "energy accounting includes parked solar and spill");
    }

    const std::array short_route{Leg{1, "Finish", 10.0, 0.0, 0.0, 40.0}};
    const std::array no_sun{Weather{0.0, 0.0, 1.0, 0.0}};
    Config flat;
    flat.vehicle = {200.0, 0.0, 0.0, 1.0, 0.0, 0.0, 100.0};
    flat.capacity_wh = 100.0;
    flat.start_soc = 0.125;
    const auto depleted = simulate(short_route, no_sun, flat, Policy::steady);
    check(!depleted.completed && depleted.reserve_breached, "depletion interrupts a leg and breaches reserve");
    near(depleted.distance_km, 5.0, "half leg on half the required energy");
    near(depleted.drive_hours, 0.125, "driving time at a hard pace cap");
    near(depleted.finish_soc, 0.0, "empty pack remains clamped");

    flat.start_soc = 0.25;
    const auto exactly_empty = simulate(short_route, no_sun, flat, Policy::push);
    check(exactly_empty.completed, "arrival with exactly zero charge counts as completion");
    near(exactly_empty.distance_km, 10.0, "entire leg");
}
