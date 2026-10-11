#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Wallet and RPC behaviour around the height-840,000 transition (840 on regtest).

The regime a signature is made for is decided when the transaction is signed, from the
height of the last block the wallet has processed plus one. These are the paths where the
step that builds a transaction and the step that signs it are separated, so that the two
can end up on different sides of the transition height:

- the funding RPCs `fundrawtransaction`, `walletcreatefundedpsbt` and `send`, and `bumpfee`,
  on both sides of the height;
- a PSBT funded below the height and signed above it, which works here because the regime
  is chosen when the transaction is signed and not when it is funded;
- the exact block at which the wallet switches regime, and that a restart with -rescan
  across the height leaves it signing for the right one;
- a transaction that is still unconfirmed when the height is reached, and what it takes to
  replace it.
"""

from decimal import Decimal

from test_framework.messages import COIN
from test_framework.s6b_util import S6B_HEIGHT, mine_to_height
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error

FEE = Decimal("0.0001")
MISSING_AMOUNT = "Missing amount"
OLD_STYLE = "old-style-sig-fork-id"


def hash_type_of(node, tx_hex):
    """The hash type byte of the first input's signature, which is 0x41 under the new regime."""
    decoded = node.decoderawtransaction(tx_hex)
    vin = decoded["vin"][0]
    if vin.get("txinwitness"):
        sig = vin["txinwitness"][0]
    else:
        sig = vin["scriptSig"]["asm"].split()[0]
    assert len(sig) >= 2, sig
    return int(sig[-2:], 16)


