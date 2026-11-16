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
| Gust-aware cycle | Average mapped energy across variable wind loads |

## Play the route lab

From the repository root, use CMake 3.20 or later and a C++20 compiler.
Python 3.9 or later runs the analysis scripts.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DECM_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 apps/route-lab/serve.py build/bin/solar-race
```

Open `http://127.0.0.1:8765` to change the vehicle, route, battery and model,
then replay one weather draw and inspect the C++ leg logs. The browser shows
finish rates, reserve risk and how far each pacing policy gets. A static
homepage version replays a recorded run. [Lab guide and homepage integration](apps/route-lab/README.md)

The routes are synthetic. SR7 defaults to the 2025 Cruiser limit of 55 MJ
and models external charging overnight. SR8 is a provisional
battery-and-solar scenario.
[SR7 and SR8 inputs and results](apps/route-lab/docs/sunswift-profiles.md)

## Use the library

- **C++** exports `ECM::ecm` for the core, `ECM::ecm_cont` for drive cycles, and `ECM::lab` for routes, weather, strategy and experiments. [C++ integration and examples](docs/library-guide.md) · [BWSC simulation walkthrough](docs/bwsc-simulation.md)
- **Python** builds the same C++ targets via `python -m pip install .` and exposes them as `import ecm`. The extension supports Python 3.8 or later. [Python example](docs/library-guide.md#python)

## Design and validation

- [System design](docs/architecture.md) covers the component interfaces, build targets, and simulation flow.
- [Route energy adapters](docs/route-models.md) explain distance-preserving speed ramps and model substitution.
- [Drive-cycle mathematics](docs/drive-cycle.md) covers signed drag, bisection at power transitions, motor maps, Gaussian quadrature, and closed-form checks.
- [Experiment methods](apps/route-lab/docs/model.md) describe paired weather, battery accounting, and confidence intervals.

[MIT License](LICENSE.md)
