# Route model adapters

All three C++ adapters take a road segment, plane-of-array irradiance,
vehicle, drivetrain and array. They return seconds, signed DC demand in
watt-hours and solar generation in watt-hours. An application can pass any
adapter into a route simulator with the same result type. Parking and battery
capacity are managed by the caller.

| Adapter | Motion | Motoring efficiency |
| --- | --- | --- |
| `segment_route_energy` | Cruise with a lumped entry-to-exit kinetic-energy change | Constant |
| `cycle_route_energy` | Explicit entry and exit speed ramps and a cruise phase | Constant |
| `mapped_route_energy` | Same speed cycle | Bilinear speed–torque map |

```cpp
#include <ecm_cont/route_energy.hpp>

const ecm_cont::RouteInput leg{{4000, 20, 0, 0, 0.015, 4}, 720};
const ecm::VehicleParams vehicle;
const ecm::Drivetrain drivetrain{0.90, 0.55, 900};
const ecm::SolarArray array{4.8, 0.22, 0.90};

const auto baseline = ecm_cont::segment_route_energy(leg, vehicle, drivetrain, array);
const auto continuous = ecm_cont::cycle_route_energy(leg, vehicle, drivetrain, array);
```

## Speed-profile utility

`route_speed_profile` converts the requested leg into at most three
piecewise-linear intervals. It computes each speed change's distance from
its duration times the mean of the endpoint speeds. The remaining distance
is travelled at the requested cruise speed. Default ramp magnitudes are
0.6 m/s² accelerating and 1.0 m/s² braking.

If there is insufficient distance to reach a peak cruise speed, the utility
solves for a triangular profile by setting the two ramp distances to sum to
the leg length `L`. With entry speed `u`, exit speed `v`, acceleration
`a`, and braking magnitude `b`, the feasible peak speed satisfies

```math
p^2 = \frac{2L + u^2/a + v^2/b}{1/a + 1/b}.
```

A short leg targeting a speed below both endpoints uses the corresponding
brake-then-accelerate calculation. If the requested endpoint speeds alone
require more distance than the leg provides, the utility rejects the leg.
These are prescribed accelerations. Motor and tyre traction limits require
additional vehicle data.

The linear ramps feed the [continuous energy integrator](drive-cycle.md).
Its mapped mode refines the motoring-power integration in speed space while
keeping the same road loads and fixed regeneration settings.
