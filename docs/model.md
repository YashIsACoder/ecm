# Model and experiment

## What is being compared

- **Steady** targets 82 km/h.
- **Push** targets 92 km/h.
- **Reserve-first** chooses 92 or 85 km/h if the current leg's estimated draw leaves at least 35% state of charge. Otherwise it uses 75 km/h. It observes conditions for the current leg and has no future weather. Every policy applies the leg's synthetic pace cap.

A paired trial draws conditions once, then runs all three policies on that draw. This keeps differences attributable to the pacing rules within the model. The example uses independent draws across trials. Cloud and wind share a per-day random component, with smaller leg-to-leg variation. Rolling resistance gets a small independent perturbation at each leg. Route distance, grade and clear-sky irradiance come from the checked-in [synthetic fixture](../data/README.md). The weather generator is a scenario design, with no calibration to historical forecasts.

## Energy and storage

The example begins at 95% state of charge and reports when the minimum falls below 15%. It uses a 225 kg model vehicle, 0.100 m² drag area, 0.0042 rolling coefficient, 92% drive efficiency, and a 6 m² effective solar array with 24% cell and 92% system efficiencies. The values are example assumptions.

The simulator passes distance, grade, wind and speed to the ECM. The ECM calculates drag, rolling, gravity and the net kinetic energy change. It applies fixed drivetrain losses and a regeneration power limit. Solar generation uses the same library and the stage's irradiance. The car starts and stops once per day in the model. After day one it receives a capped parked solar credit, based on a synthetic 2,200 Wh/m² clear-sky exposure scaled by that day's cloud factor.

The storage balance is

```
end = initial + solar generated - net DC demand - spill
```

The implementation stores solar generation and net DC demand separately. Net demand can be negative during regeneration. Any surplus over pack capacity is recorded as spill. When net draw exceeds available charge, the car stops partway through the leg under a constant-average-draw assumption. Reaching the finish at exactly zero charge counts as a completion and breaches reserve.

This model holds cruise and forces constant within each leg, accounting for net changes in speed without simulating acceleration time. Driving hours omit breaks and parked charging. It uses an ideal energy reservoir and omits voltage sag, temperature, battery ageing, exact race rules, traffic and time-of-day feedback. The route and pacing assumptions serve as a reproducible experiment, with real-world use requiring measured inputs and validation.

## Reading the output

Reserve breaches count all trials. Median drive time in the policy table is conditional on completing the route. The paired time comparison uses only the same draws where both policies completed. The 95% Wilson intervals describe Monte Carlo sampling error under the supplied assumptions. They do not capture uncertainty in the synthetic vehicle or scenario generator.
