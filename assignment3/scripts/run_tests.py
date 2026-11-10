import argparse
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import ROOT, now, save


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    destination = args.output or ROOT / ".runtime" / ("test-results-live.json" if args.live else "test-results-offline.json")
    build = ROOT / "build"
    project = ROOT.parent
    python = ROOT / ".venv/bin/python"
    commands = [
        [str(build / "tests")],
        [str(build / "v03_tests")],
        [str(python), str(project / "tests/test_cli.py"), str(build / "blockchain")],
        [str(build / "assignment3/cpp/bitcoin_merkle_tests"), str(ROOT / "fixtures/block100000-txids.txt"),
         str(ROOT / "evidence/lab/block-txids.txt"), str(ROOT / "evidence/lab/merkleroot.txt")],
        [str(python), "-m", "unittest", "discover", "-s", str(ROOT / "tests"), "-p", "test_offline.py", "-v"],
        ["ctest", "--test-dir", str(build), "--output-on-failure"],
    ]
    if args.live:
        commands.append([str(python), "-m", "unittest", "discover", "-s", str(ROOT / "tests"),
                         "-p", "test_live.py", "-v"])
    report = {"observed_at": now(), "kind": "actual executed test output", "live_requested": args.live, "results": []}
    for command in commands:
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        output = result.stdout + result.stderr
        report["results"].append({"command": command, "exit": result.returncode, "output": output})
        save(destination, report)
        print(output, end="")
        if result.returncode or "skipped=" in output or "***Skipped" in output:
            raise RuntimeError("A test command failed or skipped tests")
    report["all_passed"] = True
    save(destination, report)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        sys.exit("Error: " + str(error))
