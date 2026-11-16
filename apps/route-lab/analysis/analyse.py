"""Summarise the paired CSV produced by the C++ simulator."""

import argparse
import csv
import math
import statistics
from collections import Counter
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, TextIO


POLICIES = ("steady", "push", "reserve-first")


@dataclass(frozen=True)
class Trial:
    completed: bool
    reserve_breached: bool
    drive_hours: float
    distance_km: Optional[float] = None
    finish_soc_pct: Optional[float] = None
    min_soc_pct: Optional[float] = None
    demand_kwh: Optional[float] = None
    solar_kwh: Optional[float] = None
    spilled_kwh: Optional[float] = None
    race_hours: Optional[float] = None
    external_charge_kwh: Optional[float] = None


def bit(value: str) -> bool:
    if value not in ("0", "1"):
        raise ValueError("expected 0 or 1")
    return value == "1"


def read_trials(path: Path) -> dict[int, dict[str, Trial]]:
    with path.open(newline="", encoding="utf-8") as file:
        return read_trial_stream(file, str(path))


def optional_number(row: dict, column: str) -> Optional[float]:
    if column not in row:
        return None
    value = float(row[column])
    if not math.isfinite(value):
        raise ValueError(f"invalid {column}")
    return value


def read_trial_stream(file: TextIO, source: str) -> dict[int, dict[str, Trial]]:
    by_trial: dict[int, dict[str, Trial]] = {}
    rows = csv.DictReader(file)
    columns = rows.fieldnames or []
    required = {"trial", "policy", "completed", "reserve_breached", "drive_hours"}
    if not required.issubset(columns):
        raise ValueError("input is missing trial result columns")
    for row in rows:
        try:
            number = int(row["trial"])
            policy = row["policy"]
            hours = float(row["drive_hours"])
            distance = optional_number(row, "distance_km")
            if number <= 0 or policy not in POLICIES or not math.isfinite(hours) or hours < 0:
                raise ValueError("invalid trial, policy or driving time")
            if distance is not None and distance < 0:
                raise ValueError("invalid distance")
            group = by_trial.setdefault(number, {})
            if policy in group:
                raise ValueError(f"duplicate policy {policy} in trial {number}")
            group[policy] = Trial(
                bit(row["completed"]), bit(row["reserve_breached"]), hours, distance,
                optional_number(row, "finish_soc_pct"), optional_number(row, "min_soc_pct"),
                optional_number(row, "demand_kwh"), optional_number(row, "solar_kwh"),
                optional_number(row, "spilled_kwh"),
                optional_number(row, "race_hours"),
                optional_number(row, "external_charge_kwh"),
            )
        except (TypeError, ValueError) as error:
            raise ValueError(f"{source}:{rows.line_num}: {error}") from error
    if not by_trial:
        raise ValueError("input contains no trials")
    for number, group in by_trial.items():
        if len(group) != len(POLICIES):
            raise ValueError(f"trial {number} is missing a policy; run paired trials before comparing")
    return by_trial


def wilson(successes: int, count: int) -> tuple[float, float]:
    """Two-sided 95% binomial interval for Monte Carlo sampling error."""
    z = statistics.NormalDist().inv_cdf(0.975)
    share = successes / count
    denominator = 1 + z * z / count
    centre = share + z * z / (2 * count)
    margin = z * math.sqrt((share * (1 - share) + z * z / (4 * count)) / count)
    return max(0.0, (centre - margin) / denominator), min(1.0, (centre + margin) / denominator)


