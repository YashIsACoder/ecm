# Portfolio copy

## Résumé

- Built a C++20 solar-race simulator adapted from Sunswift strategy work,
  comparing configurable pacing policies over paired Monte Carlo weather draws
  and reporting finish risk, battery reserve, and per-leg energy diagnostics.

## Personal site

**ECM Route Lab** explores how speed, cloud, wind, and wet roads change the
energy budget of a solar car racing from Darwin to Adelaide. I adapted the
project from my Sunswift strategy work and built the public-facing demo around
synthetic SR7 and SR8 scenarios. Its C++ engine evaluates alternate pacing
rules on matched weather draws, and the browser can replay the native trace
through control stops and overnight charging.

For a technical walkthrough, show the 30-minute control hold, open the
methods page to inspect the posterior chains, then compare the route energy
envelope with fixed speed tiers. The [benchmark and reproduction](pacing.md)
include the finish-rate change, paired interval, and driving-time trade-off.

- [Embed the static replay](../README.md#use-the-demo-on-a-homepage)
- [Weather model](forecast.md)
- [Library architecture](../../../docs/architecture.md)
