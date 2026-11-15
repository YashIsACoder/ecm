# Route fixtures

`outback-2025-style.csv` is a synthetic six-stage example following the Darwin to Adelaide corridor. The place names come from the corridor described on the [Bridgestone World Solar Challenge route map](https://worldsolarchallenge.org/about-us/route-map) and its [2025 route notes](https://assets.worldsolarchallenge.org/app/uploads/2025/07/13131527/2025-BWSC-Route-Notes-V1-FINAL-PRINT-Published-13072025.pdf). Distances are rounded for the fixture. The stage boundaries, grades, pace caps and irradiance are constructed assumptions. The pace caps are simulation parameters.

The file uses a header and six comma-separated fields per row. Names occupy one field and cannot contain commas. `day` is a contiguous stage identifier starting at 1. `distance_km` is distance from the previous point, `grade_pct` is mean rise per 100 horizontal metres, and `clear_sky_w_m2` is baseline plane-of-array irradiance while driving. Weather draws scale irradiance for each trial.

The shipped data contains no telemetry, team weather logs, or measured Sunswift vehicle parameters.

`rolling-1150.csv` is a shorter, invented 1,150 km route over three stages.
It exercises grade changes, speed changes and overnight solar charging.
