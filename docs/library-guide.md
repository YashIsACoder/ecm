# Library integration

## C++

Build and install the headers and CMake targets from the repository root.

```sh
cmake -S . -B build-sdk -DECM_BUILD_PYTHON=OFF -DECM_BUILD_ROUTE_LAB=OFF
cmake --build build-sdk --parallel
ctest --test-dir build-sdk --output-on-failure
cmake --install build-sdk --prefix "$PWD/build-sdk/install"
```

In your project, add the install prefix to `CMAKE_PREFIX_PATH`, then link
the component you use. Both targets require C++20.

```cmake
find_package(ECM 0.2 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ECM::ecm_cont)
```

For a source checkout, `add_subdirectory` creates the same targets.
The lab, tests and Python extension default off when embedded.
Use `ECM::ecm` for the segment API alone.

```cpp
#include <ecm_cont/route_energy.hpp>

const ecm::VehicleParams car{.Cd = 0.095, .A = 1.4, .m = 500, .X = 150};
const ecm_cont::RouteInput leg{{4000, 20, 0, 0, 0.015, 4}, 720};
const auto result = ecm_cont::cycle_route_energy(
    leg, car, {0.91, 0.60, 2500}, {4.8, 0.22, 0.92});
const double draw_wh = result.net_wh();
```

[Model choices and units](route-models.md) · [Installed-package example](../tests/consumer)

## Python

From the repository root, install the extension with
`python -m pip install .`. Wheel builds select the Python install component
of the same CMake project. For a development CMake build, enable
`ECM_BUILD_PYTHON`. Set `Python3_EXECUTABLE` if you need a specific
interpreter. CMake uses a local pybind11 package when available and can fetch
the pinned fallback when building with bindings enabled.

```python
import ecm

vehicle = ecm.VehicleParams()
vehicle.X = 65
profile = [
    ecm.SpeedInterval(20.0, 0.0, 18.0, irradiance_w_m2=650),
    ecm.SpeedInterval(90.0, 18.0, 18.0, irradiance_w_m2=650),
    ecm.SpeedInterval(10.0, 18.0, 0.0, grade=-0.02),
]
energy = ecm.drive_cycle(
    vehicle,
    profile,
    ecm.Drivetrain(0.9, 0.55, 450),
    ecm.SolarArray(4, 0.20, 0.90),
)
print(energy.net_dc_wh)
```

The existing discrete and continuous `stint()` interfaces remain available.
The discrete Python API uses `ecm.ECM(vehicle, ecm.StepEfficiency())`
and returns signed energy in joules.
