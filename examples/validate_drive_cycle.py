"""Compare the C++ solver with exact work and fixed-step midpoint integration."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

import ecm


def power_w(speed: float) -> tuple[float, float]:
    wheel = speed**3 - 52.0 * speed
    traction = wheel / 0.8 if wheel >= 0.0 else max(wheel, -96.0)
    return wheel, traction


def midpoint_traction_wh(steps: int) -> float:
    """Integrate the test case's electrical power independently of the C++ solver."""
    dt = 8.0 / steps
    powers = (power_w((step + 0.5) * dt)[1] for step in range(steps))
    return math.fsum(powers) * dt / 3600.0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--power-csv", type=Path, help="write the synthetic reference power trace")
    args = parser.parse_args()
    vehicle = ecm.VehicleParams()
    vehicle.rho = 2.0
    vehicle.Cd = vehicle.A = 1.0
    vehicle.m = vehicle.g = 10.0
    vehicle.Crr = vehicle.X = 0.0
    grade = -0.62 / math.sqrt(1.0 - 0.62**2)
    profile = [ecm.SpeedInterval(8.0, 0.0, 8.0, grade=grade)]
    result = ecm.drive_cycle(vehicle, profile, ecm.Drivetrain(0.8, 1.0, 96.0))
    exact_wh = -503.0 / 3600.0
    print(f"C++ traction energy  {result.traction_wh:+.12f} Wh")
    print(f"Closed-form energy   {exact_wh:+.12f} Wh")
    print(f"Absolute difference {abs(result.traction_wh - exact_wh):.3e} Wh")
    print()
    print(f"{'Substeps':>8} {'Midpoint (Wh)':>16} {'Absolute error (Wh)':>21}")
    for steps in (4, 16, 64, 256, 1024):
        wh = midpoint_traction_wh(steps)
        print(f"{steps:8d} {wh:16.12f} {abs(wh - exact_wh):21.3e}")

    if args.power_csv:
        with args.power_csv.open("w", newline="", encoding="utf-8") as trace:
            writer = csv.writer(trace)
            writer.writerow(("time_s", "wheel_w", "traction_w"))
            for sample in range(257):
                time_s = sample / 32.0
                writer.writerow((time_s, *power_w(time_s)))


if __name__ == "__main__":
    main()
