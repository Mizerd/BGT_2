import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import DEFAULT_EVIDENCE, RPC, load, now, save, satoshi


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


def payment(rpc, evidence, state):
    if state["status"] == "confirmed":
        block = rpc.call("getblock", state["block_hash"], 1)
        if state["txid"] not in block["tx"]:
            raise ValueError("Recorded payment is not in its recorded block")
        print("Existing completed experiment reused; no new payment or blocks")
        return state
    if rpc.call("getblockcount") != state["funded_height"] or rpc.call("getrawmempool"):
        raise ValueError("Chain or mempool changed since funding; refusing to mix experiments")
    prevouts = rpc.call("listunspent", wallet="MIF")
    public_fields = {"txid", "vout", "address", "scriptPubKey", "amount", "confirmations", "spendable"}
    save(evidence / "prevouts.json", [{key: value for key, value in output.items() if key in public_fields}
                                      for output in prevouts])
    state["status"] = "sending"
    save(evidence / "run.json", state)
    txid = rpc.call("sendtoaddress", address=state["recipient"], amount="1", fee_rate=2,
                    comment="bgt3-" + state["lab_id"], wallet="MIF")
    state.update(txid=txid, requested_fee_sat_vb=2, status="unconfirmed")
    save(evidence / "run.json", state)
    before = {"observed_at": now(), "mempool": rpc.call("getrawmempool"),
              "mempool_info": rpc.call("getmempoolinfo"), "entry": rpc.call("getmempoolentry", txid),
              "sender": rpc.call("gettransaction", txid, wallet="MIF"),
              "receiver": rpc.call("gettransaction", txid, wallet="student")}
    if txid not in before["mempool"] or before["sender"]["confirmations"] != 0 or before["receiver"]["confirmations"] != 0:
        raise ValueError("Expected an unconfirmed payment in mempool")
    save(evidence / "before.json", before)
    block_hash = rpc.call("generatetoaddress", 1, state["mining_address"])[0]
    state.update(block_hash=block_hash, status="capturing")
    save(evidence / "run.json", state)
    block = rpc.call("getblock", block_hash, 2)
    raw = rpc.call("getrawtransaction", txid, False, block_hash)
    decoded = rpc.call("getrawtransaction", txid, True, block_hash)
    after = {"observed_at": now(), "mempool": rpc.call("getrawmempool"),
             "sender": rpc.call("gettransaction", txid, wallet="MIF"),
             "receiver": rpc.call("gettransaction", txid, wallet="student")}
    if txid in after["mempool"] or after["receiver"]["confirmations"] < 1:
        raise ValueError("Payment failed confirmation checks")
    if txid not in [transaction["txid"] for transaction in block["tx"]]:
        raise ValueError("Confirmation block does not contain payment")
    previous = {}
    for item in decoded["vin"]:
        wallet_tx = rpc.call("gettransaction", item["txid"], wallet="MIF")
        previous[item["txid"]] = rpc.call("decoderawtransaction", wallet_tx["hex"])
    ownership = {}
    for output in decoded["vout"]:
        address = output["scriptPubKey"].get("address")
        if address:
            info = rpc.call("getaddressinfo", address, wallet="MIF")
            ownership[address] = {key: info[key] for key in ["ismine", "ischange", "iswitness"] if key in info}
    save(evidence / "after.json", after)
    save(evidence / "transaction.json", decoded)
    save(evidence / "previous-transactions.json", previous)
    save(evidence / "output-wallet-info.json", ownership)
    save(evidence / "block.json", block)
    (evidence / "transaction.hex").write_text(raw + "\n")
    (evidence / "header.hex").write_text(rpc.call("getblockheader", block_hash, False) + "\n")
    (evidence / "block-txids.txt").write_text("\n".join(transaction["txid"] for transaction in block["tx"]) + "\n")
    state.update(status="confirmed", completed_at=now(), block_height=block["height"])
    save(evidence / "run.json", state)
    return state


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["fund", "run"])
    parser.add_argument("--evidence", type=Path, default=DEFAULT_EVIDENCE)
    args = parser.parse_args()
    rpc = RPC()
    state = fund(rpc, args.evidence)
    if args.action == "run":
        state = payment(rpc, args.evidence, state)
        print("Payment:", state["txid"], "block:", state["block_hash"])
    print("Wallets ready; experiment status:", state["status"], "height:", state["funded_height"])


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        sys.exit("Error: " + str(error))
