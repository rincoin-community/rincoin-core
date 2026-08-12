#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that the terminal build stops at its terminal height -- and stops cleanly.

This release does not implement the height-840000 consensus rules, so it refuses
to go past them: it validates normally up to H-1 and then shuts down rather than
connect a block at H. On mainnet H is compiled in; here it is set with the
test-chain-only -terminalheight.

The point of this test is not only that the node stops, but *how*. A terminal
build must remain consensus-neutral, which means all of the following:

  * the tip is left at H-1 -- the boundary block is never connected;
  * the boundary block is NOT marked invalid -- it stays an ordinary candidate;
  * the peer that sent it is NOT banned or marked as misbehaving;
  * restarting past the boundary is refused rather than looping sync->abort;
  * the datadir stays usable: a build without the halt picks the same chain up
    and continues, with no reindex and no invalid branch to clear.

If any of those break, this build is no longer neutral -- it is a fork.
"""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal

TERMINAL_HEIGHT = 120
WARNING_LEAD = 5


class TerminalHaltTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # node0: the terminal build under test.
        # node1: an ordinary node that keeps mining past the boundary, standing
        #        in for the rest of the network.
        self.extra_args = [
            ["-terminalheight=%d" % TERMINAL_HEIGHT, "-terminalwarninglead=%d" % WARNING_LEAD],
            [],
        ]

    def setup_network(self):
        # Start disconnected: node1 builds the chain past the boundary first, so
        # node0 meets the boundary block as a fait accompli rather than racing it.
        self.setup_nodes()

    def run_test(self):
        terminal, network = self.nodes[0], self.nodes[1]

        self.log.info("Mine past the boundary on the ordinary node")
        address = network.get_deterministic_priv_key().address
        network.generatetoaddress(TERMINAL_HEIGHT + 5, address)
        assert_equal(network.getblockcount(), TERMINAL_HEIGHT + 5)
        boundary_hash = network.getblockhash(TERMINAL_HEIGHT)

        self.log.info("Connect the terminal node and let it sync into the boundary")
        with terminal.assert_debug_log(["Terminal height %d reached" % TERMINAL_HEIGHT],
                                       timeout=60):
            self.connect_nodes(0, 1)

        self.log.info("The terminal node shuts down of its own accord")
        terminal.wait_until_stopped(timeout=60)

        self.log.info("It stopped on the last block below the boundary")
        # Reading the datadir directly: the node is gone, so this is the only
        # evidence left -- which is exactly what an operator would have.
        terminal_log = os.path.join(terminal.datadir, terminal.chain, 'debug.log')
        with open(terminal_log, encoding='utf-8') as log:
            debug_log = log.read()
        assert "Chain left at height %d" % (TERMINAL_HEIGHT - 1) in debug_log
        assert "This release stops at block height %d" % TERMINAL_HEIGHT in debug_log

        self.log.info("The peer that served the boundary block was not punished")
        assert_equal(network.listbanned(), [])

        self.log.info("Restarting past the boundary is refused")
        # PARTIAL_REGEX because the node's stderr file is reused across runs, so
        # it still carries the message the halt itself wrote a moment ago.
        terminal.assert_start_raises_init_error(
            extra_args=["-terminalheight=%d" % TERMINAL_HEIGHT],
            expected_msg="Error: This release stops at block height %d" % TERMINAL_HEIGHT,
            match=ErrorMatch.PARTIAL_REGEX,
        )

        self.log.info("The chain it left behind is intact and resumable")
        # Same datadir, halt disabled: a successor release must be able to pick
        # this chain up untouched. The boundary block and its successors were
        # downloaded and stored before the halt -- only *connecting* them was
        # refused -- so an ordinary build connects them straight away, with no
        # reindex and nothing to reconsider. Had the halt marked the boundary
        # block invalid, this node would sit at H-1 forever instead.
        self.start_node(0, extra_args=[])
        self.wait_until(lambda: terminal.getblockcount() == TERMINAL_HEIGHT + 5, timeout=60)
        assert_equal(terminal.getbestblockhash(), network.getbestblockhash())

        for tip in terminal.getchaintips():
            assert tip["status"] != "invalid", \
                "terminal halt must never mark a block invalid, got %s" % tip
        assert_equal(terminal.getblock(boundary_hash)["confirmations"] > 0, True)

        self.log.info("Resumed to height %d with no reindex", terminal.getblockcount())


if __name__ == '__main__':
    TerminalHaltTest().main()
