#include "ecm/fleet.hpp"

#include <stdexcept>
#include <utility>

namespace ecm::lab {

std::vector<Leg> bwsc_2025_cruiser_route() {
    constexpr std::pair<std::string_view, double> anchors[]{
        {"Barrow Creek", 1209.8}, {"Port Augusta", 2717.6}, {"Adelaide", 3021.5},
    };
    auto route = default_route();
    std::size_t start = 0;
    double prior = 0.0;
    for (const auto& [destination, km] : anchors) {
        std::size_t end = start;
        double original = 0.0;
        while (end < route.size() && route[end].destination != destination) {
            original += route[end].distance_km;
            ++end;
        }
        if (end == route.size()) throw std::logic_error("missing 2025 distance anchor");
        original += route[end].distance_km;
        ++end;
        const double scale = (km - prior) / original;
        for (; start < end; ++start) route[start].distance_km *= scale;
        prior = km;
    }
    return route;
}

std::vector<CruiserProfile> bwsc_2025_cruiser_profiles() {
    auto sunswift = vehicle_config("sr7");
    sunswift.energy_model = route_energy_model("mapped");
    sunswift.race_schedule = true;
    sunswift.stage_timing = StageTiming::bwsc_2025;
    sunswift.strategy.reserve_strategy = ReserveStrategy::energy_envelope;
    sunswift.strategy.reserve_slow_kmh = 65.0;
    sunswift.strategy.reserve_target_soc = 0.15;

    // The rival presets are explicit what-if inputs, not engineering specifications.
    // Cd is set against the SR7 reference area so that these use the stated CdA.
    const auto rival = [&sunswift](double mass_kg, double drag_area, double crr, double array_m2,
                                  double cell_efficiency, double auxiliaries_w, double pace_ceiling_kmh) {
        Config config = sunswift;
        config.vehicle.m = mass_kg;
        config.vehicle.Cd = drag_area / config.vehicle.A;
        config.vehicle.Crr = crr;
        config.vehicle.X = auxiliaries_w;
        config.array.area_m2 = array_m2;
        config.array.cell_efficiency = cell_efficiency;
        config.strategy.reserve_fast_kmh = pace_ceiling_kmh;
        return FleetEntry{config};
    };

    return {
        {"sunswift", "Sunswift Racing", "Sunswift 7", "Australia", {sunswift}},
        {"vtc", "VTC Solar Car Team", "Sophie 8X", "Hong Kong", rival(610, .146, .0048, 5.1, .23, 130, 86)},
        {"solaride", "Solaride", "Solaride III Enefit", "Estonia", rival(640, .153, .0047, 5.5, .22, 140, 89)},
        {"onda", "Onda Solare", "Emilia 5.9", "Italy", rival(675, .158, .0051, 5.7, .23, 150, 84)},
        {"apollo", "Apollo Solar Car Team", "Apollo IX Plus", "Taiwan", rival(705, .171, .0053, 5.4, .225, 170, 91)},
    };
}

}  // namespace ecm::lab
