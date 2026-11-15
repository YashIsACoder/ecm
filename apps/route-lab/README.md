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

Open `http://127.0.0.1:8765` and expand **Experiment setup** to choose the
route, ECM variant, battery, speed targets, solar array, and weather stressors.
**Run experiment** returns to the map. **Race outcomes** opens the same run's
finish and reserve intervals, reach chart, battery history, and speed history.

Choose 0.5×, 1×, 2× or 4× playback. **Race highlights** gives control holds more
screen time and speeds through shared overnight parking. **Uniform time scale**
plays the simulated clock at a constant rate. **Next REST** jumps to the
selected strategy's next 30-minute control stop.

Select a strategy to see mean leg speed, target speed, generated solar power,
irradiance, wind, rain, wetness and wet-road pace cap. As a leg completes, the
native trace exposes its wheel work, DC and battery balance. Position and charge
interpolate using native leg endpoints.
[BWSC corridor](https://worldsolarchallenge.org/about-us/route-map) ·
[Natural Earth coastline](https://www.naturalearthdata.com/about/terms-of-use/)

**Save run setup** exports versioned JSON with settings, CLI arguments and a
route SHA-256. Use **Load setup JSON** in the setup panel to rerun the case. From
the repository root the exported `arguments` array can also be passed directly
to `solar-race` using the same build. The local HTTP adapter only serves bundled
route fixtures. Supply any compatible [route CSV](../../data/routes/README.md)
with the native CLI.

See the [forecast model](docs/forecast.md) for MCMC and rain, and the
[pacing algorithm and comparison](docs/pacing.md) for the route energy envelope.

## Use the demo on a homepage

Copy the contents of [`web/`](web/) into a static site directory and link to
that directory with `?replay=1`. The page loads a recorded native SR7 experiment
with interactive playback and a separate analytics page. The selected draw
shows a rainy control stop, a finish and pack depletion. New experiments run through the local lab.
To regenerate the recorded data after rebuilding, run

```sh
python3 apps/route-lab/serve.py build/bin/solar-race \
  --export-demo apps/route-lab/web/demo.json
build/bin/solar-race --forecast-report --seed 2025 > apps/route-lab/web/forecast-demo.json
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
  --route data/routes/outback-2025-style.csv --trials 2000 --seed 2025 \
  --race-schedule --weather posterior --rain-multiplier 0.65 \
  --reserve-strategy envelope --reserve-target-pct 15 --reserve-slow-kmh 65 > runs/sr7.csv
python3 apps/route-lab/analysis/analyse.py runs/sr7.csv
```

Add `--trace-trial 15` to write one draw's leg log to stderr while the regular
CSV stays on stdout. Redirect stderr to a trace file when working from a shell.
`--race-schedule` adds control and overnight waits to a separate elapsed-time
column. The default CLI uses baseline weather, dry roads and driving-time
results. The browser defaults to posterior weather with rain intensity 0.65.

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

[Résumé and personal-site copy](docs/portfolio.md)
