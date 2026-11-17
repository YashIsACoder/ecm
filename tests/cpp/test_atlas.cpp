#include "ecm/bwsc.hpp"
#include "ecm/fleet.hpp"
#include "ecm/solar.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

int main() {
    using namespace ecm::lab;
    using namespace std::chrono;

    // NREL SPA worked example, 17 October 2003 in Colorado. The simpler
    // NOAA series should come within one degree of that reference.
    const auto reference_time = sys_days{2003y / October / 17} + 19h + 30min + 30s;
    const auto reference = solar_position(reference_time, {39.742476, -105.1786});
    assert(std::abs(reference.azimuth_deg - 194.34) < 1.0);
    assert(std::abs(reference.elevation_deg - 39.87) < 1.0);
    assert(std::abs(direct_beam_factor(reference, 0.0, 0.0) - reference.up) < 1e-12);
    assert(std::abs(direct_beam_factor(reference, 90.0 - reference.elevation_deg, reference.azimuth_deg)
                    - 1.0) < 1e-12);

    const auto darwin_start = sys_days{2025y / August / 23} + 22h + 30min;
    const auto morning = solar_position(darwin_start, {-12.46, 130.85});
    const auto midday = solar_position(darwin_start + 4h, {-12.46, 130.85});
    const auto evening = solar_position(darwin_start + 9h, {-12.46, 130.85});
    const auto night = solar_position(darwin_start + 16h, {-12.46, 130.85});
    assert(morning.east > 0 && evening.east < 0 && midday.up > morning.up);
    assert(night.up < 0 && direct_beam_factor(night, 15.0, 0.0) == 0.0);
    const auto sun = sun_direction(darwin_start);
    assert(std::abs(std::hypot(sun.x, sun.y, sun.z) - 1.0) < 1e-12);
    bool rejected = false;
    try { (void)solar_position(darwin_start, {91.0, 0.0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    const auto profiles = bwsc_2025_cruiser_profiles();
    for (const auto& profile : profiles) {
        assert(profile.entry.config.capacity_wh <= bwsc_2025_cruiser_max_wh);
        assert(profile.entry.config.stage_timing == StageTiming::bwsc_2025);
    }
    const auto route = default_route();
    const auto entry = profiles.front().entry;
    const std::vector fleet{entry, entry};
    const Experiment experiment{2, 2025, WeatherModel::posterior, 1.0};
    std::vector<FleetResult> field;
    std::vector<LegObservation> traces[2];
    run_fleet(route, fleet, experiment, [&](const FleetResult& result) { field.push_back(result); },
              {2, [&](std::size_t index, const LegObservation& leg) { traces[index].push_back(leg); }});
    assert(field.size() == 4);
    assert(!traces[0].empty() && traces[0].size() == traces[1].size());
    for (std::size_t index = 0; index < traces[0].size(); ++index) {
        assert(traces[0][index].weather.headwind_mps == traces[1][index].weather.headwind_mps);
        assert(traces[0][index].weather.rain_mm_h == traces[1][index].weather.rain_mm_h);
    }
    run_experiment(route, entry.config, experiment, [&](const TrialResult& trial) {
        if (trial.policy != entry.policy) return;
        const auto first = (trial.trial - 1) * fleet.size();
        for (std::size_t index = 0; index < fleet.size(); ++index) {
            const auto& row = field[first + index];
            assert(row.trial == trial.trial && row.entry == index);
            assert(row.result.distance_km == trial.result.distance_km);
            assert(row.result.demand_wh == trial.result.demand_wh);
        }
    });
}
