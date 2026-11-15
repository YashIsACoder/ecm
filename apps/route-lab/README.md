# Sunswift route lab

Compare pacing policies and energy models on the same simulated weather.
The C++ lab handles routes, weather and battery accounting. [ECM](../../docs/route-models.md)
estimates driving energy. Python runs experiments and analyses paired results.

I adapted this lab from my Sunswift Racing race-strategy work. The supplied
routes are synthetic. The SR7 preset uses public figures where available,
and SR8 is a battery-and-solar scenario. [Vehicle inputs and example results](docs/sunswift-profiles.md)

## Run

Run these commands from the ECM repository root. The lab is part of the
native CMake build and links directly against the C++ library.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DECM_BUILD_PYTHON=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 apps/route-lab/analysis/run_matrix.py build/bin/solar-race \
  --route apps/route-lab/data/rolling-1150.csv --trials 2000 --seed 2025 \
  --out runs/rolling-2025
```

The matrix runs both Sunswift presets through all three backends and writes
CSV results, a paired report, and a manifest with route and binary hashes.
Use `--vehicles`, `--models`, or `--pack-kwh` to vary the experiment.

For one scenario, use the simulator directly.

```sh
mkdir -p runs
build/bin/solar-race --vehicle sr7 --model mapped \
  --route apps/route-lab/data/outback-2025-style.csv --trials 2000 --seed 2025 > runs/sr7.csv
python3 apps/route-lab/analysis/analyse.py runs/sr7.csv
```

The binary embeds the default outback fixture, so it runs from any working
directory without a `--route` argument. Supply `--route PATH` for your own
[route CSV](data/README.md).

## ECM backends

| Backend | Method |
| --- | --- |
| `segment` | Fixed cruising speed with a lumped kinetic-energy change |
| `cycle` | Distance-preserving speed ramps with constant drivetrain efficiency |
| `mapped` | The same speed cycle with bilinear motoring-efficiency interpolation |

Every backend returns duration, DC demand and solar generation.
`Config::energy_model` also accepts a custom C++ estimator. See the
[model and experiment notes](docs/model.md), [benchmark](docs/benchmark.md),
and [reference outback experiment](docs/outback-results.md).

The [system design](../../docs/architecture.md) shows how the lab fits into ECM.
