import hashlib
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / ".deps"
VERSION = "32.1"
ARCHIVE = f"bitcoin-{VERSION}-x86_64-linux-gnu.tar.gz"
CHECKSUM = "61545c17c3120fb7412f896d248b11993af04ec8817aad3649c55b2afb044963"
KEYS_COMMIT = "f432c43b5fbd6b8355daf36127acb7a73d0308b7"
FINGERPRINT = "E777299FC265DD04793070EB944D35F9AC3DB76A"


def run(command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout.strip()


def download(url, destination):
    if not destination.exists():
        with urllib.request.urlopen(url, timeout=120) as response:
            destination.write_bytes(response.read())


def main():
    if os.uname().machine != "x86_64" or sys.platform != "linux":
        raise RuntimeError("This pinned installer targets Linux x86_64; use verified native Core binaries elsewhere")
    downloads = DEPS / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    for filename in [ARCHIVE, "SHA256SUMS", "SHA256SUMS.asc"]:
        download(f"https://bitcoincore.org/bin/bitcoin-core-{VERSION}/{filename}", downloads / filename)
    actual = hashlib.sha256((downloads / ARCHIVE).read_bytes()).hexdigest()
    signed = next(line.split()[0] for line in (downloads / "SHA256SUMS").read_text().splitlines()
                  if line.split()[-1] == ARCHIVE)
    if actual != CHECKSUM or signed != CHECKSUM:
        raise RuntimeError("Bitcoin Core archive checksum mismatch")
    keys = DEPS / "guix.sigs"
    if not keys.exists():
        run(["git", "init", str(keys)])
        run(["git", "-C", str(keys), "remote", "add", "origin", "https://github.com/bitcoin-core/guix.sigs.git"])
        run(["git", "-C", str(keys), "fetch", "--depth", "1", "origin", KEYS_COMMIT])
        run(["git", "-C", str(keys), "checkout", "--detach", "FETCH_HEAD"])
    if run(["git", "-C", str(keys), "rev-parse", "HEAD"]) != KEYS_COMMIT:
        raise RuntimeError("Wrong release-key source commit")
    keyring = DEPS / "gnupg"
    keyring.mkdir(mode=0o700, exist_ok=True)
    run(["gpg", "--homedir", str(keyring), "--batch", "--import",
         *map(str, sorted((keys / "builder-keys").glob("*.gpg")))])
    verification = subprocess.run(["gpg", "--homedir", str(keyring), "--batch", "--status-fd", "1",
                                   "--verify", str(downloads / "SHA256SUMS.asc"), str(downloads / "SHA256SUMS")],
                                  capture_output=True, text=True)
    if verification.returncode != 0 or "[GNUPG:] BADSIG " in verification.stdout:
        raise RuntimeError("Release signature verification failed: " + verification.stderr)
    signatures = [line for line in verification.stdout.splitlines() if line.startswith("[GNUPG:] VALIDSIG ")]
    if not any(FINGERPRINT in line for line in signatures):
        raise RuntimeError("Expected release signer is missing")
    core = DEPS / f"bitcoin-{VERSION}"
    if not core.exists():
        with tarfile.open(downloads / ARCHIVE) as archive:
            archive.extractall(DEPS, filter="data")
    binary_hashes = {}
    with tarfile.open(downloads / ARCHIVE) as archive:
        for name in ["bitcoind", "bitcoin-cli"]:
            original = archive.extractfile(f"bitcoin-{VERSION}/bin/{name}").read()
            installed = (core / "bin" / name).read_bytes()
            if original != installed:
                raise RuntimeError("Installed Core binary differs from the verified archive: " + name)
            binary_hashes[name] = hashlib.sha256(installed).hexdigest()
    loader = []
    if Path("/etc/NIXOS").exists():
        for candidate in sorted(Path("/nix/store").glob("*glibc-*/lib/ld-linux-x86-64.so.2")):
            result = subprocess.run([str(candidate), str(core / "bin/bitcoind"), "--version"],
                                    capture_output=True, text=True, timeout=10)
            if result.returncode == 0 and "v32.1.0" in result.stdout:
                loader = [str(candidate)]
                break
        if not loader:
            raise RuntimeError("No compatible Nix glibc loader found")
    launchers = DEPS / "bin"
    launchers.mkdir(exist_ok=True)
    versions = {}
    for name in ["bitcoind", "bitcoin-cli"]:
        command = loader + [str(core / "bin" / name)]
        versions[name] = run(command + ["--version"])
        path = launchers / name
        path.write_text("#!/usr/bin/env bash\nexec " + shlex.join(command) + ' "$@"\n')
        path.chmod(0o755)
    venv = ROOT / ".venv"
    if not venv.exists():
        run([sys.executable, "-m", "venv", str(venv)])
    run([str(venv / "bin/python"), "-m", "pip", "install", "--require-hashes", "--only-binary=:all:",
         "-r", str(ROOT / "requirements.txt")])
    evidence = {"verified_at": datetime.now(timezone.utc).isoformat(),
                "archive": ARCHIVE, "sha256": actual, "source": "https://bitcoincore.org/en/download/",
                "key_source_commit": KEYS_COMMIT, "required_fingerprint": FINGERPRINT,
                "valid_signatures": signatures, "gpg_exit": verification.returncode,
                "versions": versions, "binary_modified": False, "binary_sha256": binary_hashes, "nix_loader": loader,
                "python": sys.version, "packages": run([str(venv / "bin/python"), "-m", "pip", "freeze"])}
    report = ROOT / ".runtime/core-installation.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(evidence, indent=2) + "\n")
    print("Verified official Core", VERSION, "with", len(signatures), "signatures; Python environment ready")


if __name__ == "__main__":
    main()