def paired_difference(outcomes: Iterable[tuple[bool, bool]]) -> tuple[float, float, float]:
    """Risk difference and Newcombe method 10 interval for (comparison, reference) pairs."""
    counts = Counter(outcomes)
    both = counts[True, True]
    comparison_only = counts[True, False]
    reference_only = counts[False, True]
    neither = counts[False, False]
    count = both + comparison_only + reference_only + neither
    comparison = both + comparison_only
    reference = both + reference_only

    # Method 10 shrinks positive sample association by a continuity correction.
    numerator = both * neither - comparison_only * reference_only
    if numerator > 0:
        numerator = max(0.0, numerator - count / 2)
    denominator = math.sqrt(comparison * (count - comparison) * reference * (count - reference))
    phi = numerator / denominator if denominator else 0.0

    comparison_low, comparison_high = wilson(comparison, count)
    reference_low, reference_high = wilson(reference, count)
    comparison_rate = comparison / count
    reference_rate = reference / count
    difference = (comparison_only - reference_only) / count

    def margin(first: float, second: float) -> float:
        return math.sqrt(max(0.0, first**2 - 2 * phi * first * second + second**2))

    return (
        difference,
        max(-1.0, difference - margin(comparison_rate - comparison_low, reference_high - reference_rate)),
        min(1.0, difference + margin(comparison_high - comparison_rate, reference_rate - reference_low)),
    )


def format_difference(estimate: float, low: float, high: float) -> str:
    return f"{100 * estimate:+.1f} pp ({100 * low:+.1f} to {100 * high:+.1f})"


def summarise(by_trial: dict[int, dict[str, Trial]]) -> str:
    count = len(by_trial)
    lines = [
        f"Paired weather draws: **{count:,}**. The reserve threshold is 15% of pack capacity.",
        "",
        "| Policy | Completed | 95% completion interval | Breached reserve | Median drive time among finishers |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for policy in POLICIES:
        group = [trial[policy] for trial in by_trial.values()]
        finished = [trial.drive_hours for trial in group if trial.completed]
        breached = sum(trial.reserve_breached for trial in group)
        low, high = wilson(len(finished), count)
        time = f"{statistics.median(finished):.2f} h" if finished else "—"
        lines.append(
            f"| {policy} | {len(finished):,}/{count:,} ({len(finished) / count:.1%}) | "
            f"{low:.1%}–{high:.1%} | {breached / count:.1%} | {time} |"
        )

    lines += [
        "",
        "| Comparison with steady | Completion difference (95% CI) | Reserve-breach difference (95% CI) |",
        "| --- | ---: | ---: |",
    ]
    for policy in POLICIES[1:]:
        pairs = [(trial[policy], trial["steady"]) for trial in by_trial.values()]
        completion = paired_difference((other.completed, steady.completed) for other, steady in pairs)
        reserve = paired_difference((other.reserve_breached, steady.reserve_breached) for other, steady in pairs)
        lines.append(f"| {policy} | {format_difference(*completion)} | {format_difference(*reserve)} |")

    pairs = [(trial["steady"], trial["reserve-first"]) for trial in by_trial.values()]
    steady_only = sum(steady.completed and not adaptive.completed for steady, adaptive in pairs)
    adaptive_only = sum(adaptive.completed and not steady.completed for steady, adaptive in pairs)
    common_delta = [
        60 * (adaptive.drive_hours - steady.drive_hours)
        for steady, adaptive in pairs
        if steady.completed and adaptive.completed
    ]
    lines += [
        "",
        f"Reserve-first completed **{adaptive_only:,}** trials that steady did not. "
        f"Steady completed **{steady_only:,}** that reserve-first did not.",
    ]
    if common_delta:
        delta = statistics.median(common_delta)
        lines.append(
            f"Across **{len(common_delta):,}** joint finishes, the median paired drive-time difference "
            f"(reserve-first minus steady) was **{delta:+.1f} minutes**."
        )
    lines += [
        "",
        "Each interval covers Monte Carlo sampling error under the synthetic assumptions "
        "and applies to its own comparison.",
    ]
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="CSV from solar-race")
    args = parser.parse_args()
    try:
        print(summarise(read_trials(args.results)), end="")
    except (OSError, ValueError) as error:
        parser.exit(1, f"analyse: {error}\n")


if __name__ == "__main__":
    main()
