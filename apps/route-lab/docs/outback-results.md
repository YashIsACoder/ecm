# Outback example results

The checked-in fixture totals 3,020 km across six synthetic stages, with 28 legs. These runs used seed `2025` on Apple Clang 17 with libc++. The values below are reproducible with the commands in the [README](../README.md).

## 5.5 kWh example

Paired weather draws: **2,000**. The reserve threshold is 15% of pack capacity.

| Policy | Completed | 95% completion interval | Breached reserve | Median drive time among finishers |
| --- | ---: | ---: | ---: | ---: |
| steady | 1,505/2,000 (75.2%) | 73.3%–77.1% | 36.4% | 36.86 h |
| push | 228/2,000 (11.4%) | 10.1%–12.9% | 93.5% | 33.15 h |
| reserve-first | 1,573/2,000 (78.6%) | 76.8%–80.4% | 41.8% | 35.13 h |

| Comparison with steady | Completion difference (95% CI) | Reserve-breach difference (95% CI) |
| --- | ---: | ---: |
| push | -63.8 pp (-65.9 to -61.7) | +57.1 pp (+54.9 to +59.3) |
| reserve-first | +3.4 pp (+2.1 to +4.7) | +5.3 pp (+3.9 to +6.8) |

Reserve-first completed **125** trials that steady did not. Steady completed **57** that reserve-first did not.
Across **1,448** joint finishes, the median paired drive-time difference (reserve-first minus steady) was **-109.8 minutes**.

Each interval covers Monte Carlo sampling error under the synthetic assumptions and applies to its own comparison.

## Pack-capacity sensitivity

Both additional runs reused the same seed and initial state-of-charge fraction. Pack mass and all other model parameters were held fixed to isolate the storage constraint.

| Capacity | Steady finishes | Push finishes | Reserve-first finishes | Reserve-first completion difference vs steady (95% CI) |
| --- | ---: | ---: | ---: | ---: |
| 4.5 kWh | 1,220 | 117 | 1,390 | +8.5 pp (+7.0 to +10.0) |
| 5.5 kWh | 1,505 | 228 | 1,573 | +3.4 pp (+2.1 to +4.7) |
| 6.5 kWh | 1,707 | 370 | 1,698 | -0.4 pp (-1.6 to +0.7) |

The reserve-first policy had the most finishes in the two smaller-pack scenarios. Steady had nine more at 6.5 kWh, where the paired completion interval includes zero. Because reserve-first compares predicted charge against a fraction of capacity, changing pack size can change both the available energy and the pace decision.
