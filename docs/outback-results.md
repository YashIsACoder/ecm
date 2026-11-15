# Outback example results

The checked-in fixture totals 3,020 km across six synthetic stages, with 28 legs. These runs used seed `2025` on Apple Clang 17 with libc++. The values below are reproducible with the commands in the [README](../README.md).

## 5.5 kWh example

Paired weather draws: **2,000**. The reserve threshold is 15% of pack capacity.

| Policy | Completed | 95% completion interval | Breached reserve | Median drive time among finishers |
| --- | ---: | ---: | ---: | ---: |
| steady | 1,505/2,000 (75.2%) | 73.3%–77.1% | 36.4% | 36.86 h |
| push | 228/2,000 (11.4%) | 10.1%–12.9% | 93.5% | 33.15 h |
| reserve-first | 1,573/2,000 (78.6%) | 76.8%–80.4% | 41.8% | 35.13 h |

Reserve-first completed **125** trials that steady did not. Steady completed **57** that reserve-first did not.
Across **1,448** joint finishes, the median paired drive-time difference (reserve-first minus steady) was **-109.8 minutes**.

The interval covers sampling error under this model. The vehicle and weather assumptions are synthetic.

## Pack-capacity sensitivity

Both additional runs reused the same seed and initial state-of-charge fraction. Pack mass and all other model parameters were held fixed to isolate the storage constraint.

| Capacity | Steady finishes | Push finishes | Reserve-first finishes |
| --- | ---: | ---: | ---: |
| 4.5 kWh | 1,220 | 117 | 1,390 |
| 5.5 kWh | 1,505 | 228 | 1,573 |
| 6.5 kWh | 1,707 | 370 | 1,698 |

The reserve-first policy had the most finishes in the two smaller-pack scenarios, while steady had nine more at 6.5 kWh. Because reserve-first compares predicted charge against a fraction of capacity, changing pack size can change both the available energy and the pace decision.
