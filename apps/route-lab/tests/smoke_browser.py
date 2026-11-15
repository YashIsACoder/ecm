import csv
import io
import json
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
                assert "Replay a shared weather draw" in page.read().decode()
            with urlopen(url + "/?replay=1", timeout=5) as page:
                assert page.status == 200
            with urlopen(url + "/demo.json", timeout=5) as response:
                assert json.load(response)["result"]["trace"]["draw"] == 6

            request = {"route": "rolling", "vehicle": "sr8", "model": "mapped",
                       "trials": 8, "seed": "42", "trace_trial": 3}
            data = json.dumps(request).encode()
            with urlopen(Request(url + "/api/run", data, {"Content-Type": "application/json"}), timeout=10) as response:
                result = json.load(response)
            assert result["trials"] == 8 and len(list(csv.DictReader(io.StringIO(result["csv"])))) == 24
            assert result["trace"]["draw"] == 3 and set(result["trace"]["logs"]) == {"steady", "push", "reserve-first"}
            first = [events[0]["headwind_mps"] for events in result["trace"]["logs"].values()]
            assert len(set(first)) == 1
            assert result["distances_km"][-1] == 1150

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
