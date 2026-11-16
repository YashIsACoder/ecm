# Weather calibration and posterior prediction

The Route Lab has two synthetic weather modes. The baseline draws a daily
solar factor and headwind, then applies independent leg-level variation.
The posterior predictive mode uses calibration residuals and correlated
innovations. Both return the same `Weather` record for use with any ECM
estimator.

## Gaussian calibration

The fixture contains 12 paired synthetic residuals in clear-sky factor and
metres per second. Positive wind residuals increase headwind. Each coordinate
uses a zero-mean normal prior and an independent normal residual likelihood
centred at the unknown bias. Prior SDs are 0.18 and 3.0 m/s. Observation SDs are
0.13 and 1.8 m/s.

For one coordinate with prior SD `s`, observation SD `t` and residuals `r`,
the posterior variance is `v = 1 / (1/s² + n/t²)` and the mean is
`v × sum(r) / t²`. The CLI reports this solution alongside the chain moments.

The C++ sampler runs four symmetric Gaussian random-walk Metropolis chains
from ±2.5 posterior SDs across the two coordinates. Proposals use 1.7 posterior
SDs per coordinate. A proposal is accepted when
`log(u) < log posterior(proposal) − log posterior(current)` for uniform `u`.
Each chain discards 400 iterations and retains 1,600. Classical split R̂ and
post-warm-up acceptance are computed at full resolution. The browser plots
every eighth retained state and exposes an iteration scrubber.
[Metropolis–Hastings](https://doi.org/10.1093/biomet/57.1.97) ·
[Stan reference on chain diagnostics](https://mc-stan.org/docs/2_40/reference-manual/analysis.html)

## Predictive routes

Each trial resamples one joint bias from the retained states with replacement.
Race draws from a seeded generator use stationary normal AR(1) deviations with
coefficients 0.55 across days and 0.72 across legs. Solar and wind have
independent deviations in this fixture. Rolling resistance keeps its own
bounded, independent perturbation. Leg irradiance applies the solar factor to
the fixture's clear-sky input. The daily cloud state also scales the parked
solar credit.

The three policies receive identical weather within each trial. The outer
Monte Carlo intervals quantify finite-sample variation conditional on the
fitted synthetic generator. They exclude calibration-data quality and
vehicle-model uncertainty. The weather is constant within a leg and the route
clock uses one schedule without time-zone transitions.

`ecm::lab::calibrate_forecast` also accepts caller-supplied bias residuals.
The checked-in example uses synthetic residuals and provisional values for
noise and persistence. Forecasts for an actual race would require measured
residuals, independent calibration and forecast verification.

## Precipitation and road state

Each outback leg has an August wet-day prior from a nearby Bureau of
Meteorology station. The fixture stores the station's mean number of August
days with at least 1 mm of rain divided by 31. These are climate averages.
[Station inputs and mapping](../../../data/routes/README.md)

An independent seeded stream evolves a stationary Gaussian front with
correlation `exp(−d/180 km)` between adjacent leg midpoints. This is an
assumed spatial correlation length. The Gaussian tail is uniform under
the marginal prior. Compare that tail to the probability of rain during
the leg's reference exposure:

```text
exposure_days = (leg_km / 80 km/h) / 24 h
p_leg = 1 − (1 − p_wet_day)^(exposure_days × rain_exposure)
```

This conversion assumes a constant occurrence hazard within a day and a
reference pace independent of strategy. A wet leg uses an illustrative
lognormal rain rate, clipped to 0.25–20 mm/h. Rain rate and correlation
are scenario assumptions. Rain exposure 1× applies the route prior, 0×
is dry, and higher settings up to 8× increase the chance of a shower.
A matched seed gives every policy the same conditions.

In a 10,000-draw check on the bundled route with seed 2025, 0.38% of modeled
legs were wet at 1× and 2.91% at 8×. At least one shower occurred in 8.94%
and 50.52% of complete route draws respectively. These are simulation
frequencies on one build. They quantify the effect of the scenario knob.

Road wetness follows `dW/dx = (W_eq − W) / L` with
`W_eq = rain / (4 + rain)`. Each leg applies the exact exponential update.
`L` is 35 km in rain and 115 km when dry. The leg uses the mean of its entry
and exit wetness. This route-coordinate approximation carries wetness across
checkpoints and avoids treating driver pace as a weather input.

A wet road multiplies rolling resistance by `1 + 0.14W` and reduces the pace
cap by `1 − 0.14W`. Rain multiplies driving and parked irradiance by
`1 − 0.35 × rain/(5 + rain)` after the scenario solar adjustment.
The leg trace records rain probability, rainfall, wetness and effective pace cap.
The road and irradiance response coefficients are synthetic scenario values. The browser's sky labels describe attenuation
and rain in the current simulated leg.
