"""Time Release builds of the C++ simulator on one fixed workload."""

import argparse
import statistics
import subprocess
import time
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executables", nargs="+", type=Path)
    args = parser.parse_args()
    executables = [path.resolve() for path in args.executables]

    route = Path(__file__).resolve().parents[3] / "data" / "routes" / "outback-2025-style.csv"
    trials = 100_000
    command_args = ["--route", str(route), "--trials", str(trials), "--seed", "2025"]
    timings = {executable: [] for executable in executables}

    for repetition in range(8):
        order = reversed(executables) if repetition % 2 else executables
        for executable in order:
            start = time.perf_counter()
            subprocess.run([executable, *command_args], stdout=subprocess.DEVNULL, check=True)
            elapsed = time.perf_counter() - start
            if repetition:
                timings[executable].append(elapsed)

    for executable, samples in timings.items():
        median = statistics.median(samples)
        print(f"{executable}")
        print(f"  Median {median:.3f} s (range {min(samples):.3f}–{max(samples):.3f} s)")
        print(f"  {trials / median:,.0f} trials/s")


if __name__ == "__main__":
    main()
