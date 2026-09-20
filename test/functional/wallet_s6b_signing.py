#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test wallet, raw-transaction and PSBT signing across the height-840,000 transition.

Every signing path signs for the block that is expected to confirm the transaction, which
is the block after the current tip: historical signatures below the transition height
(840 on regtest), replay-protected ones (SIGHASH_FORKID) from it on. The same flows run
on both sides of the transition:
- wallet sends from legacy, P2SH-SegWit and bech32 addresses;
- createrawtransaction + signrawtransactionwithwallet;
- signrawtransactionwithkey, the hash type it produces and accepts, and the amount it needs
  for a pre-SegWit input from the transition height on;
- a single-signer PSBT (walletcreatefundedpsbt, walletprocesspsbt, finalizepsbt);
- two parties signing a 2-of-2 multisig one after the other with signrawtransactionwithkey,
  which has to keep the first party's signature;
- two parties signing the same unsigned transaction independently, merged with
  combinerawtransaction;
- two wallets signing a multisig PSBT independently, merged with combinepsbt, analyzed
  with analyzepsbt and finalized with finalizepsbt.

A transaction that was signed below the transition height and not confirmed in time is
refused afterwards with a reason that names the cause; signing it again makes it valid.
"""

from decimal import Decimal

from test_framework.address import key_to_p2pkh, key_to_p2wpkh
from test_framework.key import ECKey
from test_framework.messages import COIN, CTransaction, CTxIn, CTxOut
from test_framework.s6b_util import (
    S6B_HEIGHT,
    STANDARD_OUTPUT_SCRIPT,
    fund_outputs,
    mine_to_height,
)
from test_framework.script import OP_2, OP_CHECKMULTISIG, CScript
from test_framework.script_util import key_to_p2pkh_script, script_to_p2sh_script, script_to_p2wsh_script
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet_util import bytes_to_wif

OLD_STYLE = "old-style-sig-fork-id"
FEE = 10000


class TwoOfTwo:
    """A 2-of-2 multisig output with keys held by two parties, as P2SH or P2WSH."""

    def __init__(self, wrap, amount=COIN):
        self.wrap = wrap
        self.amount = amount
        self.outpoint = None
        self.keys = [ECKey(), ECKey()]
        for key in self.keys:
            key.generate()
        self.script = CScript([OP_2] + [k.get_pubkey().get_bytes() for k in self.keys] + [OP_2, OP_CHECKMULTISIG])
        self.script_pubkey = script_to_p2sh_script(self.script) if wrap == "p2sh" else script_to_p2wsh_script(self.script)

    def wif(self, i):
        return bytes_to_wif(self.keys[i].get_bytes())

    def prevtx(self):
        entry = {"txid": "%064x" % self.outpoint.hash, "vout": self.outpoint.n, "scriptPubKey": self.script_pubkey.hex(),
                 "amount": Decimal(self.amount) / COIN}
        entry["redeemScript" if self.wrap == "p2sh" else "witnessScript"] = self.script.hex()
        return [entry]

    def unsigned_spend(self):
        tx = CTransaction()
        tx.vin = [CTxIn(self.outpoint, nSequence=0xfffffffe)]
        tx.vout = [CTxOut(self.amount - FEE, STANDARD_OUTPUT_SCRIPT)]
        return tx.serialize().hex()


class S6bSigningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-acceptnonstdtxn=0"]] * 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def sent(self, txid):
        """Remember a transaction that was accepted to a mempool; confirm() expects to mine it."""
        self.pending.add(txid)
        return txid

    def confirm(self):
        """Relay the mempool to node1, which mines it; everything sent so far has to be in a block by now."""
        self.sync_mempools()
        block_hash = self.nodes[1].generatetoaddress(1, self.nodes[1].getnewaddress())[0]
        self.sync_blocks()
        self.pending -= set(self.nodes[0].getblock(block_hash)["tx"])
        assert_equal(self.pending, set())
        assert_equal(self.nodes[0].getrawmempool(), [])

    def signing_flows(self):
        node0, node1 = self.nodes

        self.log.info("- wallet sends from every address type")
        received = {}
        for address_type in ("legacy", "p2sh-segwit", "bech32"):
            addr = node1.getnewaddress(address_type=address_type)
            received[address_type] = self.sent(node0.sendtoaddress(addr, 2))
        self.confirm()
        for address_type, funding_txid in received.items():
            utxo = [u for u in node1.listunspent() if u["txid"] == funding_txid][0]
            # spend exactly this coin, so that the signature really is of the address type in question
            raw = node1.createrawtransaction([{"txid": utxo["txid"], "vout": utxo["vout"]}],
                                             [{node0.getnewaddress(): utxo["amount"] - Decimal("0.0001")}])
            signed = node1.signrawtransactionwithwallet(raw)
            assert_equal(signed["complete"], True)
            self.sent(node1.sendrawtransaction(signed["hex"]))
        self.sent(node0.sendtoaddress(node1.getnewaddress(), 1))
        self.sent(node0.sendmany("", {node1.getnewaddress(): 1, node1.getnewaddress(address_type="legacy"): 1}))

        self.log.info("- signrawtransactionwithkey")
        key = ECKey()
        key.generate()
        pubkey = key.get_pubkey().get_bytes()
        for addr in (key_to_p2pkh(pubkey), key_to_p2wpkh(pubkey)):
            funding_txid = self.sent(node0.sendtoaddress(addr, 1))
            vout = [o["n"] for o in node0.getrawtransaction(funding_txid, True)["vout"] if o["value"] == 1][0]
            raw = node0.createrawtransaction([{"txid": funding_txid, "vout": vout}], [{node0.getnewaddress(): Decimal("0.9999")}])
            signed = node0.signrawtransactionwithkey(raw, [bytes_to_wif(key.get_bytes())])
            assert_equal(signed["complete"], True)
            self.sent(node0.sendrawtransaction(signed["hex"]))

        self.log.info("- hash types and amounts")
        active = node0.getblockcount() + 1 >= S6B_HEIGHT
        wif = bytes_to_wif(key.get_bytes())
        funding_txid = self.sent(node0.sendtoaddress(key_to_p2pkh(pubkey), 1))
        vout = [o["n"] for o in node0.getrawtransaction(funding_txid, True)["vout"] if o["value"] == 1][0]
        raw = node0.createrawtransaction([{"txid": funding_txid, "vout": vout}], [{node0.getnewaddress(): Decimal("0.9999")}])

        def hash_type_of(signed_hex):
            asm = node0.decoderawtransaction(signed_hex)["vin"][0]["scriptSig"]["asm"]
            return asm[asm.index("[") + 1:asm.index("]")]
        signed = node0.signrawtransactionwithkey(raw, [wif])
        assert_equal(signed["complete"], True)
        assert_equal(hash_type_of(signed["hex"]), "ALL|FORKID" if active else "ALL")
        # A pre-SegWit output described by the caller without its amount: enough below the transition
        # height; from it the signature hash commits to the amount of every input.
        prevtx = {"txid": funding_txid, "vout": vout, "scriptPubKey": key_to_p2pkh_script(pubkey).hex()}
        if active:
            assert_raises_rpc_error(-3, "Missing amount", node0.signrawtransactionwithkey, raw, [wif], [prevtx])
        else:
            assert_equal(node0.signrawtransactionwithkey(raw, [wif], [prevtx])["complete"], True)
        prevtx["amount"] = 1
        assert_equal(node0.signrawtransactionwithkey(raw, [wif], [prevtx])["complete"], True)
        # Naming SIGHASH_FORKID: possible where it is in force (and added there in any case), an error elsewhere
        for name in ("ALL|FORKID", "SINGLE|FORKID|ANYONECANPAY"):
            if active:
                named = node0.signrawtransactionwithkey(raw, [wif], None, name)
                assert_equal(named["complete"], True)
                assert_equal(hash_type_of(named["hex"]), name)
            else:
                assert_raises_rpc_error(-8, "FORKID", node0.signrawtransactionwithkey, raw, [wif], None, name)
                assert_raises_rpc_error(-8, "FORKID", node0.signrawtransactionwithwallet, raw, None, name)
        if active:
            assert_equal(hash_type_of(node0.signrawtransactionwithkey(raw, [wif], None, "NONE|ANYONECANPAY")["hex"]), "NONE|FORKID|ANYONECANPAY")
        self.sent(node0.sendrawtransaction(signed["hex"]))

        self.log.info("- single-signer PSBT")
        psbt = node0.walletcreatefundedpsbt([], [{node1.getnewaddress(): 1}])["psbt"]
        processed = node0.walletprocesspsbt(psbt)
        assert_equal(processed["complete"], True)
        assert_equal(node0.analyzepsbt(processed["psbt"])["next"], "extractor")
        final = node0.finalizepsbt(processed["psbt"])
        assert_equal(final["complete"], True)
        self.sent(node0.sendrawtransaction(final["hex"]))
        self.confirm()

        self.log.info("- two parties, signrawtransactionwithkey one after the other")
        outputs = [TwoOfTwo("p2sh"), TwoOfTwo("p2wsh"), TwoOfTwo("p2sh"), TwoOfTwo("p2wsh")]
        fund_outputs(node0, outputs)
        self.sync_blocks()
        for out in outputs[:2]:
            first = node0.signrawtransactionwithkey(out.unsigned_spend(), [out.wif(0)], out.prevtx())
            assert_equal(first["complete"], False)
            # the second party has to find the first party's signature in the transaction and keep it
            second = node1.signrawtransactionwithkey(first["hex"], [out.wif(1)], out.prevtx())
            assert_equal(second["complete"], True)
            self.sent(node1.sendrawtransaction(second["hex"]))

        self.log.info("- two parties signing independently, combinerawtransaction")
        for out in outputs[2:]:
            halves = [n.signrawtransactionwithkey(out.unsigned_spend(), [out.wif(i)], out.prevtx())
                      for i, n in enumerate(self.nodes)]
            assert_equal([h["complete"] for h in halves], [False, False])
            combined = node0.combinerawtransaction([h["hex"] for h in halves])
            assert_equal(node0.testmempoolaccept([combined])[0]["allowed"], True)
            self.sent(node0.sendrawtransaction(combined))

        self.log.info("- two wallets signing a multisig PSBT independently, combinepsbt and finalizepsbt")
        for address_type in ("legacy", "p2sh-segwit", "bech32"):
            pubkeys = [n.getaddressinfo(n.getnewaddress())["pubkey"] for n in self.nodes]
            multisig = [n.addmultisigaddress(2, pubkeys, "", address_type)["address"] for n in self.nodes]
            assert_equal(multisig[0], multisig[1])
            funding_txid = self.sent(node0.sendtoaddress(multisig[0], 1))
            vout = [o["n"] for o in node0.getrawtransaction(funding_txid, True)["vout"] if o["value"] == 1][0]
            self.confirm()
            psbt = node0.createpsbt([{"txid": funding_txid, "vout": vout}], [{node1.getnewaddress(): Decimal("0.9999")}])
            # node0 paid the output, so its wallet can add the information about it that both signers need
            psbt = node0.walletprocesspsbt(psbt, False)["psbt"]
            parts = [n.walletprocesspsbt(psbt) for n in self.nodes]
            assert_equal([p["complete"] for p in parts], [False, False])
            combined = node0.combinepsbt([p["psbt"] for p in parts])
            analysis = node0.analyzepsbt(combined)
            assert_equal(analysis["next"], "finalizer")
            final = node1.finalizepsbt(combined)
            assert_equal(final["complete"], True)
            self.sent(node1.sendrawtransaction(final["hex"]))
        self.confirm()

    def run_test(self):
        node0, node1 = self.nodes
        self.pending = set()
        mine_to_height(node0, 300, node0.getnewaddress())
        mine_to_height(node0, S6B_HEIGHT - 40)
        self.sync_blocks()

        self.log.info("Below the transition height")
        self.signing_flows()
        assert node0.getblockcount() < S6B_HEIGHT - 5

        self.log.info("A transaction signed below the transition height that is not confirmed in time")
        utxo = node0.listunspent()[0]
        node0.lockunspent(False, [{"txid": utxo["txid"], "vout": utxo["vout"]}])
        raw = node0.createrawtransaction([{"txid": utxo["txid"], "vout": utxo["vout"]}],
                                         [{node1.getnewaddress(): utxo["amount"] - Decimal("0.0001")}])
        presigned = node0.signrawtransactionwithwallet(raw)
        assert_equal(presigned["complete"], True)
        assert_equal(node0.testmempoolaccept([presigned["hex"]])[0]["allowed"], True)
        psbt = node0.walletcreatefundedpsbt([], [{node1.getnewaddress(): 1}], 0, {"lockUnspents": True})["psbt"]
        presigned_psbt = node0.finalizepsbt(node0.walletprocesspsbt(psbt)["psbt"])
        assert_equal(presigned_psbt["complete"], True)

        mine_to_height(node0, S6B_HEIGHT - 1)
        self.sync_blocks()
        for stale in (presigned["hex"], presigned_psbt["hex"]):
            assert_equal(node0.testmempoolaccept([stale])[0]["reject-reason"], OLD_STYLE)
            assert_raises_rpc_error(-26, OLD_STYLE, node0.sendrawtransaction, stale)
        # Signing the same transaction again produces a valid one. signrawtransactionwithwallet keeps a
        # signature that is already there, so start from the unsigned transaction.
        resigned = node0.signrawtransactionwithwallet(raw)
        assert_equal(resigned["complete"], True)
        assert resigned["hex"] != presigned["hex"]
        self.sent(node0.sendrawtransaction(resigned["hex"]))
        node0.lockunspent(True)
        self.confirm()
        assert_equal(node0.getblockcount(), S6B_HEIGHT)

        self.log.info("From the transition height on")
        self.signing_flows()


if __name__ == '__main__':
    S6bSigningTest().main()
