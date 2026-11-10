from copy import deepcopy
from decimal import Decimal
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from bitcoin_data import (bitcoin_merkle, block_output_total, hash_bytes, header_bytes,
                          sha256d, transaction_analysis, transaction_metrics, verify_header)
from common import ROOT, RPC, RPCError, btc, load, satoshi


class OfflineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = ROOT / "evidence/lab"
        cls.transaction = load(cls.folder / "transaction.json")
        cls.block = load(cls.folder / "block.json")
        cls.state = load(cls.folder / "run.json")
        cls.prevouts = load(cls.folder / "prevouts.json")
        cls.header = (cls.folder / "header.hex").read_text().strip()

    def analysis(self, transaction=None, prevouts=None, **options):
        return transaction_analysis(self.transaction if transaction is None else transaction,
                                    self.prevouts if prevouts is None else prevouts, **options)

    def test_exact_satoshi(self):
        self.assertEqual(satoshi(Decimal("0.00000001")), 1)
        self.assertEqual(satoshi("1.23456789"), 123456789)
        self.assertEqual(btc(282), "0.00000282")

    def test_invalid_money(self):
        for value in [0.1, True, "-1", "0.000000001", "NaN", "Infinity", "not-money"]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                satoshi(value)

    def test_real_inputs(self):
        result = self.analysis()
        self.assertEqual(result["input_total_sat"], 5000000000)
        self.assertEqual(result["vin"][0]["vout"], 0)
        self.assertEqual(len(result["vin"][0]["txinwitness"]), 2)

    def test_real_output_roles(self):
        result = self.analysis(recipient=self.state["recipient"], ownership=load(self.folder / "output-wallet-info.json"))
        self.assertEqual(result["vout"][1]["role"], "recipient")
        self.assertEqual(result["vout"][1]["value_sat"], 100000000)
        self.assertEqual(result["vout"][0]["role"], "change (sender wallet evidence)")
        self.assertEqual(result["vout"][0]["script_type"], "witness_v0_keyhash")

    def test_multiple_inputs_by_exact_index(self):
        tx = {"vin": [{"txid": "same", "vout": 0}, {"txid": "same", "vout": 1}], "vsize": 200,
              "vout": [{"n": 0, "value": "3.999995", "scriptPubKey": {"address": "change"}},
                       {"n": 1, "value": "0", "scriptPubKey": {"type": "nulldata"}},
                       {"n": 2, "value": "1", "scriptPubKey": {"address": "recipient"}}]}
        previous = [{"txid": "same", "vout": 0, "amount": "2"}, {"txid": "same", "vout": 1, "amount": "3"}]
        result = self.analysis(tx, previous, recipient="recipient", wallet_fee="-0.000005")
        self.assertEqual(result["input_total_sat"], 500000000)
        self.assertEqual(result["fee_sat"], 500)
        self.assertEqual(result["fee_sat_vb"], "2.5")
        self.assertIsNone(result["vout"][1]["address"])
        self.assertEqual(result["vout"][2]["role"], "recipient")

    def test_coinbase_is_not_normal_fee(self):
        result = self.analysis(self.block["tx"][0], [])
        self.assertTrue(result["coinbase"])
        self.assertIsNone(result["fee_sat"])
        self.assertEqual(result["output_total_sat"], 5000000282)

    def test_missing_prevout(self):
        with self.assertRaisesRegex(ValueError, "Missing prevout"):
            self.analysis(prevouts=[])

    def test_wrong_prevout_index(self):
        outputs = deepcopy(self.prevouts)
        outputs[0]["vout"] += 1
        with self.assertRaisesRegex(ValueError, "Missing prevout"):
            self.analysis(prevouts=outputs)

    def test_previous_transaction_fallback(self):
        result = self.analysis(prevouts=[], previous=load(self.folder / "previous-transactions.json"))
        self.assertEqual(result["fee_sat"], 282)

    def test_prevout_evidence_conflict(self):
        outputs = deepcopy(self.prevouts)
        outputs[0]["amount"] = "49"
        with self.assertRaisesRegex(ValueError, "disagrees"):
            self.analysis(prevouts=outputs, previous=load(self.folder / "previous-transactions.json"))

    def test_wallet_fee_mismatch(self):
        with self.assertRaisesRegex(ValueError, "differs"):
            self.analysis(wallet_fee="-0.00000001")

    def test_fee_and_vsize(self):
        result = self.analysis(wallet_fee=load(self.folder / "after.json")["sender"]["fee"])
        self.assertEqual(result["fee_sat"], 282)
        self.assertEqual(result["fee_sat_vb"], "2")
        self.assertNotEqual(Decimal(result["fee_sat"]) / self.transaction["size"], Decimal("2"))

    def test_negative_fee(self):
        tx = deepcopy(self.transaction)
        tx["vout"][0]["value"] = "100"
        with self.assertRaisesRegex(ValueError, "Negative"):
            self.analysis(tx)

    def test_missing_transaction(self):
        for data in [None, {}, {"vin": [], "vout": []}]:
            with self.assertRaises(ValueError):
                self.analysis(transaction=data if data is not None else {})

    def test_legacy_weight(self):
        fixture = load(ROOT / "fixtures/legacy.json")
        result = transaction_metrics(fixture["raw"])
        self.assertEqual(result["txid"], fixture["txid"])
        self.assertEqual(result["txid"], result["wtxid"])
        self.assertEqual(result["size"], result["base_size"])
        self.assertEqual(result["weight"], 4 * result["size"])
        self.assertFalse(result["has_witness"])
        self.assertTrue(result["vin_scripts"][0]["scriptSig"]["hex"])

    def test_segwit_weight_and_rounding(self):
        result = transaction_metrics(self.transaction["hex"], self.transaction)
        self.assertEqual((result["size"], result["base_size"], result["weight"], result["vsize"]), (222, 113, 561, 141))
        self.assertNotEqual(result["txid"], result["wtxid"])
        self.assertTrue(result["has_witness"])
        self.assertEqual(result["vin_scripts"][0]["scriptSig"]["hex"], "")

    def test_segwit_output_does_not_imply_witness(self):
        from bitcoin.core import CMutableTransaction, CMutableTxIn, CMutableTxOut, COutPoint
        from bitcoin.core.script import CScript
        tx = CMutableTransaction([CMutableTxIn(COutPoint(bytes.fromhex("12" * 32), 0))],
                                 [CMutableTxOut(1000, CScript(bytes.fromhex("0014" + "11" * 20)))])
        result = transaction_metrics(tx.serialize().hex())
        self.assertFalse(result["has_witness"])
        self.assertEqual(result["size"], result["vsize"])

    def test_raw_transaction_rejection(self):
        for raw in ["zz", "", "00"]:
            with self.assertRaises(ValueError):
                transaction_metrics(raw)

    def test_rpc_metrics_mismatch(self):
        changed = deepcopy(self.transaction)
        changed["weight"] += 1
        with self.assertRaisesRegex(ValueError, "disagrees"):
            transaction_metrics(changed["hex"], changed)

    def test_header_exact_bytes(self):
        header = header_bytes(self.block)
        self.assertEqual(len(header), 80)
        self.assertEqual(header.hex(), self.header)
        self.assertEqual(header[4:36], bytes.fromhex(self.block["previousblockhash"])[::-1])
        self.assertEqual(header[36:68], bytes.fromhex(self.block["merkleroot"])[::-1])
        self.assertEqual(header[72:76], int(self.block["bits"], 16).to_bytes(4, "little"))

    def test_header_hash_and_nonce(self):
        result = verify_header(self.block, self.header)
        self.assertEqual(result["calculated_hash"], self.block["hash"])
        self.assertNotEqual(result["modified_nonce_hash"], result["node_hash"])
        self.assertFalse(result["full_consensus_validation"])

    def test_signed_version_field(self):
        block = dict(self.block, version=-1)
        self.assertEqual(header_bytes(block)[:4], b"\xff" * 4)

    def test_invalid_header_fields(self):
        for changes in [{"nonce": -1}, {"time": 1 << 32}, {"version": 1 << 32}, {"bits": "xyz"}, {"merkleroot": "00"}]:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                header_bytes(dict(self.block, **changes))

    def test_missing_header_data(self):
        with self.assertRaises(ValueError):
            header_bytes({})
        with self.assertRaises(ValueError):
            verify_header(self.block, "00" * 80)

    def test_genesis_previous_hash(self):
        genesis = dict(self.block, height=0)
        del genesis["previousblockhash"]
        self.assertEqual(header_bytes(genesis)[4:36], b"\x00" * 32)

    def test_all_block_outputs(self):
        self.assertEqual(block_output_total(self.block), 10000000000)
        self.assertEqual(len(self.block["tx"]), 2)
        self.assertGreater(block_output_total(self.block), 100000000)

    def test_missing_block(self):
        for data in [None, {}, {"tx": ["only-a-txid"]}]:
            with self.assertRaises(ValueError):
                block_output_total(data)

    def test_transaction_merkle_uses_txid(self):
        ids = [tx["txid"] for tx in self.block["tx"]]
        witness_ids = [tx["hash"] for tx in self.block["tx"]]
        self.assertEqual(bitcoin_merkle(ids), self.block["merkleroot"])
        self.assertNotEqual(bitcoin_merkle(witness_ids), self.block["merkleroot"])

    def test_hash_encoding(self):
        self.assertEqual(hash_bytes("01" + "00" * 31), b"\x00" * 31 + b"\x01")
        for bad in ["00", "g" * 64, " " * 64]:
            with self.assertRaises(ValueError):
                hash_bytes(bad)

    def test_rpc_errors(self):
        with self.assertRaisesRegex(RPCError, "unavailable"):
            RPC(executable="/nonexistent/bitcoin-cli").call("getblockchaininfo")
        with patch("common.subprocess.run", side_effect=subprocess.TimeoutExpired("cli", 1)):
            with self.assertRaisesRegex(RPCError, "timed out"):
                RPC().call("getblockchaininfo")
        with patch("common.subprocess.run", return_value=subprocess.CompletedProcess([], 1, "", "No authentication cookie")):
            with self.assertRaisesRegex(RPCError, "authentication"):
                RPC().call("getblockchaininfo")

    def test_rpc_decimal_parsing(self):
        result = subprocess.CompletedProcess([], 0, '{"amount":0.00000001}', "")
        with patch("common.subprocess.run", return_value=result):
            self.assertEqual(RPC().call("getbalances")["amount"], Decimal("0.00000001"))

    def test_rpc_raw_hex_stays_text(self):
        result = subprocess.CompletedProcess([], 0, "1" * 160, "")
        with patch("common.subprocess.run", return_value=result):
            self.assertEqual(RPC().call("getblockheader", "11" * 32, False), "1" * 160)

    def test_missing_json(self):
        with self.assertRaisesRegex(ValueError, "Cannot read"):
            load(self.folder / "does-not-exist.json")

    def test_unowned_node_is_not_stopped(self):
        with tempfile.TemporaryDirectory() as directory:
            env = dict(os.environ, BGT3_DATADIR=directory)
            result = subprocess.run([sys.executable, str(ROOT / "scripts/node.py"), "stop"], env=env,
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not a project-owned", result.stderr)


if __name__ == "__main__":
    unittest.main()
