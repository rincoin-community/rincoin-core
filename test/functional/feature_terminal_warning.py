#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the pre-halt warning window.

A node that is going to stop entirely owes its operator more notice than a
log line at the last moment. From nTerminalWarningLead blocks before the
boundary -- 43,200 blocks, about 30 days, on mainnet -- the node carries a
persistent warning that names the height and counts down the blocks remaining.

It has to be persistent and it has to be everywhere an operator might look:
getblockchaininfo, getnetworkinfo and getmininginfo all surface it, and it is
re-stated on startup so an operator who never calls an RPC still sees it in the
log.
"""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

TERMINAL_HEIGHT = 40
WARNING_LEAD = 8
WINDOW_START = TERMINAL_HEIGHT - WARNING_LEAD  # 32


class TerminalWarningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-terminalheight=%d" % TERMINAL_HEIGHT,
            "-terminalwarninglead=%d" % WARNING_LEAD,
        ]]

    def warnings(self, node):
        """The warning as each RPC an operator might reach for reports it."""
        return {
            "getblockchaininfo": node.getblockchaininfo()["warnings"],
            "getnetworkinfo": node.getnetworkinfo()["warnings"],
            "getmininginfo": node.getmininginfo()["warnings"],
        }

    def run_test(self):
        node = self.nodes[0]
        address = node.get_deterministic_priv_key().address

        self.log.info("Below the window there is no warning at all")
        node.generatetoaddress(WINDOW_START - 1, address)
        assert_equal(node.getblockcount(), WINDOW_START - 1)
        for rpc, warning in self.warnings(node).items():
            assert_equal((rpc, warning), (rpc, ""))

        self.log.info("The first block inside the window raises it everywhere")
        node.generatetoaddress(1, address)
        assert_equal(node.getblockcount(), WINDOW_START)
        expected_fragment = "This release stops at block height %d" % TERMINAL_HEIGHT
        for rpc, warning in self.warnings(node).items():
            assert expected_fragment in warning, \
                "%s did not surface the terminal warning: %r" % (rpc, warning)
        # It counts down rather than repeating a static string, so an operator
        # can tell how much time is left without doing arithmetic.
        assert "about %d blocks away" % WARNING_LEAD in self.warnings(node)["getblockchaininfo"]

        self.log.info("The countdown tracks the tip")
        node.generatetoaddress(1, address)
        assert_equal(node.getblockcount(), WINDOW_START + 1)
        assert "about %d blocks away" % (WARNING_LEAD - 1) in \
            self.warnings(node)["getblockchaininfo"]

        self.log.info("It is restated on startup, before any RPC is called")
        self.restart_node(0, extra_args=self.extra_args[0])
        node_log = os.path.join(node.datadir, node.chain, 'debug.log')
        with open(node_log, encoding='utf-8') as log:
            debug_log = log.read()
        assert expected_fragment in debug_log
        # And it is still live after the restart, not just a one-off log line.
        assert expected_fragment in self.warnings(node)["getblockchaininfo"]

        self.log.info("A build with the halt disabled never warns")
        self.restart_node(0, extra_args=[])
        for rpc, warning in self.warnings(node).items():
            assert_equal((rpc, warning), (rpc, ""))


if __name__ == '__main__':
    TerminalWarningTest().main()
