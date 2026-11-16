#include "ecm/forecast.hpp"
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

    auto staged = route;
    staged[0].control_minutes = 30.0;
    staged.back().control_minutes = 30.0;
    auto schedule = defaults;
    schedule.race_schedule = true;
    std::vector<LegObservation> timetable;
    const auto scheduled = simulate(staged, weather, schedule, Policy::steady,
                                    [&](const LegObservation& leg) { timetable.push_back(leg); });
    check(scheduled.completed && timetable.size() == 3, "scheduled route finishes");
    near(timetable[0].race_depart_hours, 0.0, "race starts at zero elapsed hours");
    near(timetable[0].control_hours, 0.5, "control holds for 30 minutes");
    near(timetable[1].race_depart_hours, timetable[0].race_arrive_hours + 0.5, "clock includes control hold");
    near(timetable[2].race_depart_hours, 24.0, "next morning at the same local time");
    near(timetable[1].overnight_hours, 24.0 - timetable[1].race_arrive_hours, "overnight wait is timestamped");
    near(timetable.back().control_hours, 0.0, "finish line does not hold the car");
    near(scheduled.race_hours, timetable.back().race_arrive_hours, "race duration ends at final arrival");
    check(scheduled.race_hours > scheduled.drive_hours, "elapsed race time includes holds and waits");

    const std::array planning_route{
        Leg{1, "Midpoint", 10.0, 0.0, 0.0, 100.0},
        Leg{1, "Finish", 15.0, 0.0, 0.0, 90.0},
    };
    const std::array calm{Weather{0.5, 0.0, 1.0, 0.5}, Weather{0.5, 0.0, 1.0, 0.5}};
    Config planning;
    planning.capacity_wh = 100.0;
    planning.start_soc = .9;
    planning.strategy = {67.0, 120.0, 100.0, 70.0, 40.0, .2, ReserveStrategy::energy_envelope};
    planning.energy_model = [](const ecm_cont::RouteInput& input, const ecm::VehicleParams&,
                               const ecm::Drivetrain&, const ecm::SolarArray&) {
        const double speed = input.road.cruise_speed_mps;
        const double draw = input.road.distance_m / 1000.0 * speed * 3.6 / 20.0;
        return ecm_cont::RouteEnergy{input.road.distance_m / speed, draw, 0.0};
    };
    std::vector<LegObservation> plan;
    const auto planned = simulate(planning_route, calm, planning, Policy::reserve_first,
                                  [&](const LegObservation& leg) { plan.push_back(leg); });
    check(planned.completed && plan.size() == 2, "envelope completes the analytic route");
    near(*plan[0].planned_arrival_soc, .5, "arrival target includes the remaining leg's minimum draw");
    near(*plan.back().planned_arrival_soc, .2, "finish target equals the configured reserve");
    near(plan[0].cruise_kmh, 77.5, "envelope picks the fastest grid point meeting the target");
    near(plan[0].mean_speed_kmh, 77.5, "trace includes true leg mean speed");
    near(plan[1].cruise_kmh, 40.0, "controller preserves terminal charge with the lower bound");
    check(planned.finish_soc >= .2, "envelope meets the terminal target");
    const auto fixed = simulate(planning_route, calm, planning, Policy::push,
                                [](const LegObservation& leg) {
        check(!leg.planned_arrival_soc, "fixed pacing does not use the envelope");
        if (leg.leg == 0) near(leg.cruise_kmh, 100.0, "configured target respects the route cap");
    });
    check(!fixed.completed, "the aggressive policy overdraws the test pack");
    planning.weather_adjustments = {.5, -2.0};
    const auto stress = simulate(planning_route, calm, planning, Policy::steady, [](const LegObservation& leg) {
        near(leg.weather.solar_factor, .25, "scenario changes the sampled solar fraction");
        near(leg.weather.headwind_mps, -2.0, "scenario offsets signed wind");
    });

    check(stress.completed, "weather-adjusted run reaches the finish");

    auto wet = calm;
    wet[0].rain_mm_h = 10.0;
    wet[0].road_wetness = .5;
    planning.weather_adjustments = {};
    const auto wet_run = simulate(planning_route, wet, planning, Policy::push, [](const LegObservation& leg) {
        if (leg.leg != 0) return;
        near(leg.effective_pace_cap_kmh, 93.0, "wet roads reduce the route pace cap");
        near(leg.cruise_kmh, 93.0, "configured speed respects the wet pace cap");
        near(leg.weather.rolling_multiplier, 1.07, "wet roads increase rolling losses");
        near(leg.weather.solar_factor, .5 * (1.0 - .35 * 10.0 / 15.0), "rain reduces usable sunlight");
    });
    check(!wet_run.completed, "wet run uses the same depletion rules");

    const auto residuals = synthetic_weather_residuals();
    const auto forecast = calibrate_forecast(residuals, 2025);
    const auto rerun = calibrate_forecast(residuals, 2025);
    double solar_mean = 0.0, wind_mean = 0.0;
    const auto sample_count = static_cast<double>(forecast.chains.size() * forecast.chains.front().samples.size());
    for (std::size_t c = 0; c < forecast.chains.size(); ++c) {
        const auto& chain = forecast.chains[c];
        check(chain.samples.size() == 1600 && chain.acceptance > .2 && chain.acceptance < .6,
              "forecast retains the requested samples and accepts well-scaled steps");
        near(chain.samples.back().solar, rerun.chains[c].samples.back().solar, "forecast calibration is seeded");
        for (const auto bias : chain.samples) {
            solar_mean += bias.solar / sample_count;
            wind_mean += bias.wind_mps / sample_count;
        }
    }
    check(std::abs(solar_mean - forecast.solar_exact.mean) < .012 &&
          std::abs(wind_mean - forecast.wind_exact.mean) < .15, "MCMC means agree with the conjugate posterior");
    check(forecast.split_rhat.solar < 1.06 && forecast.split_rhat.wind_mps < 1.06,
          "split chains agree on the synthetic calibration");
    std::mt19937_64 weather_random(19);
    const auto predicted = draw_forecast_weather(route, forecast, weather_random);
    check(predicted.size() == route.size(), "predictive draw covers the route");
    near(predicted[0].park_solar_factor, predicted[1].park_solar_factor, "predictions reuse daily conditions");
    for (const auto& conditions : predicted) {
        check(conditions.solar_factor >= .25 && conditions.solar_factor <= 1.0 &&
              std::isfinite(conditions.headwind_mps), "forecast stays within physical input bounds");
    }

    const auto long_route = default_route();
    near(long_route.front().rain_day_probability, .0096774, "route reads the August wet-day prior");
    auto shower_route = planning_route;
    shower_route[0].rain_day_probability = 1.0;
    shower_route[1].rain_day_probability = 0.0;
    auto generated = calm;
    auto generated_again = calm;
    std::mt19937_64 wet_rng(91), wet_repeat(91);
    draw_rain(shower_route, generated, 1.0, wet_rng);
    draw_rain(shower_route, generated_again, 1.0, wet_repeat);
    near(generated.front().solar_factor, calm.front().solar_factor, "rain preserves the base forecast");
    near(generated.front().rain_mm_h, generated_again.front().rain_mm_h, "rain is seeded");
    check(generated[0].rain_mm_h > 0.0 && generated[1].rain_mm_h == 0.0 &&
          generated[1].road_wetness > 0.0, "roads retain moisture after a shower");

    std::mt19937_64 climate_rng(87), stress_rng(87);
    std::vector<Weather> climate(long_route.size(), calm[0]);
    auto stress_weather = climate;
    int climate_legs = 0, stressed_legs = 0;
    for (int draw = 0; draw < 2000; ++draw) {
        draw_rain(long_route, climate, 1.0, climate_rng);
        draw_rain(long_route, stress_weather, 8.0, stress_rng);
        for (std::size_t i = 0; i < climate.size(); ++i) {
            const auto& base = climate[i];
            const auto& stress_leg = stress_weather[i];
            check(base.road_wetness >= 0.0 && base.road_wetness <= 1.0, "wetness stays bounded");
            check(base.rain_mm_h == 0.0 || stress_leg.rain_mm_h == base.rain_mm_h,
                  "increased exposure only adds wet events for the same seed");
            climate_legs += base.rain_mm_h > 0.0;
            stressed_legs += stress_leg.rain_mm_h > 0.0;
        }
    }
    check(climate_legs > 0 && climate_legs < 2000 * long_route.size() / 50,
          "August rain affects fewer than two percent of default legs");
    check(stressed_legs > climate_legs * 3, "stress mode increases rain frequency");

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
    double last_hours = 0.0;
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
        if (observation.leg == 0) {
            last_hours = 0.0;
            near(observation.depart_soc, defaults.start_soc, "first leg uses starting charge");
        }
        check(observation.elapsed_hours > last_hours, "native driving time advances within a policy");
        near(observation.irradiance_w_m2, route[observation.leg].clear_sky_w_m2 * observation.weather.solar_factor,
             "trace reports modelled plane-of-array irradiance");
        check(observation.components.has_value(), "native trace includes energy components");
        near(observation.components->traction_wh + observation.components->auxiliary_wh, observation.demand_wh,
             "trace DC demand agrees with the component breakdown");
        last_hours = observation.elapsed_hours;
        ++traced_legs;
    }});
    check(rows == 2 * policies.size(), "experiment streams all policy outcomes");
    check(traced_legs == route.size() * policies.size(), "only the chosen draw emits leg logs");
}
