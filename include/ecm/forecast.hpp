#pragma once

#include "ecm/lab.hpp"

namespace ecm::lab {

struct WeatherBias {
    double solar;  // Offset to the clear-sky factor.
    double wind_mps;
};

struct NormalPosterior {
    double mean;
    double sd;
};

struct CalibrationChain {
    std::vector<WeatherBias> samples;
    double acceptance;
};

struct ForecastCalibration {
    std::array<CalibrationChain, 4> chains;
    NormalPosterior solar_exact;
    NormalPosterior wind_exact;
    WeatherBias split_rhat;  // Classical split R-hat for the two scalar coordinates.
};

[[nodiscard]] std::span<const WeatherBias> synthetic_weather_residuals();
[[nodiscard]] ForecastCalibration calibrate_forecast(std::span<const WeatherBias> residuals, std::uint64_t seed);
[[nodiscard]] std::vector<Weather> draw_forecast_weather(
    std::span<const Leg> route, const ForecastCalibration& calibration, std::mt19937_64& random);

// Apply seeded showers and carry wetness across legs. Does not change the dry weather fields.
void draw_rain(std::span<const Leg> route, std::span<Weather> weather, double multiplier,
               std::mt19937_64& random);

}  // namespace ecm::lab
