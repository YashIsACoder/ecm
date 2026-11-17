"""Serve the local browser lab using the native solar-race executable."""

import argparse
import csv
import hashlib
import io
import json
import math
import os
import re
import statistics
import subprocess
from bisect import bisect_left
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from itertools import accumulate
from pathlib import Path
from threading import BoundedSemaphore
from urllib.parse import urlsplit

from analysis.analyse import POLICIES, paired_difference, read_trial_stream, wilson


WEB = Path(__file__).resolve().parent / "web"
ROUTES = Path(__file__).resolve().parents[2] / "data" / "routes"
ROUTE_FILES = {"outback": "outback-2025-style.csv", "rolling": "rolling-1150.csv"}
ASSETS = {
    "/": ("index.html", "text/html; charset=utf-8"),
    "/index.html": ("index.html", "text/html; charset=utf-8"),
    "/atlas.css": ("atlas.css", "text/css; charset=utf-8"),
    "/atlas.mjs": ("atlas.mjs", "text/javascript; charset=utf-8"),
    "/atlas-map.mjs": ("atlas-map.mjs", "text/javascript; charset=utf-8"),
    "/atlas-demo.json": ("atlas-demo.json", "application/json"),
    "/atlas-relief.webp": ("atlas-relief.webp", "image/webp"),
    "/race.html": ("race.html", "text/html; charset=utf-8"),
    "/race.mjs": ("race.mjs", "text/javascript; charset=utf-8"),
    "/bwsc-2025.json": ("bwsc-2025.json", "application/json"),
    "/experiment.html": ("experiment.html", "text/html; charset=utf-8"),
    "/fleet-data.mjs": ("fleet-data.mjs", "text/javascript; charset=utf-8"),
    "/fleet-pages.mjs": ("fleet-pages.mjs", "text/javascript; charset=utf-8"),
    "/teams.html": ("teams.html", "text/html; charset=utf-8"),
    "/data.html": ("data.html", "text/html; charset=utf-8"),
    "/about.html": ("about.html", "text/html; charset=utf-8"),
    "/lab.css": ("lab.css", "text/css; charset=utf-8"),
    "/race-control.css": ("race-control.css", "text/css; charset=utf-8"),
    "/lab.mjs": ("lab.mjs", "text/javascript; charset=utf-8"),
    "/configure.html": ("configure.html", "text/html; charset=utf-8"),
    "/configure.mjs": ("configure.mjs", "text/javascript; charset=utf-8"),
    "/scenario.mjs": ("scenario.mjs", "text/javascript; charset=utf-8"),
    "/analytics.html": ("analytics.html", "text/html; charset=utf-8"),
    "/analytics.mjs": ("analytics.mjs", "text/javascript; charset=utf-8"),
    "/run-data.mjs": ("run-data.mjs", "text/javascript; charset=utf-8"),
    "/map-data.mjs": ("map-data.mjs", "text/javascript; charset=utf-8"),
    "/replay.mjs": ("replay.mjs", "text/javascript; charset=utf-8"),
    "/methods.html": ("methods.html", "text/html; charset=utf-8"),
    "/methods.mjs": ("methods.mjs", "text/javascript; charset=utf-8"),
    "/forecast-demo.json": ("forecast-demo.json", "application/json"),
    "/demo.json": ("demo.json", "application/json"),
}
MAX_TRIALS = 3000
CRUISER_MAX_KWH = (55_000_000.0 / 3600.0) / 1000.0
DEMO = {"route": "outback", "vehicle": "sr7", "model": "mapped", "trials": 500,
        "seed": "2025", "trace_trial": 7, "pack_kwh": None, "weather": "posterior",
        "reserve_strategy": "envelope", "reserve_target_pct": 15, "reserve_slow_kmh": 65, "rain_multiplier": 1}


PACE_DEFAULTS = {"steady_kmh": 82, "push_kmh": 92, "reserve_fast_kmh": 92,
                 "reserve_medium_kmh": 85, "reserve_slow_kmh": 75}
