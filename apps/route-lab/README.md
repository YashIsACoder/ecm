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
python3 apps/route-lab/serve.py build/bin/solar-race
```

Open `http://127.0.0.1:8765`. Run an experiment, then play or scrub the
selected draw. The cars share a driving-time clock and split when their
strategies choose different paces. Select a car's strategy to see its latest
leg's wheel work, DC and battery balance, and native log. Download either the
trial CSV or the leg trace.

The route marker and state of charge are interpolated between native leg
observations for replay. The clock excludes parked time and applies each parked
solar credit at the start of the next driving leg. The outback map connects
approximate checkpoints. Rolling country uses a schematic.
[Bridgestone World Solar Challenge corridor](https://worldsolarchallenge.org/about-us/route-map) ·
[Natural Earth coastline](https://www.naturalearthdata.com/about/terms-of-use/)

Finish and reserve-breach rates show 95% Wilson intervals. Expand the trial
diagnostics to see paired intervals versus Steady, alongside median distance,
charge, demand, solar and spill.

## Use the demo on a homepage

Copy the contents of [`web/`](web/) into a static site directory and link to
that directory with `?replay=1`. The page loads a recorded native SR7 experiment
with interactive playback. New experiments are available through the local lab.
To regenerate the recorded data after rebuilding, run

```sh
python3 apps/route-lab/serve.py build/bin/solar-race \
  --export-demo apps/route-lab/web/demo.json
```

The lab's dark-green palette takes its cue from the team's public presentation
of Sunswift 8. [Sunswift Racing](https://www.sunswift.com/)

## Batch experiments

```sh
python3 apps/route-lab/analysis/run_matrix.py build/bin/solar-race \
  --route data/routes/rolling-1150.csv --trials 2000 --seed 2025 \
  --out runs/rolling-2025
```

The matrix runs both Sunswift presets through all three backends and writes
CSV results, a paired report, and a manifest with route and binary hashes.
Use `--vehicles`, `--models`, or `--pack-kwh` to vary the experiment.

For one scenario, use the simulator directly.

```sh
mkdir -p runs
build/bin/solar-race --vehicle sr7 --model mapped \
  --route data/routes/outback-2025-style.csv --trials 2000 --seed 2025 > runs/sr7.csv
python3 apps/route-lab/analysis/analyse.py runs/sr7.csv
```

Add `--trace-trial 6` to write one draw's leg log to stderr while the regular
CSV stays on stdout. Redirect stderr to a trace file when working from a shell.

The binary embeds the default outback fixture, so it runs from any working
directory without a `--route` argument. Supply `--route PATH` for your own
[route CSV](../../data/routes/README.md).

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
