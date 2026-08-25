#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the -uacomment option."""

import re

from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch


class UacommentTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        # consensus/s5b-testing: this build always stamps a fixed
        # "s5b-testing-rc1" marker as the first uacomment component (see the
        # unconditional uacomments.push_back() in init.cpp) so the
        # fork-testing build self-identifies on the wire regardless of what
        # -uacomment values an operator adds -- every expected subversion
        # string below has to account for it, unlike upstream.
        self.log.info("test multiple -uacomment")
        subversion = self.nodes[0].getnetworkinfo()["subversion"]
        assert subversion.endswith("(s5b-testing-rc1; testnode0)/"), subversion

        self.restart_node(0, ["-uacomment=foo"])
        subversion = self.nodes[0].getnetworkinfo()["subversion"]
        assert subversion.endswith("(s5b-testing-rc1; testnode0; foo)/"), subversion

        self.log.info("test -uacomment max length")
        self.stop_node(0)
        expected = r"Error: Total length of network version string \([0-9]+\) exceeds maximum length \(256\). Reduce the number or size of uacomments."
        self.nodes[0].assert_start_raises_init_error(["-uacomment=" + 'a' * 256], expected, match=ErrorMatch.FULL_REGEX)

        self.log.info("test -uacomment unsafe characters")
        for unsafe_char in ['/', ':', '(', ')', '₿', '🏃']:
            expected = r"Error: User Agent comment \(" + re.escape(unsafe_char) + r"\) contains unsafe characters."
            self.nodes[0].assert_start_raises_init_error(["-uacomment=" + unsafe_char], expected, match=ErrorMatch.FULL_REGEX)


if __name__ == '__main__':
    UacommentTest().main()