SOLAR_LIMITS = {
    "array_m2": ("Solar array area", 0, 30),
    "cell_efficiency_pct": ("Cell efficiency", 0.01, 100),
    "system_efficiency_pct": ("System efficiency", 0.01, 100),
    "parked_wh_m2": ("Parked exposure", 0, 10000),
}


def bounded_number(value, label, minimum, maximum):
    if type(value) not in (int, float) or not minimum <= value <= maximum:
        raise ValueError(f"{label} must be from {minimum:g} to {maximum:g}.")
    return value


def atlas_arguments(request: dict) -> list[str]:
    if not isinstance(request, dict):
        raise ValueError("Expected fleet simulation settings as an object.")
    model, weather = request.get("model"), request.get("weather")
    if model not in ("segment", "cycle", "mapped", "gust") or weather not in ("baseline", "posterior"):
        raise ValueError("Choose a supported ECM and weather model.")
    trials, draw, seed = request.get("trials"), request.get("draw"), request.get("seed")
    if type(trials) is not int or not 1 <= trials <= 500:
        raise ValueError("Fleet trial count must be an integer from 1 to 500.")
    if type(draw) is not int or not 1 <= draw <= trials:
        raise ValueError("Choose a replay draw between 1 and the trial count.")
    if not isinstance(seed, str) or not re.fullmatch(r"[0-9]{1,20}", seed) or int(seed) >= 2**64:
        raise ValueError("Seed must be a nonnegative 64-bit integer.")
    rain = bounded_number(request.get("rain_multiplier", 1), "Rain exposure", 0, 8)
    gust = bounded_number(request.get("gust_sd_mps", 2), "Gust spread", 0, 15)
    args = ["--atlas-report", "--model", model, "--weather", weather, "--trials", str(trials),
            "--trace-trial", str(draw), "--seed", seed, "--rain-multiplier", str(rain), "--gust-sd-mps", str(gust)]
    custom = request.get("custom")
    if custom is not None:
        limits = {
            "mass_kg": ("Race mass", 120, 2000), "drag_area_m2": ("Drag area", .03, 1.5),
            "rolling_coefficient": ("Rolling coefficient", .001, .03), "auxiliary_w": ("Auxiliary load", 0, 1500),
            "custom_pace_kmh": ("Pace ceiling", 20, 130), "array_m2": ("Array area", 0, 30),
            "cell_efficiency_pct": ("Cell efficiency", .01, 100), "pack_kwh": ("Pack capacity", .001, CRUISER_MAX_KWH),
        }
        if not isinstance(custom, dict) or custom.keys() - limits.keys():
            raise ValueError("Invalid custom vehicle fields.")
        args.append("--atlas-custom")
        for key, value in custom.items():
            args.extend(["--" + key.replace("_", "-"), str(bounded_number(value, *limits[key]))])
    return args


