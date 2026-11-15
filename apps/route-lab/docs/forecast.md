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
