#include "ecm/forecast.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ecm::lab {
namespace {

constexpr std::size_t warmup = 400;
constexpr std::size_t retained = 1600;
constexpr WeatherBias prior_sd{0.18, 3.0};
constexpr WeatherBias observation_sd{0.13, 1.8};

double square(double value) { return value * value; }

NormalPosterior normal_posterior(std::span<const WeatherBias> residuals, double WeatherBias::*member,
                                 double prior, double noise) {
    const double precision = 1.0 / square(prior) + static_cast<double>(residuals.size()) / square(noise);
    double total = 0.0;
    for (auto residual : residuals) total += residual.*member;
    return {total / (square(noise) * precision), std::sqrt(1.0 / precision)};
}

double split_rhat(const std::array<CalibrationChain, 4>& chains, double WeatherBias::*member) {
    constexpr auto half = retained / 2;
    std::array<double, 8> means{};
    double within = 0.0;
    for (std::size_t chain = 0; chain < chains.size(); ++chain) {
        for (std::size_t split = 0; split < 2; ++split) {
            const auto samples = std::span(chains[chain].samples).subspan(split * half, half);
            double mean = 0.0;
            for (const auto sample : samples) mean += sample.*member / half;
            means[2 * chain + split] = mean;
            for (const auto sample : samples) within += square(sample.*member - mean) / (half - 1);
        }
    }
    within /= means.size();
    const double centre = std::accumulate(means.begin(), means.end(), 0.0) / means.size();
    double between = 0.0;
    for (double mean : means) between += half * square(mean - centre) / (means.size() - 1);
    if (!(within > 0.0)) throw std::runtime_error("forecast chains have zero within-chain variance");
    return std::sqrt(((half - 1) * within + between) / (half * within));
}

}  // namespace

std::span<const WeatherBias> synthetic_weather_residuals() {
    static constexpr std::array residuals{
        WeatherBias{-.11, 2.0}, WeatherBias{-.04, -.7}, WeatherBias{.01, 1.2}, WeatherBias{-.08, .8},
        WeatherBias{.03, -1.4}, WeatherBias{-.15, 2.3}, WeatherBias{-.06, .4}, WeatherBias{-.02, -.3},
        WeatherBias{-.09, 1.6}, WeatherBias{.04, -.1}, WeatherBias{-.07, 1.1}, WeatherBias{-.03, .5},
    };
    return residuals;
}

ForecastCalibration calibrate_forecast(std::span<const WeatherBias> residuals, std::uint64_t seed) {
    if (residuals.empty() || !std::ranges::all_of(residuals, [](auto residual) {
        return std::isfinite(residual.solar) && std::abs(residual.solar) <= 1.0 &&
               std::isfinite(square(residual.wind_mps));
    })) {
        throw std::invalid_argument("forecast calibration needs valid solar-factor and signed wind residuals");
    }
    ForecastCalibration result{};
    result.solar_exact = normal_posterior(residuals, &WeatherBias::solar, prior_sd.solar, observation_sd.solar);
    result.wind_exact = normal_posterior(residuals, &WeatherBias::wind_mps, prior_sd.wind_mps, observation_sd.wind_mps);
    const auto log_density = [&](WeatherBias bias) {
        double rss = square(bias.solar / prior_sd.solar) + square(bias.wind_mps / prior_sd.wind_mps);
        for (const auto residual : residuals) {
            rss += square((residual.solar - bias.solar) / observation_sd.solar) +
                   square((residual.wind_mps - bias.wind_mps) / observation_sd.wind_mps);
        }
        return -0.5 * rss;
    };
    for (std::size_t c = 0; c < result.chains.size(); ++c) {
        std::seed_seq seeds{static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(seed >> 32),
                            static_cast<std::uint32_t>(c)};
        std::mt19937_64 random(seeds);
        std::normal_distribution<double> normal;
        std::uniform_real_distribution<double> uniform;
        const double solar_sign = c % 2 ? 1.0 : -1.0;
        const double wind_sign = c / 2 ? 1.0 : -1.0;
        WeatherBias state{result.solar_exact.mean + solar_sign * 2.5 * result.solar_exact.sd,
                          result.wind_exact.mean + wind_sign * 2.5 * result.wind_exact.sd};
        double log_current = log_density(state);
        if (!std::isfinite(log_current)) throw std::overflow_error("forecast residuals exceed numerical range");
        std::size_t accepted = 0;
        auto& chain = result.chains[c];
        chain.samples.reserve(retained);
        for (std::size_t i = 0; i < warmup + retained; ++i) {
            const WeatherBias candidate{state.solar + 1.7 * result.solar_exact.sd * normal(random),
                                        state.wind_mps + 1.7 * result.wind_exact.sd * normal(random)};
            const double log_candidate = log_density(candidate);
            if (std::log1p(-uniform(random)) < log_candidate - log_current) {
                state = candidate;
                log_current = log_candidate;
                if (i >= warmup) ++accepted;
            }
            if (i >= warmup) chain.samples.push_back(state);
        }
        chain.acceptance = static_cast<double>(accepted) / retained;
    }
    result.split_rhat = {split_rhat(result.chains, &WeatherBias::solar), split_rhat(result.chains, &WeatherBias::wind_mps)};
    return result;
}

