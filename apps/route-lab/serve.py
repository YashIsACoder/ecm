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
from pathlib import Path
from threading import BoundedSemaphore
from urllib.parse import urlsplit

from analysis.analyse import POLICIES, paired_difference, read_trial_stream, wilson


WEB = Path(__file__).resolve().parent / "web"
ROUTES = Path(__file__).resolve().parents[2] / "data" / "routes"
ROUTE_FILES = {"outback": "outback-2025-style.csv", "rolling": "rolling-1150.csv"}
ASSETS = {
    "/": ("index.html", "text/html; charset=utf-8"),
    "/lab.css": ("lab.css", "text/css; charset=utf-8"),
    "/lab.js": ("lab.js", "text/javascript; charset=utf-8"),
    "/demo.json": ("demo.json", "application/json"),
}
MAX_TRIALS = 3000
DEMO = {"route": "outback", "vehicle": "sr7", "model": "mapped", "trials": 500,
        "seed": "2025", "trace_trial": 6, "pack_kwh": None}


def experiment_arguments(request: dict) -> tuple[list[str], Path, int]:
    if not isinstance(request, dict):
        raise ValueError("Expected experiment settings as an object.")
    route, vehicle, model = (request.get(key) for key in ("route", "vehicle", "model"))
    if not isinstance(route, str) or route not in ROUTE_FILES:
        raise ValueError("Choose one of the supplied routes.")
    if vehicle not in ("sr7", "sr8", "reference") or model not in ("segment", "cycle", "mapped"):
        raise ValueError("Choose a supported vehicle and ECM model.")
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
    args = ["--route", str(path), "--vehicle", vehicle, "--model", model, "--trials", str(trials),
            "--seed", seed, "--trace-trial", str(trace)]
    pack = request.get("pack_kwh")
    if pack is not None:
        if type(pack) not in (int, float) or pack <= 0 or not math.isfinite(pack * 1000.0):
            raise ValueError("Battery capacity must be a positive finite number of kWh.")
        args += ["--pack-kwh", str(pack)]
    return args, path, trials


def run_case(executable: Path, request: dict) -> dict:
    args, route, count = experiment_arguments(request)
    with route.open(newline="", encoding="utf-8") as route_file:
        route_rows = list(csv.DictReader(route_file))
    route_points = [{"day": int(leg["day"]), "name": leg["destination"]} for leg in route_rows]
    length = math.fsum(float(leg["distance_km"]) for leg in route_rows)
    completed = subprocess.run([executable, *args], capture_output=True, text=True, check=True, timeout=15)
    cases = read_trial_stream(io.StringIO(completed.stdout), "solar-race")
    if len(cases) != count or min(cases) != 1 or max(cases) != count:
        raise RuntimeError("The simulator returned an incomplete experiment.")

    distances = [length * i / 50 for i in range(51)]
    policies = []
    for name in POLICIES:
        trials = [trial[name] for trial in cases.values()]
        times = [trial.drive_hours for trial in trials if trial.completed]
        lengths = sorted(trial.distance_km for trial in trials if trial.distance_km is not None)
        if len(lengths) != count:
            raise RuntimeError("The simulator omitted route distances.")
        reached = [(count - bisect_left(lengths, point)) / count for point in distances[:-1]]
        reached.append(len(times) / count)
        policies.append({
            "name": name, "finishes": len(times), "completion_ci": wilson(len(times), count),
            "reserve_breaches": sum(trial.reserve_breached for trial in trials),
            "median_hours": statistics.median(times) if times else None,
            "route_reach": reached,
        })
    comparisons = {
        name: paired_difference((trial[name].completed, trial["steady"].completed) for trial in cases.values())
        for name in POLICIES[1:]
    }
    logs = {name: [] for name in POLICIES}
    for row in csv.DictReader(io.StringIO(completed.stderr)):
        name = row.pop("policy", "")
        if name not in logs:
            raise ValueError("Unexpected policy in the native leg trace.")
        leg = int(row.pop("leg"))
        if leg != len(logs[name]) or leg >= len(route_points):
            raise ValueError("Unexpected route index in the native leg trace.")
        logs[name].append({"leg": leg, **{key: float(value) for key, value in row.items()}})
    if any(not events for events in logs.values()):
        raise ValueError("The native simulator omitted the requested leg trace.")
    return {
        "trials": count, "distances_km": distances, "policies": policies,
        "completion_vs_steady": comparisons, "csv": completed.stdout,
        "trace": {"draw": request.get("trace_trial", 1), "points": route_points, "logs": logs,
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
            if self.headers.get("Host") != host or self.path != "/api/run":
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
                experiment_arguments(request)
            except (ValueError, OverflowError) as error:
                self.json_response(HTTPStatus.BAD_REQUEST, {"error": str(error)})
                return
            if not slots.acquire(blocking=False):
                self.json_response(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "The lab is busy. Try again shortly."})
                return
            try:
                result = run_case(executable, request)
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
