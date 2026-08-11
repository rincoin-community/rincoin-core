#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that a terminal build refuses to produce the boundary block itself.

The halt in ActivateBestChain stops the node connecting a boundary block it
receives. That leaves one gap: between the tip reaching H-1 and the boundary
block arriving from the network, the node is alive and would otherwise happily
*build* the boundary block itself, under rules it does not implement.

So every mining and block-submission route fails closed one block early: from
the moment the next block would be at H, getblocktemplate, all the generate*
variants, submitblock and submitheader refuse. Below that the node mines
perfectly normally -- this build is only terminal at the boundary, not crippled.
"""

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE_DESCRIPTOR
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

TERMINAL_HEIGHT = 30
WARNING_LEAD = 5
# RPC_MISC_ERROR
TERMINAL_RPC_ERROR = -1


class TerminalMiningRefusedTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-terminalheight=%d" % TERMINAL_HEIGHT,
            "-terminalwarninglead=%d" % WARNING_LEAD,
        ]]

    def assert_mining_refused(self, node, address, descriptor):
        expected = "This release stops at block height %d" % TERMINAL_HEIGHT

        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected,
                                node.getblocktemplate, {"rules": ["segwit", "mweb"]})
        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected,
                                node.generatetoaddress, 1, address)
        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected,
                                node.generatetodescriptor, 1, descriptor)
        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected,
                                node.generateblock, address, [])
        # submitblock/submitheader refuse before they even parse their argument,
        # so obvious junk still yields the terminal error rather than a decode
        # error. That ordering is deliberate: fail closed first, parse second.
        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected, node.submitblock, "00")
        assert_raises_rpc_error(TERMINAL_RPC_ERROR, expected, node.submitheader, "00")

    def run_test(self):
        node = self.nodes[0]
        address = node.get_deterministic_priv_key().address
        descriptor = ADDRESS_BCRT1_UNSPENDABLE_DESCRIPTOR

        self.log.info("Mining works normally well below the boundary")
        node.generatetoaddress(TERMINAL_HEIGHT - 2, address)
        assert_equal(node.getblockcount(), TERMINAL_HEIGHT - 2)

        self.log.info("At H-2 the next block is H-1, still allowed")
        template = node.getblocktemplate({"rules": ["segwit", "mweb"]})
        assert_equal(template["height"], TERMINAL_HEIGHT - 1)

        self.log.info("The guard is a function of the height, not a startup flag")
        # Restart while still below the boundary and confirm mining resumes: the
        # refusal must key off the tip, not off some state latched at startup.
        self.restart_node(0, extra_args=self.extra_args[0])
        assert_equal(node.getblockcount(), TERMINAL_HEIGHT - 2)

        self.log.info("Mine the last permitted block, leaving the tip at H-1")
        node.generatetoaddress(1, address)
        assert_equal(node.getblockcount(), TERMINAL_HEIGHT - 1)

        self.log.info("Now the next block would be the boundary: every route refuses")
        self.assert_mining_refused(node, address, descriptor)

        self.log.info("The node itself is otherwise healthy and still serving RPC")
        assert_equal(node.getblockcount(), TERMINAL_HEIGHT - 1)
        assert_equal(node.getblockchaininfo()["blocks"], TERMINAL_HEIGHT - 1)
        # Reaching this point at all proves it has not shut down: the halt fires
        # on a *received* boundary block, not merely on sitting at H-1.


if __name__ == '__main__':
    TerminalMiningRefusedTest().main()
