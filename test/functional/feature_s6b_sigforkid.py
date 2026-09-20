#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the replay-protected signature hash across the height-840,000 transition.

From the transition height on (840 on regtest), every ECDSA signature (pre-SegWit and
SegWit v0) has to set SIGHASH_FORKID, and is hashed with the BIP143 algorithm with the
fork ID 840 in the hash type, as on Bitcoin Gold. The rule is keyed to the height of the
confirming block; the mempool applies the rule of the next block. Taproot signatures do
not change.

Covered here:
- every common script type (P2PK, P2PKH, bare and P2SH multisig, P2WPKH, P2WSH,
  P2SH-P2WPKH, P2SH-P2WSH) with every hash type, on both sides of the transition and
  in both directions: the signature made for the other side is refused by the mempool
  with a reason that names the cause and makes a block invalid; the right one is
  relayed between nodes and mined;
- Taproot key-path spends with every hash type, unchanged on both sides;
- the mempool at the boundary: transactions signed for the historical side are removed,
  with their descendants, when the last block below the transition height connects;
  transactions without ECDSA signatures stay; the node keeps producing valid blocks;
- a wallet transaction that is still unconfirmed at the boundary;
- a reorganization that moves the tip across the boundary in both directions.
"""

from test_framework.messages import COutPoint
from test_framework.s6b_util import (
    ALL_HASH_TYPES,
    ECDSA_KINDS,
    NoSigOutput,
    S6B_HEIGHT,
    STANDARD_OUTPUT_SCRIPT,
    TAPROOT_HASH_TYPES,
    TAPROOT_KIND,
    TestOutput,
    assert_block_accepted,
    assert_block_rejected,
    build_block,
    fund_outputs,
    mempool_accepts,
    mine_to_height,
)
from test_framework.script import SIGHASH_ALL, SIGHASH_DEFAULT
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

OLD_STYLE = "old-style-sig-fork-id"  # historical signatures, but the next block needs replay-protected ones
NEW_STYLE = "new-style-sig-fork-id"  # replay-protected signatures, but the next block is below the transition
# Reject reason of a block with a signature made for the other side. A replay-protected signature below
# the transition height is simply a signature that does not verify; a historical one from it on is a
# script error of its own. For a witness program the node words either as "non-mandatory" because the
# same spend would pass with the witness rules switched off; it is a consensus failure of the block
# all the same.
EVAL_FALSE = "(Script evaluated without error but finished with a false/empty top stack element)"
MUST_USE_FORKID = "(Signature must use SIGHASH_FORKID)"


def block_script_failure(fork_active, is_witness):
    prefix = "non-mandatory-script-verify-flag " if is_witness else "mandatory-script-verify-flag-failed "
    return prefix + (MUST_USE_FORKID if fork_active else EVAL_FALSE)


EVICTION_LOG = "signed for the other side of the height-%d transition" % S6B_HEIGHT

FEE = 10000


class S6bSigForkIdTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # -par=1: a failing script in a block is reported with its script error instead of as a failed
        # parallel check. Standardness is enforced as on mainnet, so everything accepted here also relays.
        self.extra_args = [["-par=1", "-acceptnonstdtxn=0"]] * 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def fund(self, outputs):
        return fund_outputs(self.nodes[0], outputs)

    def spend(self, out, hashtype, fork_active):
        return out.spend(hashtype, fork_active, fee=FEE, script_pubkey=STANDARD_OUTPUT_SCRIPT)

    def check_matrix(self, fork_active):
        node = self.nodes[0]
        assert_equal(node.getblockcount() + 2 >= S6B_HEIGHT, fork_active)  # funding adds one block
        ecdsa = [(TestOutput(kind), hashtype) for kind in ECDSA_KINDS for hashtype in ALL_HASH_TYPES]
        taproot = [(TestOutput(TAPROOT_KIND), hashtype) for hashtype in TAPROOT_HASH_TYPES]
        self.fund([out for out, _ in ecdsa + taproot])
        assert_equal(node.getblockcount() + 1 >= S6B_HEIGHT, fork_active)

        wrong_reason = OLD_STYLE if fork_active else NEW_STYLE
        sent = []
        for out, hashtype in ecdsa:
            self.log.debug("%s, hash type 0x%02x", out.kind, hashtype)
            wrong = self.spend(out, hashtype, not fork_active)
            assert_equal(mempool_accepts(node, wrong), (False, wrong_reason))
            assert_raises_rpc_error(-26, wrong_reason, node.sendrawtransaction, wrong.serialize().hex())
            assert_block_rejected(node, build_block(node, txs=[wrong], fees=FEE), block_script_failure(fork_active, out.is_witness))

            right = self.spend(out, hashtype, fork_active)
            assert_equal(mempool_accepts(node, right), (True, None))
            node.sendrawtransaction(right.serialize().hex())
            sent.append(right.hash)
        for out, hashtype in taproot:
            self.log.debug("%s, hash type 0x%02x", out.kind, hashtype)
            tx = self.spend(out, hashtype, fork_active)
            # the Taproot signature hash does not change: the same transaction whatever the regime
            assert_equal(tx.serialize(), self.spend(out, hashtype, not fork_active).serialize())
            assert_equal(mempool_accepts(node, tx), (True, None))
            node.sendrawtransaction(tx.serialize().hex())
            sent.append(tx.hash)

        # Relayed to the other node, which mines them
        self.sync_mempools()
        block_hash = self.nodes[1].generatetoaddress(1, self.nodes[1].getnewaddress())[0]
        self.sync_blocks()
        assert_equal(node.getrawmempool(), [])
        confirmed = set(node.getblock(block_hash)["tx"])
        assert all(txid in confirmed for txid in sent)
        assert_equal(node.getblockcount() >= S6B_HEIGHT, fork_active)

    def run_test(self):
        node, node1 = self.nodes

        self.log.info("Below the transition height: every script and hash type, both regimes")
        mine_to_height(node, 200, node.getnewaddress())
        mine_to_height(node, S6B_HEIGHT - 20)
        self.sync_blocks()
        self.check_matrix(fork_active=False)

        self.log.info("The mempool at the boundary")
        mine_to_height(node, S6B_HEIGHT - 3)
        hist = [TestOutput("p2pkh"), TestOutput("p2wpkh"), TestOutput("p2sh_multisig")]
        parent_out = TestOutput("p2pkh")
        child_out = TestOutput(TAPROOT_KIND)
        taproot_out = TestOutput(TAPROOT_KIND)
        nosig_out = NoSigOutput()
        # for the reorganization further down: confirmed below the boundary, spent above it
        reorg_out = TestOutput("p2wsh_multisig")
        reorg_taproot_out = TestOutput(TAPROOT_KIND)
        self.fund(hist + [parent_out, taproot_out, nosig_out, reorg_out, reorg_taproot_out])
        assert_equal(node.getblockcount(), S6B_HEIGHT - 2)

        # The next block is the last one under the historical rule
        historical = [self.spend(out, SIGHASH_ALL, False) for out in hist]
        parent = parent_out.spend(SIGHASH_ALL, False, fee=FEE, script_pubkey=child_out.script_pubkey)
        child_out.outpoint = COutPoint(parent.sha256, 0)
        child_out.amount = parent.vout[0].nValue
        child = self.spend(child_out, SIGHASH_DEFAULT, False)
        taproot_tx = self.spend(taproot_out, SIGHASH_DEFAULT, False)
        nosig_tx = nosig_out.spend(fee=FEE)
        early = self.spend(hist[0], SIGHASH_ALL, True)
        assert_equal(mempool_accepts(node, early), (False, NEW_STYLE))
        for tx in historical + [parent, child, taproot_tx, nosig_tx]:
            node.sendrawtransaction(tx.serialize().hex())
        wallet_txid = node.sendtoaddress(node1.getnewaddress(), 1)
        self.sync_mempools()
        doomed = {tx.hash for tx in historical + [parent, child]} | {wallet_txid}
        survivors = {taproot_tx.hash, nosig_tx.hash}
        assert_equal(set(node1.getrawmempool()), doomed | survivors)

        # Connect the last block below the transition height without confirming anything
        with node.assert_debug_log([EVICTION_LOG]), node1.assert_debug_log([EVICTION_LOG]):
            boundary_block = build_block(node)
            assert_block_accepted(node, boundary_block)
            self.sync_blocks()
        assert_equal(node.getblockcount(), S6B_HEIGHT - 1)
        for n in self.nodes:
            assert_equal(set(n.getrawmempool()), survivors)
        # The node can still assemble a valid block
        assert_equal(node.getblocktemplate({"rules": ["mweb", "segwit"]})["height"], S6B_HEIGHT)

        for tx in historical:
            assert_raises_rpc_error(-26, OLD_STYLE, node.sendrawtransaction, tx.serialize().hex())
        replacements = [self.spend(out, SIGHASH_ALL, True) for out in hist]
        for tx in replacements:
            node.sendrawtransaction(tx.serialize().hex())

        self.log.info("A wallet transaction that was still unconfirmed at the boundary")
        wtx = node.gettransaction(wallet_txid)
        assert_equal(wtx["confirmations"], 0)
        assert wallet_txid not in node.getrawmempool()
        assert_raises_rpc_error(-26, OLD_STYLE, node.sendrawtransaction, wtx["hex"])
        # Its inputs stay reserved until it is abandoned; then the wallet can pay again, signing for the new side
        node.abandontransaction(wallet_txid)
        assert_equal(node.gettransaction(wallet_txid)["details"][0]["abandoned"], True)
        new_wallet_txid = node.sendtoaddress(node1.getnewaddress(), 1)
        assert new_wallet_txid in node.getrawmempool()

        self.sync_mempools()
        transition_hash = node1.generatetoaddress(1, node1.getnewaddress())[0]
        self.sync_blocks()
        assert_equal(node.getblockcount(), S6B_HEIGHT)
        confirmed = set(node.getblock(transition_hash)["tx"])
        assert survivors | {tx.hash for tx in replacements} | {new_wallet_txid} <= confirmed
        assert_equal(node.getrawmempool(), [])

        self.log.info("Above the transition height: every script and hash type, both regimes")
        self.check_matrix(fork_active=True)

        self.log.info("A reorganization across the boundary, in both directions")
        self.disconnect_nodes(0, 1)
        tip_height = node.getblockcount()
        tip_hash = node.getbestblockhash()
        new_style = self.spend(reorg_out, SIGHASH_ALL, True)
        old_style = self.spend(reorg_out, SIGHASH_ALL, False)
        reorg_taproot_tx = self.spend(reorg_taproot_out, SIGHASH_DEFAULT, True)
        node.sendrawtransaction(new_style.serialize().hex())
        node.sendrawtransaction(reorg_taproot_tx.serialize().hex())

        # Back below the boundary: what was signed for the new side leaves the mempool, including the
        # transactions of the disconnected blocks; transactions without ECDSA signatures return to it.
        with node.assert_debug_log([EVICTION_LOG]):
            node.invalidateblock(boundary_block.hash)
        assert_equal(node.getblockcount(), S6B_HEIGHT - 2)
        mempool = set(node.getrawmempool())
        assert new_style.hash not in mempool
        assert all(tx.hash not in mempool for tx in replacements)
        assert new_wallet_txid not in mempool
        assert survivors | {reorg_taproot_tx.hash} <= mempool
        assert_raises_rpc_error(-26, NEW_STYLE, node.sendrawtransaction, new_style.serialize().hex())
        node.sendrawtransaction(old_style.serialize().hex())
        assert_equal(node.getblocktemplate({"rules": ["mweb", "segwit"]})["height"], S6B_HEIGHT - 1)

        # And forward again
        with node.assert_debug_log([EVICTION_LOG]):
            node.reconsiderblock(boundary_block.hash)
        assert_equal(node.getblockcount(), tip_height)
        assert_equal(node.getbestblockhash(), tip_hash)
        assert_equal(set(node.getrawmempool()), {reorg_taproot_tx.hash})
        assert_raises_rpc_error(-26, OLD_STYLE, node.sendrawtransaction, old_style.serialize().hex())
        node.sendrawtransaction(new_style.serialize().hex())
        block_hash = node.generatetoaddress(1, node.getnewaddress())[0]
        assert {new_style.hash, reorg_taproot_tx.hash} <= set(node.getblock(block_hash)["tx"])

        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(node1.getbestblockhash(), block_hash)


if __name__ == '__main__':
    S6bSigForkIdTest().main()
