import argparse
import json
from pathlib import Path
import sys

from common import RPC, now, save


def observe(rpc):
    chain = rpc.require_regtest()
    network = rpc.call("getnetworkinfo")
    return {"observed_at": now(), "source": "live local Bitcoin Core RPC via bitcoin-cli",
            "chain": chain["chain"], "blocks": chain["blocks"], "headers": chain["headers"],
            "initialblockdownload": chain["initialblockdownload"], "bestblockhash": chain["bestblockhash"],
            "core_version": network["version"], "core_subversion": network["subversion"],
            "connections": rpc.call("getconnectioncount")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = observe(RPC())
    if args.output:
        save(args.output, result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        sys.exit("Error: " + str(error))
