# Route energy envelope

Steady and Push use fixed cruise targets. Reserve-first offers speed tiers
or an energy envelope. The default tier rule tries 92, 85 and 75 km/h, using
a 35% planning target. The envelope uses the configured upper and lower
speeds, and the higher of the planning target and the reporting reserve.
Every candidate is subject to the leg's effective pace cap.

## Backward pass

For leg i, let A[i] be the required charge at arrival and D[i] the required
charge at departure. The terminal requirement is the chosen reserve R.
For each leg, from finish to start:

1. Set A[i] to max(R, D[i+1] − C[i+1]), where C is the nominal overnight
   solar credit before the next leg. Apply the credit only at a stage
   boundary and when the next departure target fits in the pack.
2. Compute the smallest net draw E[i] across a grid of candidate speeds and set
   D[i] to max(R, A[i] + E[i]). At normal waypoints the nominal entry
   speed is the candidate speed. At starts, overnights and controls it is zero.

The backward pass uses the selected ECM backend and a default dry planning
forecast with 70% of clear-sky irradiance, 3 m/s headwind, and a 65%
parked-solar factor. A caller can prepare a plan with a forecast per leg
and set the speed grid size through `StrategyConfig`. Weather adjustments
apply to that planning forecast and the observed simulations. If the
Cruiser charger can refill the pack in a nominal 15-hour night, the envelope
uses the reserve target at the end of each day. Otherwise it includes the
limited charging budget in the backward pass.
[Prepared race API](../../../docs/bwsc-simulation.md)
A target can exceed usable capacity. The trace exposes that infeasibility.

## Current-leg decision

At departure, the controller uses the available state of charge, sampled
conditions on that leg, and actual entry speed. It chooses the fastest of
the evenly spaced candidates whose capacity-clamped arrival charge meets
A[i]. If none qualifies, it picks the candidate with the smallest net
draw. This search evaluates grid candidates directly because regeneration,
auxiliary draw and solar exposure can make net draw non-monotonic in speed.

The controller shares an immutable O(L) envelope across the experiment.
The setup costs O(LK) estimator calls, with L legs and K candidate paces. The default grid uses nine samples.
Each simulated envelope leg takes up to K calls and reuses the chosen energy
estimate for the battery update. The heuristic uses the prepared reference
forecast for future legs. Actual sampled rain and wind enter the decision
when the car reaches the leg. It makes no claim of optimal travel time or
confidence guarantees under real weather.
[Top Dutch Solar Racing strategy study](https://www.cambridge.org/engage/miir/article-details/662cb561418a5379b0db608a)

## Example comparison

The native release executable ran 500 paired draws of the synthetic
SR7 Cruiser scenario. Both runs used the same seed, motor map, 65–92 km/h
bounds, 55 MJ pack and 15% charge target. The table shows the Reserve-first
row when configured to use tiers or the envelope.

| Rain exposure | Pacing rule | Finishes | Below 15% | Median external charge |
| --- | --- | ---: | ---: | ---: |
| 0 | Tiers | 442/500 | 313/500 | 48.59 kWh |
| 0 | Envelope | 499/500 | 132/500 | 49.62 kWh |
| 1 | Tiers | 441/500 | 313/500 | 48.52 kWh |
| 1 | Envelope | 499/500 | 132/500 | 49.62 kWh |

At the August prior, the finish-rate difference is 11.6 percentage points
with a 95% paired interval of 8.9 to 14.7 points. The comparison uses
synthetic weather and a fixed stage itinerary. Each external energy
median includes unfinished trials.

Reproduce from the repository root with the Release binary:

```sh
for rain in 0 1; do
  for rule in tiers envelope; do
    build/bin/solar-race --vehicle sr7 --model mapped --bwsc-2025-timing \
      --route data/routes/outback-2025-style.csv --weather posterior \
      --seed 2025 --trials 500 --reserve-target-pct 15 --reserve-slow-kmh 65 \
      --rain-multiplier "$rain" --reserve-strategy "$rule" > "${rule}-${rain}.csv"
  done
done
```

Standard-library normal draws can differ between toolchains.