def experiment_arguments(request: dict) -> tuple[list[str], Path, int]:
    if not isinstance(request, dict):
        raise ValueError("Expected experiment settings as an object.")
    route, vehicle, model = (request.get(key) for key in ("route", "vehicle", "model"))
    if not isinstance(route, str) or route not in ROUTE_FILES:
        raise ValueError("Choose one of the supplied routes.")
    if vehicle not in ("sr7", "sr8", "reference") or model not in ("segment", "cycle", "mapped", "gust"):
        raise ValueError("Choose a supported vehicle and ECM model.")
    weather = request.get("weather", "posterior")
    if weather not in ("baseline", "posterior"):
        raise ValueError("Choose a supported weather model.")
    trials = request.get("trials")
    if type(trials) is not int or not 1 <= trials <= MAX_TRIALS:
        raise ValueError(f"Trial count must be an integer from 1 to {MAX_TRIALS:,}.")
    trace = request.get("trace_trial", 1)
    if type(trace) is not int or not 1 <= trace <= trials:
        raise ValueError("Replay draw must be between 1 and the number of trials.")
    seed = request.get("seed")
    if not isinstance(seed, str) or not re.fullmatch(r"[0-9]{1,20}", seed) or int(seed) >= 2**64:
        raise ValueError("Seed must be a nonnegative 64-bit integer.")
    path = ROUTES / ROUTE_FILES[route]
    schedule = "--bwsc-2025-timing" if vehicle == "sr7" else "--race-schedule"
    args = ["--route", str(path), "--vehicle", vehicle, "--model", model, "--trials", str(trials),
            "--seed", seed, "--trace-trial", str(trace), "--weather", weather, schedule]
    pack = request.get("pack_kwh")
    if pack is not None:
        if type(pack) not in (int, float) or pack <= 0 or not math.isfinite(pack * 1000.0):
            raise ValueError("Battery capacity must be a positive finite number of kWh.")
        if vehicle == "sr7" and pack > CRUISER_MAX_KWH:
            raise ValueError("2025 Cruiser storage is capped at 55 MJ. Leave the pack field blank to use the exact limit.")
        args += ["--pack-kwh", str(pack)]

    reserve_strategy = request.get("reserve_strategy", "tiers")
    if reserve_strategy not in ("tiers", "envelope"):
        raise ValueError("Choose speed tiers or an energy envelope.")
    pace = {name: bounded_number(request.get(name, default), name.replace("_kmh", "").replace("_", " ").title(),
                                 20, 140) for name, default in PACE_DEFAULTS.items()}
    if pace["reserve_fast_kmh"] < pace["reserve_slow_kmh"]:
        raise ValueError("The reserve-first upper speed must be at least its lower speed.")
    if reserve_strategy == "tiers" and not pace["reserve_fast_kmh"] >= pace["reserve_medium_kmh"] >= pace["reserve_slow_kmh"]:
        raise ValueError("The middle tier must be between the upper and lower speeds.")
    numbers = {
        **pace,
        "reserve_target_pct": bounded_number(request.get("reserve_target_pct", 35), "Charge target", 0, 100),
        "solar_multiplier": bounded_number(request.get("solar_multiplier", 1), "Solar multiplier", 0, 2),
        "headwind_offset_mps": bounded_number(request.get("headwind_offset_mps", 0), "Wind offset", -25, 25),
        "gust_sd_mps": bounded_number(request.get("gust_sd_mps", 2), "Gust spread", 0, 15),
        "rain_multiplier": bounded_number(request.get("rain_multiplier", 1), "Rain exposure", 0, 8),
    }
    for name, (label, minimum, maximum) in SOLAR_LIMITS.items():
        if request.get(name) is not None:
            numbers[name] = bounded_number(request[name], label, minimum, maximum)
    args += ["--reserve-strategy", reserve_strategy]
    for name, value in numbers.items():
        args.extend(["--" + name.replace("_", "-"), str(value)])
    return args, path, trials


