from decimal import Decimal
import hashlib

from bitcoin.core import CTransaction, b2lx
from bitcoin.core.serialize import SerializationError

from common import btc, satoshi


def sha256d(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()


def transaction_metrics(raw_hex, decoded=None):
    try:
        raw = bytes.fromhex(raw_hex)
        transaction = CTransaction.deserialize(raw)
    except (ValueError, SerializationError) as error:
        raise ValueError("Invalid raw transaction serialization") from error
    base = transaction.serialize({"include_witness": False})
    weight = 3 * len(base) + len(raw)
    txid = sha256d(base)[::-1].hex()
    wtxid = sha256d(raw)[::-1].hex()
    if b2lx(transaction.GetTxid()) != txid or b2lx(transaction.GetHash()) != wtxid or transaction.calc_weight() != weight:
        raise ValueError("Library and independent transaction measurements disagree")
    result = {"txid": txid, "wtxid": wtxid, "size": len(raw), "base_size": len(base),
              "weight": weight, "vsize": (weight + 3) // 4, "has_witness": not transaction.wit.is_null()}
    scripts = []
    for index, item in enumerate(transaction.vin):
        witness = []
        if index < len(transaction.wit.vtxinwit):
            witness = [value.hex() for value in transaction.wit.vtxinwit[index].scriptWitness.stack]
        scripts.append({"scriptSig": {"hex": bytes(item.scriptSig).hex()}, "txinwitness": witness})
    result["vin_scripts"] = scripts
    if decoded is not None:
        for name in ["txid", "size", "weight", "vsize"]:
            if decoded.get(name) != result[name]:
                raise ValueError("RPC transaction field disagrees with raw bytes: " + name)
        if decoded.get("hash") != wtxid:
            raise ValueError("RPC wtxid disagrees with raw bytes")
    return result


def transaction_analysis(transaction, prevouts, recipient=None, ownership=None, wallet_fee=None, previous=None):
    if not isinstance(transaction, dict) or not transaction.get("vin") or not transaction.get("vout"):
        raise ValueError("Decoded transaction must contain vin and vout")
    ownership = ownership or {}
    previous = previous or {}
    lookup = {}
    for output in prevouts:
        key = (output["txid"], output["vout"])
        value = satoshi(output["amount"])
        if key in lookup and lookup[key] != value:
            raise ValueError("Conflicting prevout values")
        lookup[key] = value
    coinbase = "coinbase" in transaction["vin"][0]
    if any("coinbase" in item for item in transaction["vin"]) and (not coinbase or len(transaction["vin"]) != 1):
        raise ValueError("Coinbase cannot be mixed with ordinary inputs")
    inputs = []
    input_total = 0
    for item in transaction["vin"]:
        if coinbase:
            inputs.append({"coinbase": item["coinbase"], "txinwitness": item.get("txinwitness", [])})
            continue
        key = (item["txid"], item["vout"])
        value = lookup.get(key)
        source = "saved listunspent (txid,vout)"
        old_transaction = previous.get(item["txid"])
        script = None
        if old_transaction is not None:
            old = next((output for output in old_transaction["vout"] if output["n"] == item["vout"]), None)
            if old is None:
                raise ValueError(f"Missing previous transaction output: {key}")
            historical_value = satoshi(old["value"])
            if value is not None and value != historical_value:
                raise ValueError(f"Prevout evidence disagrees: {key}")
            value = historical_value
            script = old.get("scriptPubKey")
            source = "previous decoded transaction; cross-checked with saved UTXO when present"
        if value is None:
            raise ValueError(f"Missing prevout: {item['txid']}:{item['vout']}")
        input_total += value
        inputs.append({"txid": item["txid"], "vout": item["vout"], "scriptSig": item.get("scriptSig", {}),
                       "txinwitness": item.get("txinwitness", []), "previous_value_sat": value,
                       "previous_value_btc": btc(value), "previous_scriptPubKey": script, "source": source})
    outputs = []
    output_total = 0
    for output in transaction["vout"]:
        value = satoshi(output["value"])
        output_total += value
        script = output["scriptPubKey"]
        address = script.get("address")
        role = "unknown"
        if address and address == recipient:
            role = "recipient"
        elif address and ownership.get(address, {}).get("ismine") and ownership[address].get("ischange"):
            role = "change (sender wallet evidence)"
        outputs.append({"n": output["n"], "value_sat": value, "value_btc": btc(value),
                        "script_type": script.get("type", "unknown"), "script_hex": script.get("hex"),
                        "address": address, "role": role})
    fee = None
    rate = None
    checked_fee = None
    if not coinbase:
        fee = input_total - output_total
        if fee < 0:
            raise ValueError("Negative transaction fee")
        size = transaction.get("vsize")
        if not isinstance(size, int) or size <= 0:
            raise ValueError("Transaction vsize must be positive")
        rate = str(Decimal(fee) / size)
        if wallet_fee is not None:
            if isinstance(wallet_fee, (float, bool)):
                raise ValueError("Wallet fee must not be a binary float")
            checked_fee = satoshi(abs(Decimal(wallet_fee)))
            if checked_fee != fee:
                raise ValueError("Calculated fee differs from sender wallet fee")
    return {"txid": transaction.get("txid"), "coinbase": coinbase, "vin": inputs, "vout": outputs,
            "input_total_sat": None if coinbase else input_total, "output_total_sat": output_total,
            "fee_sat": fee, "fee_btc": None if fee is None else btc(fee), "fee_sat_vb": rate,
            "sender_wallet_fee_abs_sat": checked_fee}
