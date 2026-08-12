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
import re

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
        assert "About %d blocks left" % WARNING_LEAD in self.warnings(node)["getblockchaininfo"]

        self.log.info("The countdown tracks the tip")
        node.generatetoaddress(1, address)
        assert_equal(node.getblockcount(), WINDOW_START + 1)
        assert "About %d blocks left" % (WARNING_LEAD - 1) in \
            self.warnings(node)["getblockchaininfo"]

        self.log.info("Every block inside the window carries the warning in the log")
        # UpdateTip already prints one line per block; the warning rides that
        # line's existing warning='...' field, so per-block visibility costs no
        # extra log lines. Mine one block and require it on that block's line.
        node_log = os.path.join(node.datadir, node.chain, 'debug.log')
        with open(node_log, encoding='utf-8') as log:
            log.seek(0, 2)
            before = log.tell()
        node.generatetoaddress(1, address)
        height = node.getblockcount()
        with open(node_log, encoding='utf-8') as log:
            log.seek(before)
            fresh = log.read()
        tip_lines = [ln for ln in fresh.splitlines()
                     if "UpdateTip: new best" in ln and "height=%d" % height in ln]
        assert tip_lines, "no UpdateTip line for height %d in:\n%s" % (height, fresh)
        assert "warning=" in tip_lines[-1], \
            "the per-block line carries no warning field: %s" % tip_lines[-1]
        assert expected_fragment in tip_lines[-1], \
            "the per-block line does not name the terminal height: %s" % tip_lines[-1]

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

        self.check_loud_cadence(node, address)

    def check_loud_cadence(self, node, address):
        """The loud warning repeats on a fixed, height-derived cadence.

        Quiet blocks still carry the warning on their UpdateTip line and in the
        RPC field; the loud line is the one that also becomes a desktop
        notification, so it must be rare enough not to nag. At 60s spacing the
        whole of this window sits inside the final day, which is the tightest
        tier: every 30 blocks, plus an unconditional one on the last block.
        """
        terminal, lead = 200, 80
        self.log.info("Loud warnings follow the tier cadence, not every block")
        self.restart_node(0, extra_args=["-terminalheight=%d" % terminal,
                                         "-terminalwarninglead=%d" % lead])

        node_log = os.path.join(node.datadir, node.chain, 'debug.log')
        with open(node_log, encoding='utf-8') as log:
            log.seek(0, 2)
            mark = log.tell()

        node.generatetoaddress(terminal - 1 - node.getblockcount(), address)
        assert_equal(node.getblockcount(), terminal - 1)

        with open(node_log, encoding='utf-8') as log:
            log.seek(mark)
            fresh = log.read()

        # The loud line is the one prefixed with '***'.
        loud = re.findall(r"\*\*\* This release stops at block height %d\. About (\d+) blocks left"
                          % terminal, fresh)
        heights = sorted(terminal - int(r) for r in loud)

        # Every 30 blocks through the window, plus the last block before the halt.
        expected = sorted({h for h in range(terminal - lead, terminal) if h % 30 == 0}
                          | {terminal - 1})
        assert_equal(heights, expected)

        # Far fewer than one per block: that is the whole point.
        assert len(heights) < lead / 4, \
            "loud warning fired %d times in %d blocks -- too noisy" % (len(heights), lead)
        self.log.info("  %d loud warnings across %d blocks, at heights %s",
                      len(heights), lead, heights)


if __name__ == '__main__':
    TerminalWarningTest().main()