def run_case(executable: Path, request: dict) -> dict:
    args, route, count = experiment_arguments(request)
    with route.open(newline="", encoding="utf-8") as route_file:
        route_rows = list(csv.DictReader(route_file))
    checkpoints = list(accumulate(float(leg["distance_km"]) for leg in route_rows))
    route_points = [
        {"day": int(leg["day"]), "name": leg["destination"], "km": km, "grade_pct": float(leg["grade_pct"]),
         "pace_cap_kmh": float(leg["pace_cap_kmh"]), "control_minutes": float(leg.get("control_minutes", 0))}
        for leg, km in zip(route_rows, checkpoints)
    ]
    length = checkpoints[-1]
    completed = subprocess.run([executable, *args], capture_output=True, text=True, check=True, timeout=15)
    cases = read_trial_stream(io.StringIO(completed.stdout), "solar-race")
    if len(cases) != count or min(cases) != 1 or max(cases) != count:
        raise RuntimeError("The simulator returned an incomplete experiment.")

    distances = [length * i / 50 for i in range(51)]
    policies = []
    for name in POLICIES:
        trials = [trial[name] for trial in cases.values()]
        times = [trial.drive_hours for trial in trials if trial.completed]
        if any(trial.race_hours is None for trial in trials):
            raise RuntimeError("The simulator omitted scheduled race times.")
        race_times = [trial.race_hours for trial in trials if trial.completed]
        lengths = sorted(trial.distance_km for trial in trials if trial.distance_km is not None)
        if len(lengths) != count:
            raise RuntimeError("The simulator omitted route distances.")
        reached = [(count - bisect_left(lengths, point)) / count for point in distances[:-1]]
        reached.append(len(times) / count)
        breaches = sum(trial.reserve_breached for trial in trials)
        metrics = {}
        for name_of_metric in ("finish_soc_pct", "min_soc_pct", "demand_kwh", "solar_kwh", "spilled_kwh"):
            values = [getattr(trial, name_of_metric) for trial in trials]
            if any(value is None for value in values):
                raise RuntimeError(f"The simulator omitted {name_of_metric}.")
            metrics[name_of_metric] = statistics.median(values)
        if request["vehicle"] == "sr7" and any(trial.external_charge_kwh is None for trial in trials):
            raise RuntimeError("The simulator omitted Cruiser charging totals.")
        metrics["external_charge_kwh"] = statistics.median(trial.external_charge_kwh or 0.0 for trial in trials)
        policies.append({
            "name": name, "finishes": len(times), "completion_ci": wilson(len(times), count),
            "reserve_breaches": breaches, "reserve_ci": wilson(breaches, count),
            "median_hours": statistics.median(times) if times else None,
            "median_race_hours": statistics.median(race_times) if race_times else None,
            "route_reach": reached, "diagnostics": {"distance_km": statistics.median(lengths), **metrics},
        })
    comparisons = {
        name: paired_difference((trial[name].completed, trial["steady"].completed) for trial in cases.values())
        for name in POLICIES[1:]
    }
    reserve_comparisons = {
        name: paired_difference(
            (trial[name].reserve_breached, trial["steady"].reserve_breached) for trial in cases.values()
        ) for name in POLICIES[1:]
    }
    logs = {name: [] for name in POLICIES}
    for row in csv.DictReader(io.StringIO(completed.stderr)):
        name = row.pop("policy", "")
        if name not in logs:
            raise ValueError("Unexpected policy in the native leg trace.")
        leg = int(row.pop("leg"))
        if leg != len(logs[name]) or leg >= len(route_points):
            raise ValueError("Unexpected route index in the native leg trace.")
        observation = {key: float(value) if value else None for key, value in row.items()}
        previous = logs[name][-1] if leg else {"elapsed_hours": 0.0, "distance_km": 0.0,
                                                "race_arrive_hours": 0.0, "control_hours": 0.0,
                                                "overnight_hours": 0.0}
        if (not all(value is None or math.isfinite(value) for value in observation.values()) or
                any(observation[key] is None or observation[key] < previous[key]
                    for key in ("elapsed_hours", "distance_km"))):
            raise ValueError("Invalid driving time or distance in the native trace.")
        depart = observation["race_depart_hours"]
        arrive = observation["race_arrive_hours"]
        release = previous["race_arrive_hours"] + previous["control_hours"] + previous["overnight_hours"]
        # Native CSV fields round independently to six decimal places.
        if (depart is None or arrive is None or arrive < depart or
                not math.isclose(depart, release, rel_tol=0, abs_tol=3e-6) or
                any(observation[key] is None or observation[key] < 0 for key in ("control_hours", "overnight_hours"))):
            raise ValueError("Invalid race schedule in the native trace.")
        logs[name].append({"leg": leg, **observation})
    if any(not events for events in logs.values()):
        raise ValueError("The native simulator omitted the requested leg trace.")
    portable_args = [*args]
    portable_args[1] = f"data/routes/{route.name}"
    return {
        "energy_rules": "bwsc_2025_cruiser" if request["vehicle"] == "sr7" else "scenario",
        "trials": count, "distances_km": distances,
        "reproduce": {"arguments": portable_args, "route_sha256": hashlib.sha256(route.read_bytes()).hexdigest()}, "policies": policies,
        "completion_vs_steady": comparisons, "reserve_vs_steady": reserve_comparisons, "csv": completed.stdout,
        "trace": {"draw": request.get("trace_trial", 1), "route": request["route"], "points": route_points, "logs": logs,
                  "csv": completed.stderr},
    }


