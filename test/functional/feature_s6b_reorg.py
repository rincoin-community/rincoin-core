#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test chain reorganizations around the height-840,000 transition (840 on regtest).

- A valid reorganization across the transition height: two nodes mine competing branches
  that both cross it. The node on the shorter branch switches over. Transactions of its
  disconnected blocks return to the mempool when they are signed for the new side of the
  transition; one that was confirmed below the transition height on the abandoned branch
  carries a historical signature, cannot confirm above it any more, and has to be made
  again. The coin supply matches the schedule afterwards.
- A branch with more work whose transition block is invalid must not win: the block
  claims the subsidy of the previous schedule, as a miner that has not upgraded would,
  and more blocks are built on it than the valid chain has. Nodes that hear about it
  from a peer keep (or later find) the valid chain, also after a restart and -reindex.
"""

from decimal import Decimal

from test_framework.messages import COIN
from test_framework.p2p import P2PDataStore
from test_framework.s6b_util import (
    S6B_HEIGHT,
    build_block,
    mine_to_height,
    subsidy,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

PREVIOUS_SCHEDULE_SUBSIDY = 312500000
OLD_STYLE = "old-style-sig-fork-id"


class S6bReorgTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def pay(self, node, utxo, dest, amount):
        """A wallet payment from exactly one coin, so that the payments of this test do not depend on each other."""
        change = utxo["amount"] - amount - Decimal("0.0001")
        raw = node.createrawtransaction([{"txid": utxo["txid"], "vout": utxo["vout"]}],
                                        [{dest: amount}, {node.getrawchangeaddress(): change}])
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def invalid_branch(self, node, length):
        """Blocks on top of the last block below the transition height: a transition block that claims
        the subsidy of the previous schedule, and `length` - 1 blocks that do the same after it."""
        parent = node.getblock(node.getblockhash(S6B_HEIGHT - 1))
        prev_hash, prev_height, prev_time = parent["hash"], parent["height"], parent["time"]
        blocks = []
        for _ in range(length):
            block = build_block(node, coinbase_value=PREVIOUS_SCHEDULE_SUBSIDY, prev_hash=prev_hash,
                                prev_height=prev_height, prev_time=prev_time)
            blocks.append(block)
            prev_hash, prev_height, prev_time = block.hash, prev_height + 1, block.nTime
        return blocks

    def offer_invalid_branch(self, node, blocks):
        """A peer announces the branch by its headers and serves the blocks the node asks for."""
        tip = node.getbestblockhash()
        peer = node.add_p2p_connection(P2PDataStore())
        peer.send_blocks_and_test(blocks, node, success=False, reject_reason="bad-cb-amount-transition",
                                  expect_disconnect=True, timeout=120)
        assert_equal(node.getbestblockhash(), tip)
        tips = {t["hash"]: t for t in node.getchaintips()}
        assert_equal(tips[tip]["status"], "active")
        # The node knows the branch, but none of it counts as valid. (Blocks after the invalid one are only
        # marked invalid when the node looks at them again, so its tip may still be listed as headers-only.)
        assert_equal(tips[blocks[-1].hash]["height"], S6B_HEIGHT - 1 + len(blocks))
        assert tips[blocks[-1].hash]["status"] in ("invalid", "headers-only"), tips
        # submitblock refuses its blocks as known invalid ones
        assert_equal(node.submitblock(blocks[0].serialize().hex()), "duplicate-invalid")
        assert node.submitblock(blocks[1].serialize().hex()) in ("duplicate-invalid", "bad-prevblk", "inconclusive")
        assert_equal(node.getbestblockhash(), tip)

    def run_test(self):
        node0, node1 = self.nodes
        mine_to_height(node0, 200, node0.getnewaddress())
        mine_to_height(node0, S6B_HEIGHT - 5)
        self.sync_blocks()

        self.log.info("Two branches that cross the transition height")
        self.disconnect_nodes(0, 1)
        dest = node1.getnewaddress()
        coins = [u for u in node0.listunspent() if u["amount"] == 50][:4]
        below = self.pay(node0, coins[0], dest, 1)  # historical signature
        node0.generatetoaddress(1, node0.getnewaddress())
        assert_equal(node0.gettransaction(below)["confirmations"], 1)
        mine_to_height(node0, S6B_HEIGHT - 1)
        at_transition = self.pay(node0, coins[1], dest, 2)  # signed for the transition block
        fee = int(-node0.gettransaction(at_transition)["fee"] * COIN)
        transition_hash = node0.generatetoaddress(1, node0.getnewaddress())[0]
        coinbase = node0.getblock(transition_hash, 2)["tx"][0]
        assert_equal(int(sum(o["value"] for o in coinbase["vout"]) * COIN), subsidy(S6B_HEIGHT) + fee)
        above = self.pay(node0, coins[2], dest, 3)
        node0.generatetoaddress(2, node0.getnewaddress())
        assert_equal(node0.getblockcount(), S6B_HEIGHT + 2)

        node1.generatetoaddress(11, node1.getnewaddress())
        assert_equal(node1.getblockcount(), S6B_HEIGHT + 6)

        self.log.info("The node on the shorter branch reorganizes across the transition height")
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(node0.getbestblockhash(), node1.getbestblockhash())
        assert_equal(node0.getblockcount(), S6B_HEIGHT + 6)
        # Signed for the new side: back in the mempool. Signed for the historical side: gone for good.
        assert_equal(set(node0.getrawmempool()), {at_transition, above})
        assert_equal(node0.gettransaction(below)["confirmations"], 0)
        assert_raises_rpc_error(-26, OLD_STYLE, node0.sendrawtransaction, node0.gettransaction(below)["hex"])
        node0.abandontransaction(below)
        again = self.pay(node0, coins[0], dest, 1)  # the same coin, signed for the new side
        # (a node does not announce transactions that a reorganization returned to its mempool, so it mines them itself)
        block_hash = node0.generatetoaddress(1, node0.getnewaddress())[0]
        self.sync_blocks()
        assert {at_transition, above, again} <= set(node0.getblock(block_hash)["tx"])
        assert_equal(node1.getreceivedbyaddress(dest), 6)

        self.log.info("The coin supply matches the schedule")
        for node in self.nodes:
            tip = node.getblockcount()
            supply = sum(subsidy(h) for h in range(1, tip + 1))
            assert_equal(node.gettxoutsetinfo()["total_amount"], Decimal(supply) / COIN)

        self.log.info("A longer branch with an invalid transition block does not win")
        self.disconnect_nodes(0, 1)
        valid_tip = node0.getbestblockhash()
        valid_height = node0.getblockcount()
        # (short enough for a node to ask for all of its blocks at once)
        branch = self.invalid_branch(node0, valid_height - (S6B_HEIGHT - 1) + 3)
        assert len(branch) <= 16
        self.offer_invalid_branch(node0, branch)

        self.log.info("- nor after a restart or -reindex")
        for extra in ([], ["-reindex"]):
            self.restart_node(0, extra_args=extra)
            self.wait_until(lambda: node0.getblockcount() == valid_height, timeout=300)
            assert_equal(node0.getbestblockhash(), valid_tip)

        self.log.info("- nor on a node that is still below the transition height when it hears about it")
        node1.invalidateblock(node1.getblockhash(S6B_HEIGHT))
        assert_equal(node1.getblockcount(), S6B_HEIGHT - 1)
        self.offer_invalid_branch(node1, branch)
        assert_equal(node1.getblockcount(), S6B_HEIGHT - 1)
        node1.reconsiderblock(node0.getblockhash(S6B_HEIGHT))
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(node1.getbestblockhash(), valid_tip)


if __name__ == '__main__':
    S6bReorgTest().main()
