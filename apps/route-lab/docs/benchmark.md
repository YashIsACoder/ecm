# Simulator benchmark

Each run evaluates three policies against the same 100,000 weather draws on
the 28-leg synthetic outback route. The benchmark includes process startup,
route parsing, computation and CSV formatting, with output sent to the null
device. It excludes saving the CSV and running the Python analysis.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DECM_BUILD_PYTHON=OFF
cmake --build build --parallel
python3 apps/route-lab/analysis/benchmark.py build/bin/solar-race
```

Pass two executables to compare builds. The harness warms up each binary once,
then records seven runs per binary, alternating their order.

| Release build | Median | Range | Trials per second |
| --- | ---: | ---: | ---: |
| Standalone prototype (`09204f5`) | 1.494 s | 1.477–1.540 s | 66,926 |
| Existing ECM integration (`4f8e72a`) | 1.561 s | 1.548–1.664 s | 64,074 |

Measured on arm64 macOS 15.7.7 with Apple Clang 17. One trial includes all
three policies. These are single-machine wall-clock timings. The integrated
build took about 4.5% longer and produced byte-identical 2,000-trial CSVs at
4.5, 5.5 and 6.5 kWh with seed 2025.

The simulator links ECM directly in C++. Python launches the benchmark and
analyses completed trials. ECM's pybind11 module is used by its Python
consumers, so switching binding libraries would have no effect on this run.

## Reusing pace evaluations

The reserve-first policy now carries the chosen speed's energy estimate into
the battery update. It also skips candidates that a leg's speed limit would
clamp to a speed already evaluated.

| Release build | Median | Range | Trials per second |
| --- | ---: | ---: | ---: |
| Before pace reuse (`074259f`) | 1.584 s | 1.582–1.657 s | 63,119 |
| With pace reuse | 1.482 s | 1.481–1.500 s | 67,493 |

The median elapsed time fell by 6.4% in a separate run of the same benchmark
on the same Mac. Seeded 2,000-trial output matched byte for byte before and
after the change at 4.5, 5.5 and 6.5 kWh.
