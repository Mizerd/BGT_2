from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EVIDENCE = ROOT / "evidence/lab"
SATOSHIS = 100000000


def now():
    return datetime.now(timezone.utc).isoformat()


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, default=str) + "\n")
    temporary.replace(path)


def load(path):
    try:
        return json.loads(Path(path).read_text(), parse_float=Decimal)
    except (OSError, ValueError) as error:
        raise ValueError(f"Cannot read JSON data: {path}") from error


def satoshi(value):
    if isinstance(value, (float, bool)):
        raise ValueError("Money must be Decimal, integer or decimal text, not float")
    try:
        amount = Decimal(value) * SATOSHIS
    except (InvalidOperation, TypeError, ValueError) as error:
        raise ValueError("Invalid decimal BTC amount") from error
    if not amount.is_finite() or amount < 0 or amount != amount.to_integral_value():
        raise ValueError("Invalid BTC amount or fractional satoshi")
    return int(amount)


def btc(value):
    return format(Decimal(value) / SATOSHIS, ".8f")


class RPCError(RuntimeError):
    pass


class RPC:
    def __init__(self, datadir=None, executable=None):
        self.datadir = Path(datadir or os.environ.get("BGT3_DATADIR", Path.home() / "bitcoin-regtest-mif")).resolve()
        self.executable = str(executable or ROOT / ".deps/bin/bitcoin-cli")
        self.port = int(os.environ.get("BGT3_RPC_PORT", "18443"))

    def call(self, method, *arguments, wallet=None, **named):
        command = [self.executable, "-regtest", f"-datadir={self.datadir}", f"-rpcport={self.port}",
                   "-rpcclienttimeout=30"]
        if wallet:
            command.append(f"-rpcwallet={wallet}")
        if named:
            if arguments:
                raise ValueError("Do not mix positional and named RPC arguments")
            command.append("-named")
        command.append(method)
        encode = lambda value: value if isinstance(value, str) else json.dumps(value)
        command.extend(encode(value) for value in arguments)
        command.extend(key + "=" + encode(value) for key, value in named.items())
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=40)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise RPCError(f"RPC {method}: executable unavailable or request timed out") from error
        if result.returncode:
            raise RPCError(f"RPC {method}: {result.stderr.strip()}")
        text = result.stdout.strip()
        if method in {"getnewaddress", "sendtoaddress", "getblockhash", "getbestblockhash"}:
            return text
        if method in {"getrawtransaction", "getblockheader"} and not text.startswith("{"):
            return text
        try:
            return json.loads(text, parse_float=Decimal)
        except ValueError:
            return text

    def require_regtest(self):
        info = self.call("getblockchaininfo")
        if info.get("chain") != "regtest":
            raise RPCError("Refusing an RPC endpoint which is not regtest")
        return info
