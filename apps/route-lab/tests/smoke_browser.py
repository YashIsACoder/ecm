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
                atlas = page.read().decode()
                assert "SolarRoute Studio" in atlas and 'id="standings"' in atlas
                assert all(link in atlas for link in ("./race.html", "./teams.html", "./data.html", "./about.html"))
                assert 'id="fleet-form"' in atlas
            with urlopen(url + "/race.html", timeout=5) as page:
                assert 'id="record-log"' in page.read().decode()
            with urlopen(url + "/bwsc-2025.json", timeout=5) as response:
                archive = json.load(response)
                assert len(archive["teams"]) == 8
                result = [row for row in archive["events"] if row["id"] == "sunswift"][-1]
                assert result["at"] == "2025-08-29T12:30:38+09:30" and result["source"] == "final"
                assert archive["stops"][result["stop"]]["name"] == "Port Augusta"
                assert sum(row["status"] == "Finished" for row in archive["events"]) == 3
            with urlopen(url + "/experiment.html", timeout=5) as page:
                dashboard = page.read().decode()
                assert "SolarRoute Studio" in dashboard and "Experiment setup" in dashboard
                assert 'class="atlas-sheet experiment-sheet"' in dashboard and 'id="route-map"' in dashboard
                assert "./analytics.html" in dashboard and 'id="reach"' not in dashboard
            with urlopen(url + "/atlas-relief.webp", timeout=5) as asset:
                assert asset.headers.get_content_type() == "image/webp"
                assert asset.read(4) == b"RIFF"
            with urlopen(url + "/atlas-demo.json", timeout=5) as response:
                roster = json.load(response)
                assert roster["model"] == "mapped" and roster["weather"] == "posterior"
                assert [team["id"] for team in roster["teams"]] == ["sunswift", "vtc", "solaride", "onda", "apollo"]
                assert all(team["legs"] and team["pace_ceiling_kmh"] > 0 for team in roster["teams"])
                ephemeris = roster["solar"]
                assert ephemeris["start_utc"] == "2025-08-23T22:30:00Z"
                assert (len(ephemeris["vectors"]) - 1) * ephemeris["step_minutes"] / 60 >= roster["horizon_hours"]
            for page, phrase in (("teams.html", "2025 Cruiser teams"), ("data.html", "Cruiser comparison"),
                                 ("about.html", "Sun and terrain")):
                with urlopen(url + "/" + page, timeout=5) as content:
                    assert phrase in content.read().decode()
            for page in ("experiment.html", "configure.html", "analytics.html", "methods.html"):
                with urlopen(url + "/" + page, timeout=5) as content:
                    document = content.read().decode()
                    assert 'href="./atlas.css"' in document and 'class="atlas-header"' in document
                    assert all(link in document for link in ("./race.html", "./teams.html", "./data.html", "./about.html"))
                    assert 'aria-label="Experiment"' in document
            with urlopen(url + "/analytics.html?trials=8", timeout=5) as page:
                analytics = page.read().decode()
                assert 'id="reach"' in analytics and 'id="history-time"' in analytics
            with urlopen(url + "/configure.html?trials=8", timeout=5) as page:
                assert 'id="experiment"' in page.read().decode()
            with urlopen(url + "/scenario.mjs", timeout=5) as asset:
                assert asset.headers.get_content_type() == "text/javascript"
            with urlopen(url + "/?replay=1", timeout=5) as page:
                assert page.status == 200
            with urlopen(url + "/demo.json", timeout=5) as response:
                bundle = json.load(response)
                assert bundle["settings"]["reserve_strategy"] == "envelope"
                demo = bundle["result"]
                assert demo["trace"]["draw"] == 7 and demo["trace"]["route"] == "outback"
                assert "race_arrive_hours" in demo["trace"]["logs"]["steady"][0]
                demo_events = demo["trace"]["logs"]["steady"]
                assert demo["energy_rules"] == "bwsc_2025_cruiser"
                assert any(event["control_hours"] == 0.5 for event in demo_events)
                assert demo_events[0]["depart_soc_pct"] == 100
                assert any(event["external_charge_wh"] > 0 and event["depart_soc_pct"] == 100 for event in demo_events)
                assert demo["trace"]["logs"]["push"][-1]["depleted"] == 1
                assert bundle["settings"]["rain_multiplier"] == 1
                assert demo_events[0]["rain_mm_h"] == 0 and 0 < demo_events[0]["rain_probability"] < 0.001
            with urlopen(url + "/methods.html", timeout=5) as methods:
                assert "Inside the sampler" in methods.read().decode()
            with urlopen(url + "/forecast-demo.json", timeout=5) as response:
                forecast = json.load(response)
                assert forecast["retained_per_chain"] == 1600 and forecast["plot_stride"] == 8
                assert len(forecast["features"]["solar"]["chains"]) == 4
            with urlopen(url + "/replay.mjs", timeout=5) as asset:
                assert asset.headers.get_content_type() == "text/javascript"

            fleet_settings = {"model": "gust", "weather": "baseline", "seed": "42", "trials": 3, "draw": 2,
                              "rain_multiplier": 0, "custom": {"mass_kg": 575, "drag_area_m2": .12,
                              "pack_kwh": 11.5, "custom_pace_kmh": 79}}
            def post_fleet(settings):
                return urlopen(Request(url + "/api/fleet", json.dumps(settings).encode(),
                                       {"Content-Type": "application/json"}), timeout=10)
            with post_fleet(fleet_settings) as response:
                simulation = json.load(response)
            assert simulation["model"] == "gust" and simulation["trials"] == 3 and simulation["draw"] == 2
            assert simulation["points"][-1]["km"] == 3021.5
            custom = simulation["teams"][-1]
            assert custom["id"] == "custom" and custom["mass_kg"] == 575 and custom["pack_kwh"] == 11.5
            assert custom["drag_area_m2"] == .12 and custom["pace_ceiling_kmh"] == 79
            assert custom["legs"][0]["demand_wh"] != simulation["teams"][0]["legs"][0]["demand_wh"]
            try:
                with post_fleet(fleet_settings | {"custom": {"pack_kwh": 38}}):
                    raise AssertionError("Fleet accepted storage beyond the 2025 Cruiser limit")
            except HTTPError as error:
                assert error.code == 400

            request = {"route": "rolling", "vehicle": "sr8", "model": "mapped",
                       "trials": 8, "seed": "42", "trace_trial": 3, "weather": "posterior", "rain_multiplier": 0}
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

            changes = {
                "steady_kmh": 66, "push_kmh": 73, "reserve_strategy": "envelope",
                "reserve_fast_kmh": 80, "reserve_slow_kmh": 60, "reserve_target_pct": 15,
                "array_m2": 2.25, "cell_efficiency_pct": 23, "system_efficiency_pct": 92,
                "parked_wh_m2": 1000, "headwind_offset_mps": 3, "solar_multiplier": 0.5,
            }
            configured = Request(url + "/api/run", json.dumps(request | changes).encode(),
                                 {"Content-Type": "application/json"})
            with urlopen(configured, timeout=10) as response:
                customized = json.load(response)
            custom_logs = customized["trace"]["logs"]
            first_steady = custom_logs["steady"][0]
            assert first_steady["speed_kmh"] == 66 and 0 < first_steady["mean_speed_kmh"] < 66
            assert custom_logs["push"][0]["speed_kmh"] == 73
            assert math.isclose(first_steady["headwind_mps"], baseline_trace["headwind_mps"] + 3, abs_tol=2e-6)
            assert math.isclose(first_steady["irradiance_w_m2"], baseline_trace["irradiance_w_m2"] / 2, abs_tol=2e-6)
            assert custom_logs["reserve-first"][0]["planned_arrival_soc_pct"] >= 15
            duration = first_steady["race_arrive_hours"] - first_steady["race_depart_hours"]
            expected_solar = 2.25 * .23 * .92 * first_steady["irradiance_w_m2"] * duration
            assert math.isclose(first_steady["solar_wh"], expected_solar, abs_tol=1e-3)
            command = [str(Path(sys.argv[1]).resolve()), *customized["reproduce"]["arguments"]]
            regenerated = subprocess.run(command, cwd=lab.parents[1], check=True, capture_output=True, text=True)
            assert regenerated.stdout == customized["csv"] and regenerated.stderr == customized["trace"]["csv"]

            wet_case = request | {"route": "outback", "vehicle": "sr7", "weather": "posterior",
                                  "seed": "2025", "trials": 15, "trace_trial": 15, "rain_multiplier": 8}
            rainy = Request(url + "/api/run", json.dumps(wet_case).encode(), {"Content-Type": "application/json"})
            with urlopen(rainy, timeout=10) as response:
                rainy_trace = json.load(response)["trace"]
            rainy_events = rainy_trace["logs"]["steady"]
            assert rainy_events[0]["rain_probability"] > demo_events[0]["rain_probability"] * 7
            for event in rainy_events:
                checkpoint = rainy_trace["points"][event["leg"]]
                assert event["effective_pace_cap_kmh"] <= checkpoint["pace_cap_kmh"]
                assert event["speed_kmh"] <= event["effective_pace_cap_kmh"]

            gust_case = request | {"model": "gust", "gust_sd_mps": 4}
            gust = Request(url + "/api/run", json.dumps(gust_case).encode(), {"Content-Type": "application/json"})
            with urlopen(gust, timeout=10) as response:
                trace_with_gust = json.load(response)["trace"]["logs"]["steady"][0]
            assert trace_with_gust["drag_wh"] > baseline_trace["drag_wh"]
            assert trace_with_gust["race_arrive_hours"] == baseline_trace["race_arrive_hours"]

            oversize = Request(url + "/api/run", json.dumps(wet_case | {"pack_kwh": 38}).encode(),
                               {"Content-Type": "application/json"})
            try:
                urlopen(oversize, timeout=5).close()
            except HTTPError as error:
                with error:
                    assert error.code == 400 and "55 MJ" in json.load(error)["error"]
            else:
                raise AssertionError("lab accepted a pack larger than the Cruiser limit")

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