def handler_for(executable: Path):
    slots = BoundedSemaphore(2)

    class LabHandler(BaseHTTPRequestHandler):
        def setup(self):
            self.request.settimeout(10)
            super().setup()

        def respond(self, status, content, content_type):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; frame-ancestors 'none'; base-uri 'none'")
            self.end_headers()
            self.wfile.write(content)

        def json_response(self, status, content):
            self.respond(status, json.dumps(content, allow_nan=False).encode(), "application/json")

        def expected_host(self):
            return f"127.0.0.1:{self.server.server_port}"

        def do_GET(self):
            path = urlsplit(self.path).path
            if self.headers.get("Host") != self.expected_host() or path not in ASSETS:
                self.send_error(HTTPStatus.NOT_FOUND)
                return
            name, media_type = ASSETS[path]
            asset = WEB / name
            if not asset.is_file():
                self.send_error(HTTPStatus.NOT_FOUND)
                return
            self.respond(HTTPStatus.OK, asset.read_bytes(), media_type)

        def do_POST(self):
            host = self.expected_host()
            if self.headers.get("Host") != host or self.path not in ("/api/run", "/api/fleet"):
                self.send_error(HTTPStatus.NOT_FOUND)
                return
            if self.headers.get("Origin") not in (None, f"http://{host}"):
                self.json_response(HTTPStatus.FORBIDDEN, {"error": "Use the local lab page to run experiments."})
                return
            try:
                size = int(self.headers.get("Content-Length", "0"))
                if self.headers.get_content_type() != "application/json" or not 0 < size <= 2048:
                    raise ValueError("Expected a small JSON request.")
                request = json.loads(self.rfile.read(size))
                fleet_args = atlas_arguments(request) if self.path == "/api/fleet" else None
                if fleet_args is None:
                    experiment_arguments(request)
            except (ValueError, OverflowError) as error:
                self.json_response(HTTPStatus.BAD_REQUEST, {"error": str(error)})
                return
            if not slots.acquire(blocking=False):
                self.json_response(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "The lab is busy. Try again shortly."})
                return
            try:
                if fleet_args is None:
                    result = run_case(executable, request)
                else:
                    native = subprocess.run([executable, *fleet_args], capture_output=True, text=True,
                                            check=True, timeout=15)
                    result = json.loads(native.stdout)
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
                self.log_error("Simulation failed: %s", error)
                self.json_response(HTTPStatus.BAD_GATEWAY, {"error": "The native simulation could not finish."})
            else:
                self.json_response(HTTPStatus.OK, result)
            finally:
                slots.release()

    return LabHandler


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="built solar-race binary")
    parser.add_argument("--port", type=int, default=8765, help="loopback port, or 0 for an available port")
    parser.add_argument("--export-demo", type=Path, help="write a native run for the static homepage demo")
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file() or not os.access(executable, os.X_OK):
        parser.error("build solar-race and pass the path to its executable")
    if not 0 <= args.port <= 65535:
        parser.error("port must be between 0 and 65535")
    if args.export_demo:
        _, route, _ = experiment_arguments(DEMO)
        result = {
            "settings": DEMO,
            "source": {"route_sha256": hashlib.sha256(route.read_bytes()).hexdigest(),
                       "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()},
            "result": run_case(executable, DEMO),
        }
        args.export_demo.write_text(json.dumps(result, allow_nan=False, separators=(",", ":")) + "\n", encoding="utf-8")
        return
    with ThreadingHTTPServer(("127.0.0.1", args.port), handler_for(executable)) as server:
        print(f"Sunswift Route Lab · http://127.0.0.1:{server.server_port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
