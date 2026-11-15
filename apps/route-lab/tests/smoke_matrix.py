import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> None:
    tests = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory() as temporary:
        output = Path(temporary) / "matrix"
        subprocess.run(
            [
                sys.executable, str(tests.parent / "analysis" / "run_matrix.py"), sys.argv[1],
                "--route", sys.argv[2], "--trials", "3", "--seed", "42", "--out", str(output),
            ],
            check=True,
        )
        manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        cases = {(case["vehicle"], case["model"]) for case in manifest["cases"]}
        expected = {(vehicle, model) for vehicle in ("sr7", "sr8") for model in ("segment", "cycle", "mapped")}
        assert manifest["trials"] == 3 and manifest["seed"] == 42 and cases == expected
        report = (output / "report.md").read_text(encoding="utf-8")
        assert "Difference from the segment ECM" in report


if __name__ == "__main__":
    main()
