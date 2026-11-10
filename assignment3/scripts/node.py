import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import ROOT, RPC, RPCError, load, now, save

PROJECT = "Mizerd/BGT_2/assignment3"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["start", "status", "stop"])
    args = parser.parse_args()
    rpc = RPC()
    marker = rpc.datadir / ".bgt3-owner.json"
    pidfile = rpc.datadir / "regtest/bgt3-bitcoind.pid"
    if args.action == "start" and not marker.exists():
        if rpc.datadir.exists() and any(rpc.datadir.iterdir()):
            raise RuntimeError("Existing unowned directory: choose a separate BGT3_DATADIR; no files were reset")
        rpc.datadir.mkdir(parents=True, exist_ok=True)
        save(marker, {"project": PROJECT, "lab_id": str(uuid.uuid4())})
    if not marker.exists() or load(marker).get("project") != PROJECT:
        raise RuntimeError("This is not a project-owned laboratory directory")
    running = False
    if pidfile.exists():
        pid = int(pidfile.read_text())
        process = Path(f"/proc/{pid}/cmdline")
        if process.exists():
            arguments = process.read_bytes().split(b"\0")
            running = b"-regtest" in arguments and f"-datadir={rpc.datadir}".encode() in arguments
            if not running:
                raise RuntimeError("PID belongs to another process; refusing to control it")
    if args.action == "stop":
        if not running:
            print("Project node is already stopped")
            return
        rpc.require_regtest()
        rpc.call("stop")
        for _ in range(100):
            if not pidfile.exists():
                print("Project-owned regtest node stopped; data retained")
                return
            time.sleep(0.1)
        raise RuntimeError("Node did not finish stopping")
    if args.action == "start" and not running:
        command = [str(ROOT / ".deps/bin/bitcoind"), "-regtest", f"-datadir={rpc.datadir}",
                   f"-pid={pidfile}", "-daemonwait", "-server", "-listen=0", "-networkactive=0",
                   "-discover=0", "-dnsseed=0", "-rpcbind=127.0.0.1", "-rpcallowip=127.0.0.1",
                   f"-rpcport={rpc.port}", "-port=18444"]
        subprocess.run(command, check=True, timeout=60)
    info = rpc.require_regtest()
    network = rpc.call("getnetworkinfo")
    observation = {"observed_at": now(), "lab_id": load(marker)["lab_id"], "blockchain": info,
                   "network": network, "connections": rpc.call("getconnectioncount"),
                   "rpc_port": rpc.port, "p2p_listening": False}
    if args.action == "start":
        save(ROOT / "evidence/node-start.json", observation)
    print(json.dumps(observation, indent=2, default=str))


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, subprocess.SubprocessError) as error:
        sys.exit("Error: " + str(error))