std::vector<Weather> draw_forecast_weather(std::span<const Leg> route, const ForecastCalibration& calibration,
                                          std::mt19937_64& random) {
    const auto& chain = calibration.chains[std::uniform_int_distribution<std::size_t>(0, 3)(random)];
    if (chain.samples.empty()) throw std::invalid_argument("forecast calibration has an empty chain");
    const auto bias = chain.samples[std::uniform_int_distribution<std::size_t>(0, chain.samples.size() - 1)(random)];
    if (!std::isfinite(bias.solar) || !std::isfinite(bias.wind_mps)) {
        throw std::invalid_argument("forecast calibration has a non-finite bias");
    }
    std::normal_distribution<double> normal;
    // Standardised AR(1) innovations keep the marginal variance fixed.
    const auto advance = [&](double previous, double rho) {
        return rho * previous + std::sqrt(1.0 - rho * rho) * normal(random);
    };
    double solar_day = normal(random), wind_day = normal(random);
    double solar_leg = normal(random), wind_leg = normal(random);
    int day = route.empty() ? 0 : route.front().day;
    std::vector<Weather> result;
    result.reserve(route.size());
    for (const auto& leg : route) {
        if (leg.day != day) {
            solar_day = advance(solar_day, 0.55);
            wind_day = advance(wind_day, 0.55);
            day = leg.day;
        }
        solar_leg = advance(solar_leg, 0.72);
        wind_leg = advance(wind_leg, 0.72);
        const double clear = .83 + bias.solar + .16 * solar_day;
        result.push_back({
            std::clamp(clear + .055 * solar_leg, .25, 1.0),
            .6 + bias.wind_mps + 2.1 * wind_day + 1.1 * wind_leg,
            std::clamp(1.0 + .075 * normal(random), .78, 1.24),
            std::clamp(clear, .30, 1.0),
        });
    }
    return result;
}

void draw_rain(std::span<const Leg> route, std::span<Weather> weather, double multiplier,
               std::mt19937_64& random) {
    if (route.size() != weather.size() || !(multiplier >= 0.0 && multiplier <= 2.0)) {
        throw std::invalid_argument("rain needs matching legs and a multiplier from 0 to 2");
    }
    std::normal_distribution<double> normal;
    constexpr double persistence = .72;
    const double innovation = std::sqrt(1.0 - persistence * persistence);
    double front = multiplier > 0.0 ? normal(random) : 0.0;
    double wetness = 0.0;
    for (std::size_t i = 0; i < route.size(); ++i) {
        auto& leg_weather = weather[i];
        if (multiplier > 0.0) front = persistence * front + innovation * normal(random);
        const double moisture = front + 2.2 * (.75 - leg_weather.solar_factor) - .60;
        const double rain = std::clamp(7.5 * multiplier * moisture, 0.0, 40.0);
        const double equilibrium = rain / (4.0 + rain);
        const double response_km = rain > 0.0 ? 35.0 : 115.0;
        const double previous = wetness;
        wetness += (equilibrium - wetness) * -std::expm1(-route[i].distance_km / response_km);
        leg_weather.rain_mm_h = rain;
        leg_weather.road_wetness = (previous + wetness) / 2.0;
    }
}

}  // namespace ecm::lab
