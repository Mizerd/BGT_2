import argparse
import json
from pathlib import Path
import sys

from bitcoin_data import bitcoin_merkle, block_output_total
from common import DEFAULT_EVIDENCE, RPC, btc, load, now, save


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, default=DEFAULT_EVIDENCE)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    state = load(args.evidence / "run.json")
    if state.get("status") != "confirmed":
        raise ValueError("The experiment has no confirmed block")
    if args.offline:
        block = load(args.evidence / "block.json")
    else:
        rpc = RPC()
        rpc.require_regtest()
        block = rpc.call("getblock", state["block_hash"], 2)
    txids = [transaction["txid"] for transaction in block["tx"]]
    if state["txid"] not in txids or block["hash"] != state["block_hash"]:
        raise ValueError("The recorded payment does not belong to this block")
    coinbase = "coinbase" in block["tx"][0]["vin"][0]
    root = bitcoin_merkle(txids)
    if not coinbase or root != block["merkleroot"]:
        raise ValueError("Coinbase or transaction Merkle root mismatch")
    total = block_output_total(block)
    result = {"observed_at": now(), "source": "saved RPC fixture" if args.offline else "live local Bitcoin Core RPC",
              "hash": block["hash"], "height": block["height"], "previousblockhash": block["previousblockhash"],
              "time": block["time"], "version": block["version"], "bits": block["bits"], "nonce": block["nonce"],
              "transaction_count": len(txids), "txids": txids, "coinbase_first": coinbase,
              "merkleroot": root, "node_merkleroot": block["merkleroot"],
              "all_outputs_sat": total, "all_outputs_btc": btc(total)}
    if args.output:
        save(args.output, result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, KeyError) as error:
        sys.exit("Error: " + str(error))
