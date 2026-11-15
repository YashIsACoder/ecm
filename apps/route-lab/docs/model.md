# Model and experiment

## What is being compared

- **Steady** targets 82 km/h.
- **Push** targets 92 km/h.
- **Reserve-first** chooses 92 or 85 km/h if the current leg's estimated draw leaves at least 35% state of charge. Otherwise it uses 75 km/h. It observes conditions for the current leg and has no future weather. Every policy applies the leg's synthetic pace cap.

A paired trial draws conditions once, then runs all three policies on that draw. This keeps differences attributable to the pacing rules within the model. The example uses independent draws across trials. Cloud and wind share a per-day random component, with smaller leg-to-leg variation. Rolling resistance gets a small independent perturbation at each leg. Route distance, grade and clear-sky irradiance come from the checked-in [synthetic fixture](../../../data/routes/README.md). Choose baseline weather or the [posterior predictive model](forecast.md). Both use synthetic assumptions.

## Energy and storage

Each preset begins at 95% state of charge and reports when the minimum falls below 15%. The original reference experiment uses a 225 kg model vehicle, 0.100 m² drag area, 0.0042 rolling coefficient, 92% drive efficiency, and a 6 m² effective solar array with 24% cell and 92% system efficiencies. The [Sunswift presets](sunswift-profiles.md) have separate inputs.

The simulator passes distance, grade, wind, speed and irradiance to the ECM. The segment backend lumps the change in kinetic energy into steady cruise. The cycle backends resolve the speed changes using fixed acceleration and braking rates. They differ in how they estimate motoring losses. The pace cap limits each leg's cruise target. If a leg begins slower or faster than its target, its speed ramp begins at the previous leg's exit speed. The car starts and stops at stage boundaries and at enabled control holds. After day one it receives a capped parked solar credit, based on a synthetic 2,200 Wh/m² exposure scaled by that day's cloud factor.

The storage balance is

```
end = initial + solar generated - net DC demand - spill
```

The implementation stores solar generation and net DC demand separately. Net demand can be negative during regeneration. Any surplus over pack capacity is recorded as spill. When net draw exceeds available charge, the car stops partway through the leg under a constant-average-draw assumption. Reaching the finish at exactly zero charge counts as a completion and breaches reserve.

Battery charge and depletion are evaluated with mean net power within each leg, including when a cycle backend resolves shorter ramps for energy estimation. Driving hours omit breaks and parked charging. The scheduled clock adds
control holds and schedules the next day from 08:00 or the most recent release,
whichever is later. Control holds add no energy. The model credits aggregate
parked solar on the next departure, and replay interpolates that credit across
the parking interval. The lab uses an ideal energy reservoir. Real-world use requires measured inputs and validation for effects such as battery temperature, voltage sag and traffic.

## Reading the output

Reserve breaches count all trials. Median driving time and scheduled race time are conditional on completing the route. The paired time comparison uses only the draws where both policies completed.

The policy table uses 95% Wilson intervals for completion rates. The comparison table uses [Newcombe's method 10](https://pubmed.ncbi.nlm.nih.gov/9839354/) for paired proportions, keeping the weather draw as the sampling unit. A positive reserve-breach difference means the comparison policy went below reserve more often. Each interval has its own nominal 95% coverage. These intervals quantify Monte Carlo sampling error under the supplied assumptions. The synthetic vehicle and scenario generator introduce additional uncertainty.
