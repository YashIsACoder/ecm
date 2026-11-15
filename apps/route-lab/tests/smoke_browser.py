import csv
import io
import json
import math
import subprocess
import sys
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen


def main() -> None:
    lab = Path(__file__).resolve().parents[1]
    command = [sys.executable, str(lab / "serve.py"), sys.argv[1], "--port", "0"]
    with subprocess.Popen(command, stdout=subprocess.PIPE, text=True) as server:
        try:
            url = server.stdout.readline().strip().rsplit(" ", 1)[-1]
            assert url.startswith("http://127.0.0.1:")
            with urlopen(url, timeout=5) as page:
                assert "Race control" in page.read().decode()
            with urlopen(url + "/?replay=1", timeout=5) as page:
                assert page.status == 200
            with urlopen(url + "/demo.json", timeout=5) as response:
                demo = json.load(response)["result"]
                assert demo["trace"]["draw"] == 15 and demo["trace"]["route"] == "outback"
                assert "race_arrive_hours" in demo["trace"]["logs"]["steady"][0]
                assert any(event["control_hours"] == 0.5 for event in demo["trace"]["logs"]["steady"])
            with urlopen(url + "/methods.html", timeout=5) as methods:
                assert "Inside the sampler" in methods.read().decode()
            with urlopen(url + "/forecast-demo.json", timeout=5) as response:
                forecast = json.load(response)
                assert forecast["retained_per_chain"] == 1600 and forecast["plot_stride"] == 8
                assert len(forecast["features"]["solar"]["chains"]) == 4
            with urlopen(url + "/replay.mjs", timeout=5) as asset:
                assert asset.headers.get_content_type() == "text/javascript"

            request = {"route": "rolling", "vehicle": "sr8", "model": "mapped",
                       "trials": 8, "seed": "42", "trace_trial": 3, "weather": "posterior"}
            data = json.dumps(request).encode()
            with urlopen(Request(url + "/api/run", data, {"Content-Type": "application/json"}), timeout=10) as response:
                result = json.load(response)
            assert result["trials"] == 8 and len(list(csv.DictReader(io.StringIO(result["csv"])))) == 24
            assert result["trace"]["draw"] == 3 and set(result["trace"]["logs"]) == {"steady", "push", "reserve-first"}
            first = [events[0]["headwind_mps"] for events in result["trace"]["logs"].values()]
            assert len(set(first)) == 1
            assert result["distances_km"][-1] == 1150
            assert result["trace"]["points"][-1]["km"] == 1150
            assert len(result["policies"][0]["reserve_ci"]) == 2
            assert len(result["reserve_vs_steady"]["push"]) == 3
            trace = result["trace"]["logs"]["steady"][0]
            assert abs(trace["traction_wh"] + trace["auxiliary_wh"] - trace["demand_wh"]) < 3e-6
            assert trace["elapsed_hours"] > 0 and trace["rolling_wh"] > 0
            events = result["trace"]["logs"]["steady"]
            held, *_ = [event for event in events if event["control_hours"] == 0.5]
            assert math.isclose(events[held["leg"] + 1]["race_depart_hours"],
                                held["race_arrive_hours"] + 0.5, rel_tol=0, abs_tol=2e-6)
            assert any(event["race_depart_hours"] == 24 for event in events)
            request["weather"] = "baseline"
            baseline = Request(url + "/api/run", json.dumps(request).encode(), {"Content-Type": "application/json"})
            with urlopen(baseline, timeout=10) as response:
                baseline_trace = json.load(response)["trace"]["logs"]["steady"][0]
            assert baseline_trace["headwind_mps"] != trace["headwind_mps"]

            request["trace_trial"] = 9
            rejected = Request(url + "/api/run", json.dumps(request).encode(), {"Content-Type": "application/json"})
            try:
                urlopen(rejected, timeout=5).close()
            except HTTPError as error:
                with error:
                    assert error.code == 400 and "Replay draw" in json.load(error)["error"]
            else:
                raise AssertionError("lab accepted a replay draw outside the experiment")
        finally:
            server.terminate()
            server.wait(timeout=5)


if __name__ == "__main__":
    main()
