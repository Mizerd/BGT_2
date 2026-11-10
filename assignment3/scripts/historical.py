import argparse
from pathlib import Path
import sys
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from bitcoin.core import CBlock, b2lx
from bitcoin_data import bitcoin_merkle, sha256d
from common import ROOT, btc, now, save


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / ".runtime/historical-block277316.json")
    args = parser.parse_args()
    source = "https://blockstream.info/api"
    with urllib.request.urlopen(source + "/block-height/277316", timeout=60) as response:
        block_hash = response.read().decode().strip()
    with urllib.request.urlopen(source + "/block/" + block_hash + "/raw", timeout=60) as response:
        raw = response.read()
    block = CBlock.deserialize(raw)
    if sha256d(raw[:80])[::-1].hex() != block_hash:
        raise ValueError("Historical raw block hash does not match its source")
    txids = [b2lx(transaction.GetTxid()) for transaction in block.vtx]
    if bitcoin_merkle(txids) != raw[36:68][::-1].hex():
        raise ValueError("Historical transaction Merkle root mismatch")
    total = sum(output.nValue for transaction in block.vtx for output in transaction.vout)
    if total != 1032207722534:
        raise ValueError("Block 277316 output sum differs from the assignment reference")
    result = {"observed_at": now(), "kind": "historical public HTTP data, not mainnet RPC",
              "height": 277316, "hash": block_hash, "source": source + "/block/" + block_hash + "/raw",
              "raw_bytes": len(raw), "transactions": len(txids), "all_outputs_sat": total,
              "all_outputs_btc": btc(total), "header_hash_verified": True, "merkle_verified": True,
              "reference_output_sum_matches": True}
    save(args.output, result)
    print(result)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        sys.exit("Error: " + str(error))
