# Run a BWSC-style race simulation

The example compares three SR7 pacing policies on paired weather draws over
the Darwin–Adelaide corridor. It uses a 2025-style itinerary with published
control locations, synthetic leg inputs, and an August rainfall prior.
[Route format and station inputs](../data/routes/README.md) ·
[SR7 parameter sources](../apps/route-lab/docs/sunswift-profiles.md)

## Build and run

From the repository root, build and install the library, then build the
standalone example against the installed package.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DECM_BUILD_PYTHON=OFF
cmake --build build --parallel
cmake --install build --prefix "$PWD/build/install"

cmake -S examples/bwsc -B build/bwsc-example \
  -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/bwsc-example --parallel
build/bwsc-example/bwsc-simulation
```

The [example source](../examples/bwsc/main.cpp) links `ECM::lab`, prepares
an energy envelope, samples 100 paired forecasts and prints finish and
reserve statistics. To try another itinerary, pass a compatible CSV path
as the first argument.

For the CLI and exported traces, run:

```sh
build/bin/solar-race --vehicle sr7 --model gust --gust-sd-mps 2 \
  --route data/routes/outback-2025-style.csv --weather posterior \
  --rain-multiplier 1 --bwsc-2025-timing --reserve-strategy envelope \
  --reserve-target-pct 15 --reserve-slow-kmh 65 --trials 500 \
  --seed 2025 --trace-trial 15 > trials.csv 2> trace.csv
python3 apps/route-lab/analysis/analyse.py trials.csv
```

Both entry points use the SR7 Cruiser energy rules. The default pack is
55 MJ, and the 1,500 W battery-side charger can fill the pack during a
normal overnight stop. Pass `--overnight-charge-w` or set
`Config::overnight_charge_power_w` to test a different supply. The trace
records `external_charge_wh` on the next departure. The trial CSV reports
`external_charge_kwh` separately from solar.
[SR7 inputs and physics check](../apps/route-lab/docs/sunswift-profiles.md)

The two entry points use their own example settings. The native example
uses an explicit planning forecast and 17 speed samples. The CLI has
nine samples and the library's default planning forecast.

## Choose the algorithms

| Header | Entry point | Use |
| --- | --- | --- |
| `<ecm/lab.hpp>` | `read_route`, `default_route` | Load an itinerary with stage boundaries, pace caps, control holds and optional wet-day priors |
| `<ecm/lab.hpp>` | `route_energy_model` | Select `segment`, `cycle`, `mapped` or `gust`. A C++ caller can also supply a custom estimator through `Config::energy_model` |
| `<ecm/lab.hpp>` | `PreparedRace` | Compute the backward charge targets once and reuse them with many weather draws |
| `<ecm/lab.hpp>` | `PreparedRace::simulate`, `run_experiment` | Run on supplied conditions or stream paired Monte Carlo results |
| `<ecm/forecast.hpp>` | `calibrate_forecast`, `draw_forecast_weather`, `draw_rain` | Calibrate biases to caller-supplied residuals and generate correlated conditions |
| `<ecm/bwsc.hpp>` | `bwsc_2025_stage_timing` | Compute the daily cutoff, late time, penalty and next start for one planned stage |
| `<ecm/lab_statistics.hpp>` | `wilson95`, `PairedOutcomes` | Estimate risk and paired differences as results stream through the caller |

`PreparedRace` owns its itinerary and model. When you supply a planning
forecast, use one raw `Weather` value per leg in route order. The plan uses
that forecast for future-leg charge targets and the next leg's parked solar
at day breaks. Pass a separate weather draw to `simulate`. The controller
uses the draw's current-leg conditions to pick pace. Inspect the target at
each checkpoint through `arrival_targets_wh()`, in watt-hours, or read the
chosen speed and arrival target from a `LegSink`. Targets can exceed the
pack capacity when the forecast itinerary is infeasible.
[Pacing algorithm](../apps/route-lab/docs/pacing.md) ·
[Weather conventions](../apps/route-lab/docs/forecast.md)

If comparing policies or models on a custom forecast ensemble, pass the
same observed `Weather` array to each prepared plan for that draw. Add one
pair of completion flags or reserve-breach flags per draw to
`PairedOutcomes`. Its interval estimates sampling error conditional on the
scenario. [Newcombe's paired score interval](https://pubmed.ncbi.nlm.nih.gov/9839354/)

## Model the planned race clock

Enable `race_schedule` for CSV control holds and stage breaks. Selecting
`StageTiming::bwsc_2025` carries lateness from each planned overnight
arrival into the next day's start. The cutoff is 17:00. The first 10
minutes of lateness accrue one minute of delay each, then two per minute.
For example, finishing the planned Day 2 stage at 17:13 yields a Day 3
start at 08:16. The native trace and CLI results report penalty hours.
[2025 BWSC regulations, §3.21](https://worldsolarchallenge.org/files/3297_2025_bwsc_regulations_release_v20_published_28_october_2024.pdf)

The simulator follows the stage assignments supplied in the route. Timings
estimate carry-over under §3.21.4. Actual race planning also depends on
local sunset, permitted stopping locations, control opening windows, and
driver-change rules. The model treats each control hold as a single
interval, so use the applicable event rules for holds straddling the
daily cutoff.
