import json
import os
from pathlib import Path
import subprocess
import sys
import uuid
import hashlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import ROOT, load, now, save


def main():
    python = str(ROOT / ".venv/bin/python")
    node = str(ROOT / "scripts/node.py")
    token = uuid.uuid4().hex[:12]
    evidence = ROOT / ".runtime" / ("reproduction-" + token)
    data = Path.home() / ("bitcoin-regtest-mif-reproduction-" + token)
    env = dict(os.environ, BGT3_DATADIR=str(data), BGT3_EVIDENCE=str(evidence))
    reference = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in (ROOT / "evidence/lab").iterdir() if path.is_file()}

    def run(arguments, environment=None):
        result = subprocess.run([python, *map(str, arguments)], env=environment,
                                capture_output=True, text=True, timeout=180)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return result.stdout

    original_running = subprocess.run([python, node, "status"], capture_output=True).returncode == 0
    if original_running:
        run([node, "stop"])
    try:
        run([node, "start"], env)
        run([ROOT / "scripts/lab.py", "run"], env)
        transaction = json.loads(run([ROOT / "python/rpc_transaction.py"], env))
        block = json.loads(run([ROOT / "python/rpc_block.py"], env))
        run([ROOT / "scripts/lab.py", "run"], env)
        tests = subprocess.run([python, "-m", "unittest", "discover", "-s", str(ROOT / "tests"),
                                "-p", "test_live.py", "-v"], env=env, capture_output=True, text=True, timeout=180)
        test_output = tests.stdout + tests.stderr
        if tests.returncode or "Ran 6 tests" not in test_output or "OK" not in test_output:
            raise ValueError("Fresh integration tests failed or did not execute all six cases: " + test_output)
        current = load(evidence / "run.json")
        report = {"observed_at": now(), "kind": "second fresh laboratory, actual live RPC",
                  "initial_height": current["initial_height"], "confirmation_height": current["block_height"],
                  "txid": current["txid"], "block_hash": current["block_hash"],
                  "fee_sat": transaction["fee_sat"], "fee_sat_vb": transaction["fee_sat_vb"],
                  "header_match": block["header"]["hash_match"], "live_tests_exit": tests.returncode,
                  "live_tests_output": test_output,
                  "data_retained_outside_git": str(data), "reference_fixture_hashes": reference}
    finally:
        run([node, "stop"], env)
        if original_running:
            run([node, "start"])
    after = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in (ROOT / "evidence/lab").iterdir() if path.is_file()}
    if after != reference:
        raise ValueError("Reference evidence changed during reproduction")
    report["reference_fixtures_unchanged"] = True
    save(ROOT / ".runtime/reproduction-check.json", report)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.SubprocessError) as error:
        sys.exit("Error: " + str(error))
