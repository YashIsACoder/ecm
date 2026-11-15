# Sunswift scenarios

| Input | SR7 | SR8 |
| --- | ---: | ---: |
| Effective mass used in the lab | 500 kg | 850 kg |
| Drag coefficient / frontal area | 0.095 / 1.40 m² | 0.18 / 1.55 m² |
| Rolling coefficient / effective wheel radius | 0.005 / 0.27 m | 0.007 / 0.31 m |
| Auxiliary draw | 150 W | 250 W |
| Usable battery capacity | 38 kWh | 45 kWh |
| Fixed motoring / regeneration efficiency | 0.91 / 0.60 | 0.91 / 0.62 |
| Regenerative power cap | 2.5 kW | 4.0 kW |
| PV area / efficiency / system factor | 4.8 m² / 0.22 / 0.92 | 4.5 m² / 0.23 / 0.92 |

UNSW published the SR7 race-preparation mass and drag coefficient. A
separate UNSW-hosted technical presentation describes an SR7 array with
4.8 m² of modules at roughly 22% efficiency. The lab treats the other SR7
values, including usable pack capacity, as scenario assumptions.
[UNSW SR7 race profile](https://www.unsw.edu.au/newsroom/news/2025/08/Sunswift-Bridgestone-World-Solar-Challenge-2025) ·
[UNSW-hosted PV seminar](https://www2.pv.unsw.edu.au/videos/John-Rodriguez-11April2025/slides/John-Rodriguez-The-Realities-of-Vehicle-Integrated-PV.pdf)

All numerical SR8 inputs are provisional. UNSW describes SR8 as a
solar, battery and hydrogen fuel-cell design. The lab's scenario covers
its battery and solar energy only. Both presets use the same illustrative
motoring map in mapped mode. [UNSW Sunswift project](https://www.unsw.edu.au/challeng/vertically-integrated-projects/explore-vertically-integrated-projects/sunswift-racing)

## Example comparison

Paired runs used 2,000 draws, seed `2025`, the reserve-first policy and
an Apple Clang 17 / libc++ build.

| Scenario and route | Segment finishes | Cycle finishes | Mapped finishes |
| --- | ---: | ---: | ---: |
| SR7, 3,020 km outback fixture | 781 | 758 | 696 |
| SR8, 1,150 km rolling fixture | 595 | 585 | 675 |

The mapped model changed SR7 completion by −4.2 percentage points
relative to segment (95% paired interval −5.2 to −3.4) and SR8 by +4.0
points (+3.1 to +5.0). These are sensitivity results for two synthetic
experiments. They show that changing an efficiency assumption can shift
completion near a battery limit. [Reproduction commands](../README.md)
