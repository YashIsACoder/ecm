"""Run a paired route experiment across Sunswift scenarios and ECM models."""

import argparse
import hashlib
import json
import math
import statistics
import subprocess
from pathlib import Path

from analyse import POLICIES, Trial, format_difference, paired_difference, read_trials


VEHICLES = ("reference", "sr7", "sr8")
MODELS = ("segment", "cycle", "mapped")
Cases = dict[tuple[str, str], dict[int, dict[str, Trial]]]


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            checksum.update(block)
    return checksum.hexdigest()


def render_report(cases: Cases, trials: int, seed: int) -> str:
    lines = [
        "# Route lab results",
        "",
        f"Each case used **{trials:,}** weather draws with seed `{seed}`.",
        "",
        "| Vehicle | ECM | Policy | Completion | Reserve breach | Median drive time among finishers |",
        "| --- | --- | --- | ---: | ---: | ---: |",
    ]
    for (vehicle, model), case in cases.items():
        for policy in POLICIES:
            results = [trial[policy] for trial in case.values()]
            finished = [result.drive_hours for result in results if result.completed]
            breached = sum(result.reserve_breached for result in results)
            median = f"{statistics.median(finished):.2f} h" if finished else "—"
            lines.append(
                f"| {vehicle} | {model} | {policy} | {len(finished) / trials:.1%} | {breached / trials:.1%} | {median} |"
            )

    comparisons = []
    for (vehicle, model), case in cases.items():
        reference = cases.get((vehicle, "segment"))
        if model == "segment" or reference is None:
            continue
        for policy in POLICIES:
            pairs = [(trial[policy], reference[number][policy]) for number, trial in case.items()]
            completion = paired_difference((other.completed, base.completed) for other, base in pairs)
            reserve = paired_difference((other.reserve_breached, base.reserve_breached) for other, base in pairs)
            comparisons.append(
                f"| {vehicle} | {model} | {policy} | {format_difference(*completion)} | {format_difference(*reserve)} |"
            )
    if comparisons:
        lines += [
            "",
            "## Difference from the segment ECM",
            "",
            "Each comparison holds vehicle, policy, and weather draw fixed.",
            "",
            "| Vehicle | ECM | Policy | Completion difference (95% CI) | Reserve-breach difference (95% CI) |",
            "| --- | --- | --- | ---: | ---: |",
            *comparisons,
        ]
    lines += ["", "Intervals describe sampling error under these scenario assumptions."]
    return "\n".join(lines) + "\n"


def run(args: argparse.Namespace) -> None:
    executable = args.executable.resolve(strict=True)
    route = args.route.resolve(strict=True)
    manifest = {
        "route": {"file": route.name, "sha256": digest(route)},
        "simulator": {"file": executable.name, "sha256": digest(executable)},
        "trials": args.trials,
        "seed": args.seed,
        "pack_override_kwh": args.pack_kwh,
        "cases": [],
    }
    args.out.mkdir(parents=True, exist_ok=False)
    cases: Cases = {}
    for vehicle in dict.fromkeys(args.vehicles):
        for model in dict.fromkeys(args.models):
            output = args.out / f"{vehicle}-{model}.csv"
            command = [
                str(executable), "--route", str(route), "--vehicle", vehicle, "--model", model,
                "--trials", str(args.trials), "--seed", str(args.seed),
            ]
            if args.pack_kwh is not None:
                command += ["--pack-kwh", str(args.pack_kwh)]
            with output.open("wb") as stream:
                subprocess.run(command, stdout=stream, check=True)
            trials = read_trials(output)
            if len(trials) != args.trials or min(trials) != 1 or max(trials) != args.trials:
                raise ValueError(f"{output} does not contain all expected trial IDs")
            cases[vehicle, model] = trials
            manifest["cases"].append({"vehicle": vehicle, "model": model, "results": output.name})
    (args.out / "report.md").write_text(render_report(cases, args.trials, args.seed), encoding="utf-8")
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--route", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True, help="new directory for results and manifest")
    parser.add_argument("--trials", type=int, default=2000)
    parser.add_argument("--seed", type=int, default=2025)
    parser.add_argument("--vehicles", nargs="+", choices=VEHICLES, default=("sr7", "sr8"))
    parser.add_argument("--models", nargs="+", choices=(*MODELS, "gust"), default=MODELS)
    parser.add_argument("--pack-kwh", type=float, help="apply the same capacity override to every vehicle")
    args = parser.parse_args()
    if not 0 < args.trials < 2**64 or not 0 <= args.seed < 2**64:
        parser.error("trials must be positive and trials and seed must fit unsigned 64-bit integers")
    if args.pack_kwh is not None and (args.pack_kwh <= 0 or not math.isfinite(args.pack_kwh * 1000)):
        parser.error("pack capacity must be a positive finite number of kWh")
    try:
        run(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"route lab: {error}\n")


if __name__ == "__main__":
    main()
