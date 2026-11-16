#include "ecm/lab.hpp"
#include "ecm/bwsc.hpp"

#include <cmath>
#include <stdexcept>

namespace ecm::lab {

Config vehicle_config(std::string_view name) {
    Config config;
    if (name == "reference") {
        return config;
    }
    if (name == "sr7") {
        config.vehicle = {
            .rho = 1.18, .Cd = 0.095, .A = 1.4, .m = 660.0, .g = 9.80665, .Crr = 0.005, .rw = 0.27, .X = 150.0,
        };
        config.drivetrain = {0.91, 0.60, 2500.0};
        config.array = {4.8, 0.22, 0.92};
        // Published 500 kg vehicle plus the two 80 kg ballasted occupants.
        config.capacity_wh = bwsc_2025_cruiser_max_wh;
        config.start_soc = 1.0;
        config.energy_rules = EnergyRules::bwsc_2025_cruiser;
        return config;
    }
    if (name == "sr8") {
        config.vehicle = {
            .rho = 1.18, .Cd = 0.18, .A = 1.55, .m = 850.0, .g = 9.80665, .Crr = 0.007, .rw = 0.31, .X = 250.0,
        };
        config.drivetrain = {0.91, 0.62, 4000.0};
        config.array = {4.5, 0.23, 0.92};
        config.capacity_wh = 45000.0;
        return config;
    }
    throw std::invalid_argument("unknown vehicle " + std::string(name) + "; use reference, sr7 or sr8");
}

RouteEnergyModel route_energy_model(std::string_view name) {
    return route_energy_model(name, 2.0);
}

RouteEnergyModel route_energy_model(std::string_view name, double gust_sd_mps) {
    if (name == "segment") {
        return ecm_cont::segment_route_energy;
    }
    if (name == "cycle") {
        return [](const ecm_cont::RouteInput& input, const ecm::VehicleParams& vehicle,
                  const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array) {
            return ecm_cont::cycle_route_energy(input, vehicle, drivetrain, array);
        };
    }
    if (name == "mapped" || name == "gust") {
        if (name == "gust" && !(gust_sd_mps >= 0.0 && std::isfinite(gust_sd_mps))) {
            throw std::invalid_argument("gust standard deviation must be nonnegative and finite");
        }
        const double spread = name == "gust" ? gust_sd_mps : 0.0;
        return [spread](const ecm_cont::RouteInput& input, const ecm::VehicleParams& vehicle,
                        const ecm::Drivetrain& drivetrain, const ecm::SolarArray& array) {
            // Illustrative wheel-equivalent map shared by the scenario presets.
            static const ecm::EfficiencyMap map(
                {0, 40, 80, 120}, {0, 15, 30, 100, 250},
                {0.60, 0.72, 0.76, 0.77, 0.68,
                 0.76, 0.87, 0.91, 0.86, 0.76,
                 0.84, 0.90, 0.94, 0.88, 0.78,
                 0.80, 0.88, 0.92, 0.86, 0.75});
            return spread == 0.0
                ? ecm_cont::mapped_route_energy(input, vehicle, drivetrain, array, map)
                : ecm_cont::gust_route_energy(input, vehicle, drivetrain, array, map, spread);
        };
    }
    throw std::invalid_argument("unknown ECM model " + std::string(name) + "; use segment, cycle, mapped or gust");
}

}  // namespace ecm::lab
