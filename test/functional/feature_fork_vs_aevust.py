#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test S1 interop against a foreign, unrelated Rincoin implementation
(Scenario 4: new+foreign node).

Aevust/rincoin (branch configurable, default feature/port-sim-v1.0.7) is a
different team's implementation with its own piecewise subsidy schedule, no
coinbase commitment, no sig_fork_id, and an unchanged protocol version
(70017). It is treated as a fully separate, foreign repository throughout
this framework: no remote for it is ever added to this repo, and this test
only talks to a standalone binary built from a standalone clone (see
test/build_reference_node.py --foreign).

Tests the header/full-block asymmetry the design docs call out explicitly
(consensus-transition.md §5, "what this does not guarantee"): a
higher-work alternative branch's headers can be learned by our node, but
full-block validation rejects it at H1 for missing/wrong commitment -- the
validated active tip must never adopt it, regardless of how much declared
work sits on top. Also tests that IBD started against a peer set dominated
by the foreign branch still converges on the correct chain once a correct
peer is reachable, rather than wedging.

Requires a reference binary built via:
    test/build_reference_node.py --foreign \\
        aevust=https://github.com/Aevust/rincoin.git#feature/port-sim-v1.0.7
"""

import os
import time
from pathlib import Path

from test_framework.fork_scenario import FORK_H1_HEIGHT, FORK_H1_EXTRA_ARG
from test_framework.fork_util import mine_to_height
from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import assert_equal

REPO_ROOT = Path(__file__).resolve().parents[2]
AEVUST_LABEL = "aevust"


def reference_binary(label, name="rincoind"):
    return str(REPO_ROOT / "releases" / label / "bin" / name)


def reference_available(label):
    return os.path.isfile(reference_binary(label)) and os.path.isfile(reference_binary(label, "rincoin-cli"))


class ForkVsAevustTest(BitcoinTestFramework):
    def set_test_params(self):
        # node0: our S1 build. node1, node2: Aevust foreign build (node2
        # only used in the IBD-hostile-majority sub-test). node3: a fresh
        # S1 build used as the IBD client in that same sub-test.
        self.num_nodes = 4
        self.setup_clean_chain = True
        # -forkh1height= only exists on the S1 build (node0, node3) -- the
        # foreign Aevust binary (node1, node2) would fail to start on an
        # unrecognized option.
        self.extra_args = [[FORK_H1_EXTRA_ARG], [], [], [FORK_H1_EXTRA_ARG]]

    def skip_test_if_missing_module(self):
        if not reference_available(AEVUST_LABEL):
            raise SkipTest(
                "aevust reference binary not built -- run "
                "test/build_reference_node.py --foreign "
                "aevust=https://github.com/Aevust/rincoin.git#feature/port-sim-v1.0.7 first"
            )

    def setup_nodes(self):
        s1_bin, s1_cli = self.options.bitcoind, self.options.bitcoincli
        aevust_bin = reference_binary(AEVUST_LABEL)
        aevust_cli = reference_binary(AEVUST_LABEL, "rincoin-cli")
        self.add_nodes(
            self.num_nodes,
            extra_args=self.extra_args,
            binary=[s1_bin, aevust_bin, aevust_bin, s1_bin],
            binary_cli=[s1_cli, aevust_cli, aevust_cli, s1_cli],
        )
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    def setup_network(self):
        # Deliberately not the default chain-topology + sync_all(): every
        # sub-test below manages its own connect_nodes()/disconnect_nodes()
        # calls to control exactly when and how far chains are allowed to
        # diverge before being reconnected. Leaving the default topology in
        # place would keep node0 and the Aevust build permanently connected
        # from the start, silently syncing them with each other instead of
        # letting them diverge as each sub-test intends.
        self.setup_nodes()

    def is_node_alive(self, node):
        return node.process is not None and node.process.poll() is None

    def run_test(self):
        self.header_full_block_asymmetry()
        self.ibd_with_hostile_peer_majority()

    def header_full_block_asymmetry(self):
        node0, aevust = self.nodes[0], self.nodes[1]

        self.log.info(f"Diverging node0 (S1) and the Aevust build independently past H1 ({FORK_H1_HEIGHT})")
        # Both nodes start disconnected from genesis, so there's no reason
        # to expect matching hashes at any point -- each mines its own
        # coinbase (different address/timestamp/nonce) even under identical
        # consensus rules. Only the heights matter here, not the hashes.
        mine_to_height(node0, FORK_H1_HEIGHT - 1)
        mine_to_height(aevust, FORK_H1_HEIGHT - 1)

        mine_to_height(node0, FORK_H1_HEIGHT + 2)
        mine_to_height(aevust, FORK_H1_HEIGHT + 3)  # foreign branch has *more* work

        self.log.info("Connecting node0 to the higher-work Aevust branch")
        self.connect_nodes(0, 1)
        time.sleep(5)  # bounded grace period for header/block relay + rejection

        self.log.info("node0's validated active tip must not adopt the foreign branch")
        assert self.is_node_alive(node0), "node0 must not crash when offered an incompatible higher-work branch"
        assert_equal(node0.getblockcount(), FORK_H1_HEIGHT + 2)
        assert node0.getbestblockhash() != aevust.getbestblockhash()

        tips = node0.getchaintips()
        active = [t for t in tips if t["status"] == "active"]
        assert_equal(len(active), 1)
        assert_equal(active[0]["hash"], node0.getbestblockhash())
        non_active = [t for t in tips if t["status"] != "active"]
        self.log.info(f"node0 getchaintips shows {len(non_active)} non-active branch(es) "
                       "(expected to include the Aevust branch's headers, not adopted as active)")

        self.disconnect_nodes(0, 1)

    def ibd_with_hostile_peer_majority(self):
        node0, aevust1, aevust2, fresh = self.nodes

        self.log.info("Extending the Aevust majority's chain further")
        mine_to_height(aevust1, FORK_H1_HEIGHT + 5)
        self.connect_nodes(1, 2)
        time.sleep(2)

        self.log.info("Starting IBD on a fresh S1 node connected to 2 Aevust peers + 1 correct S1 peer")
        self.connect_nodes(3, 0)
        self.connect_nodes(3, 1)
        self.connect_nodes(3, 2)

        def converged():
            return self.is_node_alive(fresh) and fresh.getbestblockhash() == node0.getbestblockhash()

        self.wait_until(converged, timeout=120)
        self.log.info("Fresh node converged on the correct S1 chain despite a hostile-majority peer set")
        assert self.is_node_alive(fresh)
        assert_equal(fresh.getbestblockhash(), node0.getbestblockhash())


if __name__ == '__main__':
    ForkVsAevustTest().main()
