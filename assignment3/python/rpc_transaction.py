import argparse
import json
from pathlib import Path
import sys

from bitcoin_data import transaction_analysis
from common import DEFAULT_EVIDENCE, RPC, load, now, save


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", type=Path, default=DEFAULT_EVIDENCE)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    state = load(args.evidence / "run.json")
    if state.get("status") != "confirmed":
        raise ValueError("The experiment has no confirmed payment")
    if args.offline:
        transaction = load(args.evidence / "transaction.json")
        sender = load(args.evidence / "after.json")["sender"]
    else:
        rpc = RPC()
        rpc.require_regtest()
        transaction = rpc.call("getrawtransaction", state["txid"], True, state["block_hash"])
        sender = rpc.call("gettransaction", state["txid"], wallet="MIF")
    if transaction.get("blockhash") != state["block_hash"] or transaction.get("confirmations", 0) < 1:
        raise ValueError("Transaction is not confirmed in the recorded block")
    result = transaction_analysis(transaction, load(args.evidence / "prevouts.json"), state["recipient"],
                                  load(args.evidence / "output-wallet-info.json"), sender["fee"],
                                  load(args.evidence / "previous-transactions.json"))
    if sum(output["value_sat"] for output in result["vout"] if output["role"] == "recipient") != 100000000:
        raise ValueError("The recipient did not receive exactly 1 BTC")
    result.update(observed_at=now(), source="saved RPC fixture" if args.offline else "live local Bitcoin Core RPC")
    if args.output:
        save(args.output, result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, KeyError) as error:
        sys.exit("Error: " + str(error))
