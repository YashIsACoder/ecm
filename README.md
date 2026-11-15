# ECM · Energy Consumption Model

C++20 energy modelling for solar-electric vehicles, with a Monte Carlo route
lab adapted from Sunswift Racing strategy work. ECM estimates energy for a
road segment or continuous speed trace. The lab uses those estimates to compare
pacing and battery outcomes under the same simulated weather.

| Model | Calculation |
| --- | --- |
| Segment | Cruise at a fixed speed with a lumped kinetic-energy change |
| Drive cycle | Integrate explicit speed ramps and regenerative power limits |
| Mapped cycle | Interpolate motoring efficiency using wheel speed and torque |

## Run the route lab

From the repository root, use CMake 3.20 or later and a C++20 compiler.
Python 3.9 or later runs the analysis scripts.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DECM_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 apps/route-lab/analysis/run_matrix.py build/bin/solar-race \
  --route apps/route-lab/data/rolling-1150.csv --seed 2025 --out runs/rolling-2025
```

The lab writes raw trial CSVs, paired comparisons, and a manifest. Run
`build/bin/solar-race --help` for a single scenario or to use your own route.
The checked-in routes are synthetic. The SR7 preset uses published figures
where available, and SR8 models a provisional battery-and-solar scenario.
[Lab guide](apps/route-lab/README.md) · [SR7 and SR8 inputs and results](apps/route-lab/docs/sunswift-profiles.md)

## Use the library

- **C++** builds against `ECM::ecm` for the core or `ECM::ecm_cont` for drive cycles and route model adapters. Both are available via `find_package(ECM 0.2 CONFIG REQUIRED)` and `add_subdirectory`. [C++ integration and examples](docs/library-guide.md)
- **Python** builds the same C++ targets via `python -m pip install .` and exposes them as `import ecm`. The extension supports Python 3.8 or later. [Python example](docs/library-guide.md#python)

## Design and validation

- [System design](docs/architecture.md) covers the component interfaces, build targets, and simulation flow.
- [Route energy adapters](docs/route-models.md) explain distance-preserving speed ramps and model substitution.
- [Drive-cycle mathematics](docs/drive-cycle.md) covers signed drag, bisection at power transitions, motor maps, Gaussian quadrature, and closed-form checks.
- [Experiment methods](apps/route-lab/docs/model.md) describe paired weather, battery accounting, and confidence intervals.

[MIT License](LICENSE.md)
