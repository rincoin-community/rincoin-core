#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test reorg behavior around the height-840,000 fork (Scenario 2: two new
nodes, one overtaking work from the other).

Two sub-tests:

1. Two real S5/b nodes, isolated, each mine a competing branch straddling H1,
   then reconnect: the higher-work *valid* branch must win via ordinary
   reorg -- this exercises full-block (re-)validation of the commitment and
   subsidy rules along a reorg path, not just the initial-connect path.

2. A single S5/b node facing a synthetic, hand-built alternative branch (fed
   via a raw P2P peer) that has *more* cumulative work than the node's own
   real chain but is invalid at the first post-H1 block (missing
   commitment): headers may be accepted (extending the node's header index),
   but the active, validated tip must never adopt it -- "regardless of how
   much proof-of-work an incompatible continuation accumulates on top of it"
   (rincoin-consensus840k/technology/consensus-transition.md §5).
"""

from test_framework.blocktools import create_block, create_coinbase
from test_framework.fork_scenario import FORK_H1_HEIGHT, FORK_H1_EXTRA_ARG, expected_subsidy
from test_framework.fork_util import mine_to_height
from test_framework.p2p import P2PDataStore
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class ForkReorgTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [[FORK_H1_EXTRA_ARG]] * self.num_nodes

    def run_test(self):
        self.reorg_across_h1()
        self.higher_work_invalid_branch_does_not_win()

    def reorg_across_h1(self):
        node0, node1 = self.nodes[0], self.nodes[1]
        sync_height = FORK_H1_HEIGHT - 3

        self.log.info(f"Syncing both nodes to height {sync_height} (pre-fork)")
        mine_to_height(node0, sync_height)
        self.sync_blocks()

        self.log.info("Isolating node0 and node1, mining a shorter branch on node0, "
                       "a longer (higher-work) branch straddling H1 on node1")
        self.disconnect_nodes(0, 1)
        mine_to_height(node0, sync_height + 2)   # still pre-fork
        mine_to_height(node1, sync_height + 5)   # crosses H1

        assert node0.getbestblockhash() != node1.getbestblockhash()

        self.log.info("Reconnecting: node0 must reorg onto node1's higher-work, "
                       "post-H1-validated branch")
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(node0.getbestblockhash(), node1.getbestblockhash())
        assert_equal(node0.getblockcount(), sync_height + 5)
        assert node0.getblockcount() >= FORK_H1_HEIGHT

    def higher_work_invalid_branch_does_not_win(self):
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PDataStore())

        real_tip_height = node.getblockcount()
        real_tip_hash = node.getbestblockhash()
        fork_point_hash = int(real_tip_hash, 16)
        fork_point_height = real_tip_height

        self.log.info(f"Building a synthetic alternative branch of "
                       f"{3} blocks from height {fork_point_height} "
                       "(more than the 1 additional block the real chain will gain), "
                       "invalid at its first post-H1 block (missing commitment)")

        alt_blocks = []
        prev_hash = fork_point_hash
        prev_height = fork_point_height
        for i in range(3):
            height = prev_height + 1
            coinbase = create_coinbase(height=height)
            # Use the correct post-H1 subsidy ceiling so the block's *only*
            # defect is the missing commitment -- isolates what's actually
            # being tested from an unrelated bad-cb-amount rejection.
            if height >= FORK_H1_HEIGHT:
                coinbase.vout[0].nValue = expected_subsidy(
                    height, h1_height=FORK_H1_HEIGHT, halving_interval=150, base_reward=50 * 10**8)
                coinbase.rehash()
            # Deliberately omit any RINF commitment output at/after H1 -- this
            # branch must be rejected at the first such block, regardless of
            # how much additional (higher) work is piled on top of it.
            block = create_block(hashprev=prev_hash, coinbase=coinbase,
                                  ntime=node.getblock(real_tip_hash)["time"] + 1 + i)
            block.solve()
            alt_blocks.append(block)
            prev_hash = block.sha256
            prev_height = height

        # A block failing a hard consensus check (BLOCK_CONSENSUS, matching
        # the classification used for e.g. bad-cb-height) earns the sending
        # peer the maximum misbehavior score and gets disconnected -- expect
        # that rather than treating it as a P2P-level failure.
        peer.send_blocks_and_test(alt_blocks, node, success=False, force_send=True, expect_disconnect=True)

        self.log.info("Active tip must remain the node's own real chain despite the "
                       "higher-work alternative branch's headers being known to the node")
        assert_equal(node.getbestblockhash(), real_tip_hash)
        assert_equal(node.getblockcount(), real_tip_height)

        # The peer was disconnected immediately on the first invalid block
        # (see expect_disconnect above), so later blocks in the synthetic
        # branch were never necessarily delivered -- don't assume the whole
        # alt_blocks chain is indexed. The one invariant that must hold
        # regardless: whatever chaintips exist, none of them is active
        # except the node's own real chain.
        tips = node.getchaintips()
        active_tips = [t for t in tips if t["status"] == "active"]
        assert_equal(len(active_tips), 1)
        assert_equal(active_tips[0]["hash"], real_tip_hash)


if __name__ == '__main__':
    ForkReorgTest().main()
