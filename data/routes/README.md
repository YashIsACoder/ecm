# Route fixtures

`outback-2025-style.csv` is a synthetic six-stage example following the Darwin to Adelaide corridor. The place names come from the corridor described on the [Bridgestone World Solar Challenge route map](https://worldsolarchallenge.org/about-us/route-map) and its [2025 route notes](https://assets.worldsolarchallenge.org/app/uploads/2025/07/13131527/2025-BWSC-Route-Notes-V1-FINAL-PRINT-Published-13072025.pdf). Distances are rounded for the fixture. The stage boundaries, grades, pace caps and irradiance are constructed assumptions. The pace caps are simulation parameters.

The six required columns may be followed by `control_minutes`, a nonnegative arrival hold used in scheduled simulations. An optional final `rain_day_probability` field stores the probability of a
day with at least 1 mm of precipitation. Six-column files use no holds
and a provisional 5% daily prior. Seven-column files can use either optional
column. CSV exports may use quoted fields, doubled quotes within a name, CRLF line endings and a UTF-8 byte-order mark. Keep each leg on one line. `day` is a contiguous stage identifier starting at 1. `distance_km` is distance from the previous point, `grade_pct` is mean rise per 100 horizontal metres, and `clear_sky_w_m2` is baseline plane-of-array irradiance while driving. Weather draws scale irradiance for each trial.

The outback fixture marks the nine 2025 control towns with 30-minute holds.
The six stage endpoints and time zone use a single synthetic race clock.
[Official program](https://assets.worldsolarchallenge.org/app/uploads/2025/08/04131233/BWSC-2025-Official-Program-v5.pdf) ·
[2025 regulations, §3.27](https://worldsolarchallenge.org/files/3297_2025_bwsc_regulations_release_v20_published_28_october_2024.pdf)

The shipped data contains no telemetry, team weather logs, or measured Sunswift vehicle parameters.

`rolling-1150.csv` is a shorter, invented 1,150 km route over three stages.
It exercises grade changes, speed changes, control holds and overnight solar charging.

## August rain inputs

The Darwin–Adelaide fixture uses the Bureau of Meteorology's *mean number
of days of rain ≥1 mm* in August divided by 31. August represents the
2025 edition's late-winter timing.
[BWSC date announcement](https://worldsolarchallenge.org/latest-news/perfect-timing-to-accelerate-towards-green-energy)

| Representative station | August wet days |
| --- | ---: |
| [Darwin Airport](https://www.bom.gov.au/climate/averages/tables/cw_014015.shtml) | 0.3 |
| [Tindal RAAF](https://www.bom.gov.au/climate/averages/tables/cw_014932.shtml) | 0.1 |
| [Tennant Creek Airport](https://www.bom.gov.au/climate/averages/tables/cw_015135.shtml) | 0.4 |
| [Alice Springs Airport](https://www.bom.gov.au/climate/averages/tables/cw_015590.shtml) | 1.2 |
| [Coober Pedy Airport](https://www.bom.gov.au/climate/averages/tables/cw_016090.shtml) | 1.5 |
| [Port Augusta Aero](https://www.bom.gov.au/climate/averages/tables/cw_018201.shtml) | 3.7 |
| [Adelaide Airport](https://www.bom.gov.au/climate/averages/tables/cw_023034.shtml) | 10.7 |

Legs use representative stations along the course. Port Wakefield uses the
midpoint of Port Augusta and Adelaide's rates. Station coverage and record
lengths vary. [Exposure model](../../apps/route-lab/docs/forecast.md#precipitation-and-road-state)
