#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the first start of this release on a data directory where software with other rules
for the height-840,000 transition (840 on regtest) ran past that height.

Such software (an older release, or another implementation) connects its own blocks from the
transition height and marks the valid transition block as invalid when it sees it. Here it is
emulated with -disables6b: the earlier rules at every height, and block index entries written
with the client version of 1.1.0. On the first start of this release on the same data
directory the node disconnects the blocks that software connected, forgets the invalidity it
found, and so ends on the valid chain by itself. Later starts do nothing of the kind.

Old nodes, each with the shared history below the transition height:
- seen:     saw the valid transition block (and rejected it), then built more work of its own;
- stalled:  saw and rejected the valid transition block and stayed below the transition height;
- unseen:   never saw the valid chain, built more work of its own;
- reindex:  like "seen", but the first start of this release is with -reindex-chainstate.
"""

from test_framework.s6b_util import S6B_HEIGHT, mine_to_height
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

OLD_RULES = ["-disables6b"]
REVALIDATE = "RevalidateS6bBlocks"


class S6bForeignUpgradeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True
        noban = ["-whitelist=noban@127.0.0.1"]
        self.extra_args = [noban] + [noban + OLD_RULES] * 4

    def setup_network(self):
        self.setup_nodes()
        for i in range(1, self.num_nodes):
            self.connect_nodes(i, 0)

    def chain_status(self, node, block_hash):
        return {t["hash"]: t["status"] for t in node.getchaintips()}.get(block_hash, "in a branch")

    def upgrade(self, i, expected_msgs, extra_args=()):
        with self.nodes[i].assert_debug_log(expected_msgs=expected_msgs):
            self.restart_node(i, extra_args=["-whitelist=noban@127.0.0.1"] + list(extra_args))

    def run_test(self):
        valid, seen, stalled, unseen, reindex = self.nodes
        self.log.info("Shared history up to the last block before the transition height")
        mine_to_height(valid, S6B_HEIGHT - 1)
        self.sync_blocks()
        for i in range(1, self.num_nodes):
            self.disconnect_nodes(i, 0)

        valid_tip = valid.generate(3)[-1]
        valid_840 = valid.getblockhash(S6B_HEIGHT)
        valid_block_840 = valid.getblock(valid_840, 0)

        self.log.info("The old nodes reject the valid transition block (it claims the new subsidy)")
        for node in (seen, stalled, reindex):
            assert_equal(node.submitblock(valid_block_840), "bad-cb-amount")
            assert_equal(self.chain_status(node, valid_840), "invalid")
        self.log.info("... and build more work of their own from the transition height")
        own_tip = {}
        for node in (seen, unseen, reindex):
            own_tip[node.index] = node.generate(6)[-1]
            assert_equal(node.getblockcount(), S6B_HEIGHT + 5)
        assert_equal(stalled.getblockcount(), S6B_HEIGHT - 1)

        self.log.info("seen: this release disconnects the old node's blocks and takes the valid transition block from disk")
        self.upgrade(seen.index, [
            "%s: 7 block index entries from height %d were written by a client without the rules" % (REVALIDATE, S6B_HEIGHT),
            "%s: disconnecting the active chain from height %d (tip %d)" % (REVALIDATE, S6B_HEIGHT, S6B_HEIGHT + 5),
            "%s: 1 blocks from height %d are no longer marked invalid" % (REVALIDATE, S6B_HEIGHT),
        ])
        self.wait_until(lambda: seen.getbestblockhash() == valid_840)
        assert_equal(self.chain_status(seen, own_tip[seen.index]), "invalid")

        self.log.info("stalled: nothing to disconnect, the valid transition block is no longer marked invalid")
        self.upgrade(stalled.index, [
            "%s: 1 block index entries from height %d" % (REVALIDATE, S6B_HEIGHT),
            "%s: 1 blocks from height %d are no longer marked invalid" % (REVALIDATE, S6B_HEIGHT),
        ])
        self.wait_until(lambda: stalled.getbestblockhash() == valid_840)

        self.log.info("unseen: the old node's blocks are disconnected and found invalid")
        self.upgrade(unseen.index, [
            "%s: 6 block index entries from height %d" % (REVALIDATE, S6B_HEIGHT),
            "%s: disconnecting the active chain from height %d (tip %d)" % (REVALIDATE, S6B_HEIGHT, S6B_HEIGHT + 5),
        ])
        self.wait_until(lambda: self.chain_status(unseen, own_tip[unseen.index]) == "invalid")
        assert_equal(unseen.getblockcount(), S6B_HEIGHT - 1)

        self.log.info("reindex: the same with -reindex-chainstate")
        self.upgrade(reindex.index, ["%s: 7 block index entries from height %d" % (REVALIDATE, S6B_HEIGHT)],
                     ["-reindex-chainstate"])
        self.wait_until(lambda: reindex.getbestblockhash() == valid_840, timeout=300)
        assert_equal(self.chain_status(reindex, own_tip[reindex.index]), "invalid")

        self.log.info("All of them follow the valid chain, with the same UTXO set")
        for i in range(1, self.num_nodes):
            self.connect_nodes(i, 0)
        self.sync_blocks()
        valid_tip = valid.generate(1)[0]
        self.sync_blocks()
        utxo_hash = valid.gettxoutsetinfo()["hash_serialized_2"]
        for node in self.nodes:
            assert_equal(node.getbestblockhash(), valid_tip)
            assert_equal(node.gettxoutsetinfo()["hash_serialized_2"], utxo_hash)

        self.log.info("Later starts, and starts of a node that always ran this release, do not check anything again")
        for node in self.nodes:
            with node.assert_debug_log(expected_msgs=[], unexpected_msgs=[REVALIDATE]):
                self.restart_node(node.index, extra_args=["-whitelist=noban@127.0.0.1"])
            assert_equal(node.getbestblockhash(), valid_tip)


if __name__ == '__main__':
    S6bForeignUpgradeTest().main()