class WalletS6bBoundaryTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-fallbackfee=0.0002"], ["-fallbackfee=0.0002"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def mine(self, count=1):
        self.sync_mempools()
        self.nodes[1].generatetoaddress(count, self.nodes[1].getnewaddress())
        self.sync_all()

    def to(self, height):
        mine_to_height(self.nodes[1], height, self.nodes[1].getnewaddress())
        self.sync_all()

    def funded_hex(self, node, addr):
        raw = node.createrawtransaction([], [{addr: 1}])
        return node.fundrawtransaction(raw)["hex"]

    def run_test(self):
        node, miner = self.nodes
        self.log.info("Funding the wallet under test")
        miner.generatetoaddress(110, miner.getnewaddress())
        self.sync_all()
        for _ in range(8):
            miner.sendtoaddress(node.getnewaddress(), 5)
        self.mine()
        assert_greater_than(node.getbalance(), 30)

        self.to(S6B_HEIGHT - 4)
        self.check_building_rpcs("below the transition height", expected_hash_type=0x01)

        self.log.info("A PSBT funded below the transition height, signed above it")
        addr = miner.getnewaddress()
        early = node.walletcreatefundedpsbt([], [{addr: 1}])
        early_psbt = early["psbt"]
        # Keep the wallet from spending the same coins in the meantime.
        node.lockunspent(False, [{"txid": vin["txid"], "vout": vin["vout"]}
                                 for vin in node.decodepsbt(early_psbt)["tx"]["vin"]])

        self.log.info("A transaction that is still unconfirmed when the height is reached")
        stuck_txid = node.sendtoaddress(miner.getnewaddress(), 2)
        stuck = node.gettransaction(stuck_txid)
        stuck_inputs = [{"txid": vin["txid"], "vout": vin["vout"]}
                        for vin in node.decoderawtransaction(stuck["hex"])["vin"]]
        assert stuck_txid in node.getrawmempool()
        # Keep it out of the blocks that follow by fee, so that it is still unconfirmed when
        # the height is reached. The delta is taken back below, which is what makes the case
        # about the transition rule and not about the fee.
        self.sync_mempools()
        for n in self.nodes:
            n.prioritisetransaction(stuck_txid, 0, -10 * int(COIN))

        self.log.info("The block at which the wallet switches")
        self.to(S6B_HEIGHT - 2)
        assert_equal(self.probe_hash_type(), 0x01)  # the next block is still below the height
        self.to(S6B_HEIGHT - 1)
        assert_equal(self.probe_hash_type(), 0x41)  # the next block is the transition block

        self.check_stuck_transaction(stuck_txid, stuck_inputs)
        self.check_building_rpcs("from the transition height on", expected_hash_type=0x41)
        self.check_early_psbt(early_psbt)
        self.check_after_rescan()

    def check_building_rpcs(self, stage, expected_hash_type):
        """Every path that funds and later signs must use the regime of the next block."""
        node, miner = self.nodes
        self.log.info("Funding and signing RPCs %s" % stage)
        addr = miner.getnewaddress()

        self.log.info("- fundrawtransaction, then signrawtransactionwithwallet")
        funded = self.funded_hex(node, addr)
        assert_equal(node.decoderawtransaction(funded)["version"], 2)
        signed = node.signrawtransactionwithwallet(funded)
        assert_equal(signed["complete"], True)
        assert_equal(hash_type_of(node, signed["hex"]), expected_hash_type)
        txid = node.sendrawtransaction(signed["hex"])
        assert txid in node.getrawmempool()

        self.log.info("- walletcreatefundedpsbt, then walletprocesspsbt and finalizepsbt")
        psbt = node.walletcreatefundedpsbt([], [{addr: 1}])["psbt"]
        processed = node.walletprocesspsbt(psbt)
        assert_equal(processed["complete"], True)
        final = node.finalizepsbt(processed["psbt"])
        assert_equal(final["complete"], True)
        assert_equal(hash_type_of(node, final["hex"]), expected_hash_type)
        assert node.sendrawtransaction(final["hex"]) in node.getrawmempool()

        self.log.info("- send")
        sent = node.send({addr: 1}, None, "unset", None, {"add_to_wallet": True})
        assert_equal(sent["complete"], True)
        assert_equal(hash_type_of(node, node.gettransaction(sent["txid"])["hex"]), expected_hash_type)

        self.log.info("- bumpfee re-signs for the same regime")
        bumpable = node.sendtoaddress(addr, 1, "", "", False, True)
        bumped = node.bumpfee(bumpable)
        assert_equal(bumped["errors"], [])
        assert_equal(hash_type_of(node, node.gettransaction(bumped["txid"])["hex"]), expected_hash_type)
        # This build inherits Litecoin's -mempoolreplacement default of 0, so the node that
        # made the replacement does not accept it while the original is in its mempool. The
        # replacement is signed for the right regime either way, which is what is pinned here.
        assert bumped["txid"] not in node.getrawmempool()
        assert bumpable in node.getrawmempool()

        # Everything built here has to be able to confirm on this side of the height.
        before = node.getblockcount()
        self.mine()
        assert_equal(node.getblockcount(), before + 1)
        for txid in (txid, sent["txid"], bumpable):
            assert_equal(node.gettransaction(txid)["confirmations"], 1)
        assert_equal(node.getrawmempool(), [])

    def check_early_psbt(self, early_psbt):
        """The regime is decided when the transaction is signed, so a PSBT that was funded
        below the transition height is still usable above it."""
        node = self.nodes[0]
        self.log.info("- a PSBT funded below the height still signs above it")
        node.lockunspent(True)
        processed = node.walletprocesspsbt(early_psbt)
        assert_equal(processed["complete"], True)
        final = node.finalizepsbt(processed["psbt"])
        assert_equal(final["complete"], True)
        assert_equal(hash_type_of(node, final["hex"]), 0x41)
        txid = node.sendrawtransaction(final["hex"])
        assert txid in node.getrawmempool()
        self.mine()
        assert_equal(node.gettransaction(txid)["confirmations"], 1)

    def probe_hash_type(self):
        """Sign a transaction without sending it, and report the hash type the wallet chose."""
        node = self.nodes[0]
        signed = node.signrawtransactionwithwallet(self.funded_hex(node, self.nodes[1].getnewaddress()))
        assert_equal(signed["complete"], True)
        return hash_type_of(node, signed["hex"])

    def check_after_rescan(self):
        """A wallet reads the regime from the last block it has processed itself. After a
        restart with -rescan it has processed the whole chain again, so it must still sign
        for the block that comes next, not for the height it was last started at."""
        node = self.nodes[0]
        self.log.info("- after a restart with -rescan across the height")
        self.restart_node(0, extra_args=["-fallbackfee=0.0002", "-rescan"])
        self.connect_nodes(0, 1)
        self.sync_all()
        assert_greater_than(node.getblockcount(), S6B_HEIGHT)
        signed = node.signrawtransactionwithwallet(self.funded_hex(node, self.nodes[1].getnewaddress()))
        assert_equal(hash_type_of(node, signed["hex"]), 0x41)
        assert node.sendrawtransaction(signed["hex"]) in node.getrawmempool()
        self.mine()

    def check_stuck_transaction(self, stuck_txid, stuck_inputs):
        """A transaction signed below the height that did not confirm in time is gone, and
        its coins can be spent again straight away."""
        node, miner = self.nodes
        self.log.info("- the unconfirmed transaction is out of the mempool")
        for n in self.nodes:
            assert_equal(n.getrawmempool(), [])
        # Give it back the fee it was denied, so that what keeps it out from here on is the
        # transition rule and nothing else.
        for n in self.nodes:
            n.prioritisetransaction(stuck_txid, 0, 10 * int(COIN))
        assert_equal(node.gettransaction(stuck_txid)["confirmations"], 0)
        assert_raises_rpc_error(-26, OLD_STYLE, node.sendrawtransaction, node.gettransaction(stuck_txid)["hex"])

        self.log.info("- its inputs are free, so the same coins can be spent again")
        node.abandontransaction(stuck_txid)
        raw = node.createrawtransaction(stuck_inputs, [{miner.getnewaddress(): 1}])
        funded = node.fundrawtransaction(raw)["hex"]
        signed = node.signrawtransactionwithwallet(funded)
        assert_equal(signed["complete"], True)
        assert_equal(hash_type_of(node, signed["hex"]), 0x41)
        again = node.sendrawtransaction(signed["hex"])
        assert again in node.getrawmempool()
        self.mine()
        assert_equal(node.gettransaction(again)["confirmations"], 1)


if __name__ == "__main__":
    WalletS6bBoundaryTest().main()
