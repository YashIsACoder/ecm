# Sunswift route lab

Compare pacing policies and energy models on the same simulated weather.
The C++ lab handles routes, weather and battery accounting. [ECM](https://github.com/YashIsACoder/ecm/tree/interchangeable-route-models)
estimates driving energy. Python runs experiments and analyses paired results.

I adapted this lab from my Sunswift Racing race-strategy work. The supplied
routes are synthetic. The SR7 preset uses public figures where available,
and SR8 is a battery-and-solar scenario. [Vehicle inputs and example results](docs/sunswift-profiles.md)

## Run

Place a checkout of private ECM 0.2 beside this repository or set
`ECM_SOURCE_DIR` to its path. An installed ECM package also works via
`CMAKE_PREFIX_PATH`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 analysis/run_matrix.py build/solar-race \
  --route data/rolling-1150.csv --trials 2000 --seed 2025 \
  --out runs/rolling-2025
```

The matrix runs both Sunswift presets through all three backends and writes
CSV results, a paired report, and a manifest with route and binary hashes.
Use `--vehicles`, `--models`, or `--pack-kwh` to vary the experiment.

For one scenario, use the simulator directly.

```sh
mkdir -p runs
./build/solar-race --vehicle sr7 --model mapped \
  --route data/outback-2025-style.csv --trials 2000 --seed 2025 > runs/sr7.csv
python3 analysis/analyse.py runs/sr7.csv
```

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

GitHub Actions pins ECM and uses the `ECM_READ_TOKEN` repository secret
to read that private dependency.
