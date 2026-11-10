import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import DEFAULT_EVIDENCE, RPC, btc, load, now, save, satoshi


def wallets(rpc):
    loaded = rpc.call("listwallets")
    existing = {wallet["name"] for wallet in rpc.call("listwalletdir")["wallets"]}
    for name in ["MIF", "student"]:
        if name in loaded:
            continue
        if name in existing:
            rpc.call("loadwallet", name)
        else:
            rpc.call("createwallet", wallet_name=name, load_on_startup=True)


def fund(rpc, evidence):
    info = rpc.require_regtest()
    owner = load(rpc.datadir / ".bgt3-owner.json")
    path = evidence / "run.json"
    if path.exists():
        state = load(path)
        if state["lab_id"] != owner["lab_id"]:
            raise ValueError("Evidence belongs to another laboratory")
        if state["status"] not in ["funded", "confirmed"]:
            raise ValueError("Unfinished experiment: inspect run.json and the wallet before retrying")
        wallets(rpc)
        return state
    if rpc.call("getrawmempool"):
        raise ValueError("Mempool must be empty before an isolated experiment")
    wallets(rpc)
    state = {"lab_id": owner["lab_id"], "started_at": now(), "chain": "regtest",
             "initial_height": info["blocks"], "status": "funding",
             "mining_address": rpc.call("getnewaddress", "mining", "bech32", wallet="MIF"),
             "recipient": rpc.call("getnewaddress", "payment", "bech32", wallet="student")}
    save(path, state)
    blocks = rpc.call("generatetoaddress", 101, state["mining_address"])
    balances = {name: rpc.call("getbalances", wallet=name) for name in ["MIF", "student"]}
    height = rpc.call("getblockcount")
    if len(blocks) != 101 or height != info["blocks"] + 101:
        raise ValueError("Mining did not produce the expected 101 blocks")
    if info["blocks"] == 0:
        mine = balances["MIF"]["mine"]
        if satoshi(mine["trusted"]) != 5000000000 or satoshi(mine["immature"]) != 500000000000:
            raise ValueError("Fresh-chain coinbase maturity balances differ from expected 50/5000 BTC")
    save(evidence / "mined-blocks.json", blocks)
    save(evidence / "funded-balances.json", {"observed_at": now(), "height": height, "wallets": balances})
    state.update(status="funded", funded_height=height)
    save(path, state)
    return state


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["fund"])
    parser.add_argument("--evidence", type=Path, default=DEFAULT_EVIDENCE)
    args = parser.parse_args()
    state = fund(RPC(), args.evidence)
    print("Wallets ready; experiment status:", state["status"], "height:", state["funded_height"])


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        sys.exit("Error: " + str(error))
