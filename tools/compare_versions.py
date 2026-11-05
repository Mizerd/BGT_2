import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


EXPECTED_FAILURES = {
    "stale solved candidate",
    "competition context",
    "competition cached proof",
    "competition existing budget",
}


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run(command, expected=0):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=120)
    output = result.stdout + result.stderr
    check(result.returncode == expected, f"Command failed: {command}\n{output}")
    return output


def cache(build, name):
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(name + ":"):
            return line.split("=", 1)[1]
    raise AssertionError(f"Missing CMake setting: {name}")


def summary(output):
    match = re.search(r"(\d+)/(\d+) tests passed", output)
    check(match is not None, "Missing test summary")
    return tuple(int(value) for value in match.groups())


def main():
    parser = argparse.ArgumentParser(description="Compare frozen v0.2 with the current implementation")
    parser.add_argument("baseline", type=Path, help="unchanged v0.2 worktree with build/cmake")
    parser.add_argument("current_build", type=Path, nargs="?", default=Path("build/cmake"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    baseline = args.baseline.resolve()
    old_build = baseline / "build/cmake"
    current = args.current_build.resolve()

    frozen = run(["git", "-C", root, "rev-parse", "v0.2^{}"]).strip()
    check(run(["git", "-C", baseline, "rev-parse", "HEAD"]).strip() == frozen, "Baseline is not v0.2")
    check(not run(["git", "-C", baseline, "status", "--porcelain", "--untracked-files=no"]).strip(),
          "Baseline has modified tracked files")
    for name in ["tests/test_blockchain.cpp", "src/models.cpp", "src/models.hpp",
                 "src/hashing.hpp", "src/custom_hash.cpp", "src/custom_hash.hpp"]:
        check((baseline / name).read_bytes() == (root / name).read_bytes(), f"Shared input changed: {name}")
    for name in ["CMAKE_CXX_COMPILER", "CMAKE_BUILD_TYPE", "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE"]:
        check(cache(old_build, name) == cache(current, name), f"Different build setting: {name}")
    check(cache(current, "CMAKE_BUILD_TYPE") == "Release", "Use Release for both builds")

    old_shared = summary(run([old_build / "tests"]))
    new_shared = summary(run([current / "tests"]))
    check(old_shared == new_shared == (38, 38), "Shared correctness tests differ")
    new_regressions = summary(run([current / "v03_tests"]))
    check(new_regressions[0] == new_regressions[1], "Current regressions must pass")

    with tempfile.TemporaryDirectory(prefix="bgt2-comparison-") as directory:
        probe = Path(directory) / "baseline_regressions"
        flags = shlex.split(cache(old_build, "CMAKE_CXX_FLAGS") + " " + cache(old_build, "CMAKE_CXX_FLAGS_RELEASE"))
        command = [cache(old_build, "CMAKE_CXX_COMPILER"), "-std=c++20", *flags, "-I" + str(baseline / "src")]
        command += [baseline / "src" / name for name in ["custom_hash.cpp", "models.cpp", "chain.cpp"]]
        command += [root / "tests/test_v03.cpp", "-o", probe]
        run(command)
        old_output = run([probe], expected=1)
    failures = set(re.findall(r"^FAIL ([^:]+):", old_output, flags=re.M))
    check(failures == EXPECTED_FAILURES, f"Unexpected baseline failures: {failures}")
    old_regressions = summary(old_output)
    check(old_regressions == (new_regressions[1] - len(EXPECTED_FAILURES), new_regressions[1]),
          "Unexpected baseline regression count")
    for build in [old_build, current]:
        check("12/12 CLI checks passed" in run([sys.executable, root / "tests/test_cli.py", build / "blockchain"]),
              "CLI checks did not pass")

    print(f"Frozen v0.2: {frozen}")
    print("Shared tests: v0.2 38/38; v0.3 38/38")
    print(f"New groups: v0.2 {old_regressions[0]}/{old_regressions[1]}; "
          f"v0.3 {new_regressions[0]}/{new_regressions[1]}")
    print("Expected baseline failures: " + ", ".join(sorted(failures)))
    print("CLI: 12/12 on each version")


if __name__ == "__main__":
    main()
