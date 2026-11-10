from decimal import Decimal
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from bitcoin.core import CTransaction
from bitcoin_data import bitcoin_merkle, block_output_total, transaction_analysis, verify_header
from common import DEFAULT_EVIDENCE, ROOT, RPC, load


class LiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rpc = RPC()
        cls.rpc.require_regtest()
        cls.folder = DEFAULT_EVIDENCE
        cls.state = load(cls.folder / "run.json")
        if cls.state["status"] != "confirmed":
            raise ValueError("Run the laboratory before integration tests")
        owner = load(cls.rpc.datadir / ".bgt3-owner.json")
        if cls.state["lab_id"] != owner["lab_id"]:
            raise ValueError("Selected evidence belongs to another node")

    def block(self):
        return self.rpc.call("getblock", self.state["block_hash"], 2)

    def test_cli_and_python_same_node(self):
        command = [self.rpc.executable, "-regtest", f"-datadir={self.rpc.datadir}",
                   f"-rpcport={self.rpc.port}", "getblockchaininfo"]
        direct = json.loads(subprocess.check_output(command, text=True), parse_float=Decimal)
        python = self.rpc.require_regtest()
        self.assertEqual(direct["chain"], "regtest")
        self.assertEqual((direct["bestblockhash"], direct["blocks"], direct["headers"]),
                         (python["bestblockhash"], python["blocks"], python["headers"]))

    def test_payment_before_and_after(self):
        before = load(self.folder / "before.json")
        after = load(self.folder / "after.json")
        self.assertIn(self.state["txid"], before["mempool"])
        self.assertEqual(before["sender"]["confirmations"], 0)
        self.assertEqual(before["receiver"]["confirmations"], 0)
        self.assertNotIn(self.state["txid"], after["mempool"])
        self.assertGreaterEqual(after["receiver"]["confirmations"], 1)
        current = self.rpc.call("gettransaction", self.state["txid"], wallet="student")
        self.assertGreaterEqual(current["confirmations"], 1)
        self.assertNotIn(self.state["txid"], self.rpc.call("getrawmempool"))

    def test_live_fee_and_recipient(self):
        transaction = self.rpc.call("getrawtransaction", self.state["txid"], True, self.state["block_hash"])
        sender = self.rpc.call("gettransaction", self.state["txid"], wallet="MIF")
        result = transaction_analysis(transaction, load(self.folder / "prevouts.json"), self.state["recipient"],
                                      load(self.folder / "output-wallet-info.json"), sender["fee"],
                                      load(self.folder / "previous-transactions.json"))
        self.assertEqual(result["fee_sat"], result["sender_wallet_fee_abs_sat"])
        self.assertEqual(sum(output["value_sat"] for output in result["vout"] if output["role"] == "recipient"), 100000000)
        self.assertEqual(self.state["requested_fee_sat_vb"], 2)

    def test_live_libbitcoin_merkle(self):
        block = self.block()
        ids = [tx["txid"] for tx in block["tx"]]
        self.assertIn("coinbase", block["tx"][0]["vin"][0])
        self.assertIn(self.state["txid"], ids[1:])
        binary = os.environ.get("BGT3_MERKLE_EXE", str(ROOT / "build/assignment3/cpp/bitcoin_merkle"))
        actual = subprocess.check_output([binary, *ids], text=True).strip()
        self.assertEqual(actual, block["merkleroot"])
        self.assertEqual(bitcoin_merkle(ids), actual)

    def test_live_header_and_output_total(self):
        block = self.block()
        raw = self.rpc.call("getblockheader", self.state["block_hash"], False)
        result = verify_header(block, raw)
        self.assertEqual(result["calculated_hash"], self.state["block_hash"])
        independent = sum(output.nValue for tx in block["tx"]
                          for output in CTransaction.deserialize(bytes.fromhex(tx["hex"])).vout)
        self.assertEqual(block_output_total(block), independent)

    def test_rerun_does_not_create_payment(self):
        height = self.rpc.call("getblockcount")
        mempool = self.rpc.call("getrawmempool")
        result = subprocess.run([sys.executable, str(ROOT / "scripts/lab.py"), "run"],
                                capture_output=True, text=True, check=True)
        self.assertIn("reused", result.stdout)
        self.assertEqual(self.rpc.call("getblockcount"), height)
        self.assertEqual(self.rpc.call("getrawmempool"), mempool)
        self.assertEqual(load(self.folder / "run.json")["txid"], self.state["txid"])


if __name__ == "__main__":
    unittest.main()
