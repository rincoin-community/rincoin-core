#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test S6/b interop against two old (pre-840k-design) Rincoin releases
(Scenario 3: old+new node).

  - v1.1.0 / legacy-1.1 (rincoin-community/rincoin-core). This repo's own
    BRANCHES.md/CLAUDE.md describe legacy-1.1 as "a frozen leaf that
    deliberately shuts down at block 840,000" -- but that was verified here
    (Open Risk R8 in the approved plan) to be a *policy* statement about the
    team's development intentions, not a consensus rule actually compiled
    into the v1.1.0 binary: connected to a live post-H1 S6/b chain, it
    followed every single block with zero rejection, well past H1, with no
    fork-awareness of any kind. This is exactly the gap the coinbase
    commitment + sig_fork_id design exists to close -- an old, unmodified
    node offers no protection on its own; see consensus-transition.md §2's
    point that the coinbase rule is an upper bound, not an exact amount, so
    a subsidy difference alone can never separate chains. Shares this
    repo's regtest genesis block, so this is a real, meaningful P2P test,
    not just a connectivity check.

  - v1.0.1 (Rin-coin/rincoin, via the pre-existing `legacy` remote): predates
    the whole 840k design and has no fork awareness at all either. Verified
    directly (via `getblockhash 0` on both regtest and testnet) to have a
    *different genesis block* from the current codebase on every network --
    it predates a full network relaunch and has never shared a single block
    of history with anything built from this repo. A real "old node reacts
    to our post-H1 chain" P2P test isn't possible against it for that
    reason; what's tested instead is the honest, weaker invariant that
    still matters operationally: connecting to it doesn't crash our node,
    and the two chains simply never converge (which they never could have,
    with or without this fork).

Both cases must leave node0 (our S6/b build) itself completely unaffected,
and must not crash either old node.

Requires reference binaries built via test/build_reference_node.py:
    test/build_reference_node.py --ref v1.1.0=v1.1.0 --ref v1.0.1=v1.0.1
"""

import os
from pathlib import Path

from test_framework.fork_scenario import FORK_H1_HEIGHT, FORK_H1_EXTRA_ARG
from test_framework.fork_util import mine_to_height
from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import assert_equal

REPO_ROOT = Path(__file__).resolve().parents[2]


def reference_binary(label, name="rincoind"):
    path = REPO_ROOT / "releases" / label / "bin" / name
    return str(path)


def reference_available(label):
    return os.path.isfile(reference_binary(label)) and os.path.isfile(reference_binary(label, "rincoin-cli"))


class ForkVsLegacyTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # node0 = our S6/b build, node1 = v1.1.0/legacy-1.1, node2 = v1.0.1.
        # -forkh1height= only exists on node0's build -- the old binaries
        # would fail to start on an unrecognized option.
        self.extra_args = [[FORK_H1_EXTRA_ARG], [], []]

    def skip_test_if_missing_module(self):
        if not (reference_available("v1.1.0") and reference_available("v1.0.1")):
            raise SkipTest(
                "reference binaries not built -- run "
                "test/build_reference_node.py --ref v1.1.0=v1.1.0 --ref v1.0.1=v1.0.1 first"
            )

    def setup_nodes(self):
        self.add_nodes(
            self.num_nodes,
            extra_args=self.extra_args,
            binary=[self.options.bitcoind, reference_binary("v1.1.0"), reference_binary("v1.0.1")],
            binary_cli=[self.options.bitcoincli, reference_binary("v1.1.0", "rincoin-cli"),
                        reference_binary("v1.0.1", "rincoin-cli")],
        )
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    def setup_network(self):
        # Deliberately not the default chain-topology + sync_all(): node2
        # (v1.0.1) shares no genesis block with node0/node1 and can never
        # sync with them, so a blanket sync_all() would just time out.
        # Connect node0<->node1 (genesis-compatible, real sync exercised
        # below) and node0<->node2 (genesis-incompatible, connection-only)
        # separately, each with the expectations that actually apply to it.
        self.setup_nodes()
        self.connect_nodes(1, 0)
        self.sync_all([self.nodes[0], self.nodes[1]])
        self.connect_nodes(2, 0)

    def is_node_alive(self, node):
        return node.process is not None and node.process.poll() is None

    def run_test(self):
        node0, legacy110, legacy101 = self.nodes

        self.log.info(f"Syncing node0/legacy-1.1 (shared genesis) to just before H1 ({FORK_H1_HEIGHT - 1})")
        mine_to_height(node0, FORK_H1_HEIGHT - 1)
        self.sync_blocks([node0, legacy110])
        assert_equal(legacy110.getblockcount(), FORK_H1_HEIGHT - 1)

        self.log.info("v1.0.1 (no shared genesis, verified separately): connected but never syncs any block")
        assert_equal(legacy101.getblockcount(), 0)
        assert legacy101.getbestblockhash() != node0.getbestblockhash()

        self.log.info("node0 mines past H1 with S6/b rules (commitment + new subsidy ceiling)")
        mine_to_height(node0, FORK_H1_HEIGHT + 2)

        self.log.info("Waiting for legacy-1.1 to receive and process the post-H1 chain")
        self.wait_until(lambda: node0.getblockcount() == FORK_H1_HEIGHT + 2)
        self.sync_blocks([node0, legacy110])

        self.log.info("legacy-1.1 (v1.1.0): follows the post-H1 chain with zero fork-awareness "
                       "(empirically verified -- see module docstring); it must at least not crash")
        assert self.is_node_alive(legacy110), "legacy-1.1 process must still be running"
        assert_equal(legacy110.getblockcount(), FORK_H1_HEIGHT + 2)
        assert_equal(legacy110.getbestblockhash(), node0.getbestblockhash())

        self.log.info("v1.0.1: still alive and still on its own separate genesis, unaffected by node0's activity")
        assert self.is_node_alive(legacy101), "v1.0.1 process must still be running"
        assert_equal(legacy101.getblockcount(), 0)
        assert legacy101.getbestblockhash() != node0.getbestblockhash()

        self.log.info("node0 itself is unaffected by either old node")
        assert_equal(node0.getblockcount(), FORK_H1_HEIGHT + 2)


if __name__ == '__main__':
    ForkVsLegacyTest().main()
