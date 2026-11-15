#include "ecm/lab.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace ecm::lab;

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
    check(route.size() == 3 && route.back().day == 2 && route[1].destination == "Example, \"roadhouse\"",
          "CSV decodes quoted destinations and contiguous days");

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
    flat.vehicle = {.Cd = 0.0, .m = 200.0, .Crr = 0.0, .X = 100.0};
    flat.drivetrain = {1.0, 0.0, 0.0};
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

    check(vehicle_config("sr7").vehicle.m == 500.0 && vehicle_config("sr8").capacity_wh == 45000.0,
          "vehicle presets select distinct parameters");
    flat.start_soc = 1.0;
    flat.energy_model = route_energy_model("cycle");
    const auto explicit_ramps = simulate(short_route, no_sun, flat, Policy::steady);
    check(explicit_ramps.completed && explicit_ramps.drive_hours > 0.25, "cycle model simulates both speed ramps");
    flat.energy_model = [](const ecm_cont::RouteInput&, const ecm::VehicleParams&,
                           const ecm::Drivetrain&, const ecm::SolarArray&) {
        return ecm_cont::RouteEnergy{180.0, 30.0, 5.0};
    };
    const auto substituted = simulate(short_route, no_sun, flat, Policy::steady);
    near(substituted.drive_hours, 0.05, "a custom estimator controls leg duration");
    near(substituted.finish_soc, 0.75, "a custom estimator feeds the battery update");

    std::mt19937_64 experiment_seed(42);
    const auto first_draw = draw_weather(route, experiment_seed);
    std::size_t rows = 0;
    std::size_t traced_legs = 0;
    run_experiment(route, defaults, {2, 42}, [&](const TrialResult& trial) {
        check(trial.trial == rows / policies.size() + 1 && trial.policy == policies[rows % policies.size()],
              "trial numbering and policy order");
        if (trial.trial == 1) {
            const auto expected = simulate(route, first_draw, defaults, trial.policy);
            near(trial.result.finish_soc, expected.finish_soc, "policies use the same weather draw");
        }
        ++rows;
    }, {1, [&](Policy, const LegObservation& observation) {
        near(observation.weather.headwind_mps, first_draw[observation.leg].headwind_mps,
             "tracing uses the same wind as the paired draw");
        ++traced_legs;
    }});
    check(rows == 2 * policies.size(), "experiment streams all policy outcomes");
    check(traced_legs == route.size() * policies.size(), "only the chosen draw emits leg logs");
}
