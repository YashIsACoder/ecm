# Cruiser fleet and solar lighting

The atlas uses five 2025 Cruiser entrants from the [official field and results](https://www.bridgestone.com/bwsc/about/result/).
The rival masses, drag areas, rolling coefficients, auxiliaries, arrays and
pace ceilings are illustrative inputs. The comparison is a controlled
simulation, with no design score, start offsets, crew strategy data or
historical weather reconstruction. The fixture's waypoints and distances
approximate the Darwin–Adelaide corridor.

## C++ fleet API

Link against `ECM::lab`. Every race is prepared once, including the
energy-envelope planning forecast. Each Monte Carlo trial generates one
route-wide weather vector, which is supplied to all entries in order. The
entry can choose its own model, controller and car configuration. World
conditions use the same samples where the cars are on the same leg. Each car
has its own speed, depletion, controls, overnight recharge and stage timing.

```cpp
#include <ecm/fleet.hpp>
#include <iostream>

int main() {
    using namespace ecm::lab;
    auto route = default_route();
    auto field = bwsc_2025_cruiser_profiles();
    std::vector<FleetEntry> entries;
    for (const auto& profile : field) entries.push_back(profile.entry);
    const Experiment weather{128, 2025, WeatherModel::posterior, 1.0};
    run_fleet(route, entries, weather, [&](const FleetResult& row) {
        std::cout << field[row.entry].car << ' ' << row.trial << ' '
                  << row.result.distance_km << '\n';
    });
}
```

Use a `FleetTrace` to stream leg observations for one trial. The browser's
native report publishes Wilson intervals on finish rates, paired Newcombe
intervals against Sunswift, and median elapsed race time among finishers.
`--atlas-report` uses the built-in corridor and the five presets. It accepts
the common model, gust, weather, seed, trial count and trace draw options.
For a custom fleet or route, use `run_fleet` directly.

## Solar position and relief

The native [solar API](../../../include/ecm/solar.hpp) uses the NOAA
fractional-year approximation for solar declination and equation of time.
It returns an Earth-fixed unit direction from a UTC instant and projects it
into local east, north and up for a geographic point. Azimuth is clockwise
from true north. Elevation is geometric, so refraction and local obstructions
are omitted. For a tilted array, `direct_beam_factor` returns the positive dot
product between the sun direction and array normal, or zero below the horizon.

The atlas stores a native solar vector sampled every ten minutes and
interpolates it during playback. The clock begins at 08:00 ACST on
24 August 2025. The browser rotates the vector to the selected car's
latitude and longitude. Its horizontal components blend four directional
shadows made from the tile gradients. Elevation controls twilight and
night shading. The solar position drives the visualization. The ECM energy
accounting uses the leg irradiance and matched weather forecasts.

- [NOAA Solar Position Equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF)
- [NREL SPA worked benchmark](https://docs.nlr.gov/docs/fy08osti/34302.pdf)
- [2025 BWSC regulations](https://cms.worldsolarchallenge.org/app/uploads/2025/03/3297_2025_bwsc_regulations_release_v20_published_28_october_2024-1.pdf)

The terrain asset is regenerated with
`uv run apps/route-lab/analysis/atlas_terrain.py` from the repo root.
It downloads Mapzen Terrarium tiles from the public AWS endpoint, resamples
them onto a longitude/latitude grid, and writes a five-band WebP sprite. The
artistic contrast is exaggerated to keep the relief readable at country scale.
The generator also updates the separate coastline geometry from Natural Earth
1:50m v5.1.2. The route uses the checked-in course waypoints.

- [Mapzen terrain tiles](https://www.mapzen.com/blog/terrain-tile-service/)
- [Terrain data source attribution](https://github.com/tilezen/joerd/blob/master/docs/attribution.md)
- [Natural Earth coastline](https://www.naturalearthdata.com/about/terms-of-use/)
