"""Summarise the paired CSV produced by the C++ simulator."""

import argparse
import csv
import math
import statistics
from dataclasses import dataclass
from pathlib import Path


POLICIES = ("steady", "push", "reserve-first")


@dataclass(frozen=True)
class Trial:
    completed: bool
    reserve_breached: bool
    drive_hours: float


def bit(value: str) -> bool:
    if value not in ("0", "1"):
        raise ValueError("expected 0 or 1")
    return value == "1"


def read_trials(path: Path) -> dict[int, dict[str, Trial]]:
    by_trial: dict[int, dict[str, Trial]] = {}
    with path.open(newline="", encoding="utf-8") as file:
        rows = csv.DictReader(file)
        required = {"trial", "policy", "completed", "reserve_breached", "drive_hours"}
        if not required.issubset(rows.fieldnames or []):
            raise ValueError("input is missing trial result columns")
        for row in rows:
            try:
                number = int(row["trial"])
                policy = row["policy"]
                hours = float(row["drive_hours"])
                if number <= 0 or policy not in POLICIES or not math.isfinite(hours) or hours < 0:
                    raise ValueError("invalid trial, policy or driving time")
                group = by_trial.setdefault(number, {})
                if policy in group:
                    raise ValueError(f"duplicate policy {policy} in trial {number}")
                group[policy] = Trial(bit(row["completed"]), bit(row["reserve_breached"]), hours)
            except (TypeError, ValueError) as error:
                raise ValueError(f"{path}:{rows.line_num}: {error}") from error
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
    return (centre - margin) / denominator, (centre + margin) / denominator


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
    lines += ["", "The interval covers sampling error under this model. The vehicle and weather assumptions are synthetic."]
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
