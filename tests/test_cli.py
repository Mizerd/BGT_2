import subprocess
import sys


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run(binary, arguments):
    return subprocess.run([binary, *arguments], capture_output=True, text=True, timeout=30)


def main():
    binary = sys.argv[1]
    invalid = [
        ["--attempt-budget", "0"],
        ["--max-attempts", "0"],
        ["--attempt-budget", "-1"],
        ["--max-attempts", "1.5"],
        ["--max-attempts"],
        ["--difficulty", "65"],
        ["--users", "1"],
        ["--seed", "18446744073709551616"],
    ]
    for arguments in invalid:
        result = run(binary, arguments)
        check(result.returncode == 1 and result.stderr.startswith("Error:"), str(arguments))

    result = run(binary, ["--help"])
    check(result.returncode == 0 and "--max-attempts" in result.stdout, "help lists mining options")
    result = run(binary, ["--users", "2", "--transactions", "0"])
    check(result.returncode == 0 and "Accepted transactions: 0" in result.stdout and
          "Remaining transactions: 0" in result.stdout, "empty dataset terminates normally")
    result = run(binary, ["--users", "2", "--transactions", "1", "--difficulty", "0",
                          "--attempt-budget", "100", "--max-attempts", "1"])
    check(result.returncode == 0 and result.stdout.count("attempts=1 |") == 6 and
          "Total mining attempts: 5" in result.stdout, "budget is capped for all five candidates")

    result = run(binary, ["--users", "2", "--transactions", "1", "--difficulty", "64",
                          "--attempt-budget", "2", "--max-attempts", "7"])
    check(result.returncode == 1, "exhausted search reports failure")
    check(result.stdout.count("attempts=7 |") == 5, "each candidate respects its attempt limit")
    for expected in ["Accepted transactions: 0", "Remaining transactions: 1", "Mining rounds: 3",
                     "Total mining attempts: 35", "mining attempt limit reached", "Invariants: passed"]:
        check(expected in result.stdout, expected)
    print("12/12 CLI checks passed")


if __name__ == "__main__":
    main()
