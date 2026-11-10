import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from bitcoin_data import bitcoin_merkle, transaction_analysis, transaction_metrics, verify_header
from common import DEFAULT_EVIDENCE, ROOT, RPC, load, now, save, satoshi


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / ".runtime/audit.json")
    args = parser.parse_args()
    checks = []

    def check(name, condition, evidence):
        if not condition:
            raise ValueError("Audit failed: " + name)
        checks.append({"requirement": name, "result": "passed", "evidence": evidence})

    lab = ROOT / "evidence/lab"
    state = load(lab / "run.json")
    block = load(lab / "block.json")
    tx = load(lab / "transaction.json")
    before = load(lab / "before.json")
    after = load(lab / "after.json")
    installation = load(ROOT / "evidence/core-installation.json")
    archive = ROOT / ".deps/downloads" / installation["archive"]
    check("Core archive authenticity", hashlib.sha256(archive.read_bytes()).hexdigest() == installation["sha256"]
          and installation["gpg_exit"] == 0 and len(installation["valid_signatures"]) == 8,
          ["core-installation.json"])
    for name, expected in installation["binary_sha256"].items():
        binary = ROOT / ".deps/bitcoin-32.1/bin" / name
        check("Unmodified official " + name, hashlib.sha256(binary.read_bytes()).hexdigest() == expected,
              ["core-installation.json"])
    initial = load(ROOT / "evidence/node-start.json")
    check("Initial isolated regtest", initial["blockchain"]["chain"] == "regtest" and
          initial["blockchain"]["blocks"] == initial["blockchain"]["headers"] == initial["connections"] == 0,
          ["node-start.json"])
    balances = load(lab / "funded-balances.json")
    check("Coinbase maturity", balances["height"] == 101 and
          satoshi(balances["wallets"]["MIF"]["mine"]["trusted"]) == 5000000000 and
          satoshi(balances["wallets"]["MIF"]["mine"]["immature"]) == 500000000000,
          ["lab/funded-balances.json", "lab/mined-blocks.json"])
    check("Mempool before confirmation", state["txid"] in before["mempool"] and
          before["receiver"]["confirmations"] == 0, ["lab/before.json"])
    check("Confirmed receiver payment", state["txid"] not in after["mempool"] and
          after["receiver"]["confirmations"] >= 1 and state["txid"] in [item["txid"] for item in block["tx"]],
          ["lab/after.json", "lab/block.json"])
    analysis = transaction_analysis(tx, load(lab / "prevouts.json"), state["recipient"],
                                    load(lab / "output-wallet-info.json"), after["sender"]["fee"],
                                    load(lab / "previous-transactions.json"))
    check("Exact indexed prevouts and fee", analysis["fee_sat"] == analysis["sender_wallet_fee_abs_sat"] == 282
          and analysis["fee_sat_vb"] == "2", ["transaction-analysis.json", "lab/prevouts.json"])
    check("Recipient independent of vout zero", analysis["vout"][1]["role"] == "recipient" and
          analysis["vout"][1]["value_sat"] == 100000000 and analysis["vout"][0]["role"].startswith("change"),
          ["transaction-analysis.json", "lab/output-wallet-info.json"])
    modern = transaction_metrics(tx["hex"], tx)
    legacy = transaction_metrics(load(ROOT / "fixtures/legacy.json")["raw"])
    check("SegWit and legacy serialization", modern["weight"] == 561 and modern["vsize"] == 141 and
          modern["txid"] != modern["wtxid"] and legacy["weight"] == 4 * legacy["size"] and
          legacy["txid"] == legacy["wtxid"], ["transaction-analysis.json", "../fixtures/legacy.json"])
    executable = ROOT / "build/assignment3/cpp/bitcoin_merkle"
    historical = subprocess.check_output([str(executable), "--file", str(ROOT / "fixtures/block100000-txids.txt")], text=True).strip()
    check("Libbitcoin historical root", historical == "f3e94742aca4b5ef85488dc37c06c3282295ffec960994b2c0d5ac2a25a95766",
          ["merkle-comparison.json", "libbitcoin.json"])
    regtest = subprocess.check_output([str(executable), "--file", str(lab / "block-txids.txt")], text=True).strip()
    check("Libbitcoin regtest root", regtest == block["merkleroot"] == bitcoin_merkle([item["txid"] for item in block["tx"]]),
          ["lab/block.json", "merkle-comparison.json"])
    comparison = subprocess.check_output([str(executable), "--compare-custom", "--file", str(ROOT / "fixtures/block100000-txids.txt")], text=True)
    check("Educational and Bitcoin hashing distinct", "different=true" in comparison, ["merkle-comparison.json"])
    header = verify_header(block, (lab / "header.hex").read_text())
    check("Independent 80 byte header", header["bytes"] == 80 and header["hash_match"] and header["hex_match"] and
          header["calculated_hash"] != header["modified_nonce_hash"], ["block-analysis.json", "lab/header.hex"])
    check("Coinbase and change included in output sum", load(ROOT / "evidence/block-analysis.json")["all_outputs_sat"] == 10000000000,
          ["block-analysis.json"])
    tests = load(ROOT / "evidence/test-results.json")
    output = "\n".join(result["output"] for result in tests["results"])
    check("Executed offline and live tests", tests["all_passed"] and all(result["exit"] == 0 for result in tests["results"])
          and "Ran 37 tests" in output and "Ran 6 tests" in output and "17 checks passed" in output,
          ["test-results.json"])
    reproduction = load(ROOT / "evidence/reproduction-check.json")
    check("Fresh run and rerun safety", reproduction["initial_height"] == 0 and reproduction["confirmation_height"] == 102
          and reproduction["reference_fixtures_unchanged"], ["reproduction-check.json"])
    tags = load(ROOT / "evidence/specification.json")["preserved_tags"]
    for tag, expected in tags.items():
        actual = subprocess.check_output(["git", "rev-parse", tag], text=True, cwd=ROOT).strip()
        check("Preserved " + tag, actual == expected, ["specification.json"])
    changed = subprocess.check_output(["git", "diff", "v0.3", "--", "src"], cwd=ROOT.parent, text=True)
    check("Assignment Two source preserved", not changed, ["test-results.json"])
    files = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard", "assignment3"],
                                    cwd=ROOT.parent, text=True).splitlines()
    for name in files:
        check("Safe tracked path " + name, not any(part in {".cookie", "wallet.dat", "wallets", ".venv", ".deps", ".runtime"}
                                                    for part in Path(name).parts), [name])
        path = ROOT.parent / name
        if path.suffix in {".json", ".txt", ".hex", ".md", ".py"}:
            text = path.read_text()
            if re.search(r"(?:tprv|xprv)[A-Za-z0-9]{40,}|gh[pousr]_[A-Za-z0-9]{20,}", text):
                raise ValueError("Possible secret in " + name)
    if args.live:
        rpc = RPC()
        rpc.require_regtest()
        current = load(DEFAULT_EVIDENCE / "run.json")
        owned = load(rpc.datadir / ".bgt3-owner.json")
        check("Current live node identity", owned["lab_id"] == current["lab_id"], ["lab/run.json"])
        check("P2P disabled", rpc.call("getnetworkinfo")["networkactive"] is False and rpc.call("getconnectioncount") == 0,
              ["node-start.json"])
        listeners = [line.split()[1] for line in Path("/proc/net/tcp").read_text().splitlines()[1:]
                     if line.split()[3] == "0A" and int(line.split()[1].split(":")[1], 16) == rpc.port]
        check("RPC bound to loopback", listeners == ["0100007F:" + format(rpc.port, "04X")], ["node-start.json"])
    report = {"observed_at": now(), "kind": "executed evidence audit against PDF requirements",
              "live_requested": args.live, "all_passed": True, "checks": checks,
              "human_review": "Lecturer reviews understanding and grading; no full-consensus claim for Python hash checks"}
    save(args.output, report)
    print(len(checks), "audit checks passed")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.SubprocessError) as error:
        sys.exit("Error: " + str(error))
