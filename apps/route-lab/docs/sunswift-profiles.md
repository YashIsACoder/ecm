# Sunswift scenarios

| Input | SR7 · 2025 Cruiser | SR8 · provisional |
| --- | ---: | ---: |
| Effective driving mass | 660 kg | 850 kg |
| Drag coefficient / frontal area | 0.095 / 1.40 m² | 0.18 / 1.55 m² |
| Rolling coefficient / effective wheel radius | 0.005 / 0.27 m | 0.007 / 0.31 m |
| Auxiliary draw | 150 W | 250 W |
| Default battery capacity | 55 MJ (about 15.28 kWh) | 45 kWh |
| Initial charge | 100% | 95% |
| External overnight charging | 1.5 kW battery-side default | — |
| Fixed motoring / regeneration efficiency | 0.91 / 0.60 | 0.91 / 0.62 |
| Regenerative power cap | 2.5 kW | 4.0 kW |
| PV area / efficiency / system factor | 4.8 m² / 0.22 / 0.92 | 4.5 m² / 0.23 / 0.92 |

The SR7 driving mass treats UNSW's published 500 kg as the vehicle mass and
adds two 80 kg ballasted occupants. The drag coefficient is published.
A UNSW-hosted technical presentation describes an SR7 solar array
with 4.8 m² of modules at roughly 22% efficiency. Frontal area, tyres,
auxiliary draw, the motor map and battery-side charger power are scenario
assumptions.
[UNSW SR7 race profile](https://www.unsw.edu.au/newsroom/news/2025/08/Sunswift-Bridgestone-World-Solar-Challenge-2025) ·
[UNSW-hosted PV seminar](https://www2.pv.unsw.edu.au/videos/John-Rodriguez-11April2025/slides/John-Rodriguez-The-Realities-of-Vehicle-Integrated-PV.pdf)

The 55 MJ cap is the 2025 Cruiser nominal storage limit. The default
assumes the full nominal capacity is usable. Use `--pack-kwh` or
`Config::capacity_wh` to model a smaller usable window. The rulebook
allows external charging between driving days. After crediting parked
solar, the model takes at most the smaller of the remaining pack capacity
and charger power times parked hours. A pack depleted on a leg ends the
trial. [2025 BWSC regulations, §§2.5–2.8 and 3.19](https://worldsolarchallenge.org/files/3297_2025_bwsc_regulations_release_v20_published_28_october_2024.pdf)

All numerical SR8 inputs are provisional. UNSW describes SR8 as a
solar, battery and hydrogen fuel-cell design. This scenario covers
battery and solar only. [UNSW Sunswift project](https://www.unsw.edu.au/challeng/vertically-integrated-projects/explore-vertically-integrated-projects/sunswift-racing)

## Physics check

At 82 km/h on a flat road without headwind or solar, the SR7 fixed-efficiency
inputs imply 2.41 kWh per 100 km of DC demand. This comes from wheel work
against aerodynamic drag and rolling resistance, divided by motoring
efficiency, plus 150 W of auxiliaries over the drive time. The gust-aware
mode integrates wind load around the forecast headwind.
[Route estimator equations](../../../docs/route-models.md)

The recorded draw illustrates the daily energy budget. On Day 4 Steady
reaches the overnight stop with 1.6% charge. Push depletes on the same stage.
Reserve-first reaches that overnight stop with 11.1%. These are outputs of
the synthetic itinerary and the published-input scenario. Matching observed
race traces requires usable pack, vehicle load and weather telemetry.

## Model sensitivity

All four models ran the same 500 August-prior draws with seed 2025, timed
stages and a 15% reserve target for the energy envelope. This compares
finish counts from the recorded lab configuration.

| Estimator | Steady | Push | Reserve-first |
| --- | ---: | ---: | ---: |
| Segment | 464 | 261 | 499 |
| Cycle | 462 | 255 | 499 |
| Mapped | 471 | 265 | 499 |
| Gust-aware | 471 | 263 | 499 |

The output moves with the modelling assumptions, particularly for the Push
policy near the daily energy limit. [Reproduction commands](../README.md)
