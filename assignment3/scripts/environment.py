import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from common import ROOT, now


def command(*arguments):
    return subprocess.check_output(arguments, text=True, stderr=subprocess.STDOUT).strip()


def main():
    disk = shutil.disk_usage(ROOT)
    memory = {line.split(":")[0]: int(line.split()[1]) for line in Path("/proc/meminfo").read_text().splitlines()}
    result = {"observed_at": now(), "os_release": Path("/etc/os-release").read_text(),
              "architecture": platform.machine(), "python": sys.version,
              "compiler": command("c++", "--version").splitlines()[0],
              "cmake": command("cmake", "--version").splitlines()[0], "git": command("git", "--version"),
              "pkg_config": command("pkg-config", "--version"),
              "libbitcoin_pkg_config": command("pkg-config", "--modversion", "libbitcoin"),
              "libbitcoin_flags": command("pkg-config", "--cflags", "--libs", "libbitcoin"),
              "secp256k1": command("pkg-config", "--modversion", "libsecp256k1"),
              "bitcoind": command(str(ROOT / ".deps/bin/bitcoind"), "--version").splitlines()[0],
              "bitcoin_cli": command(str(ROOT / ".deps/bin/bitcoin-cli"), "--version").splitlines()[0],
              "python_packages": command(str(ROOT / ".venv/bin/python"), "-m", "pip", "freeze"),
              "disk_free_bytes": disk.free, "memory_total_kib": memory["MemTotal"],
              "memory_available_kib": memory["MemAvailable"]}
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
