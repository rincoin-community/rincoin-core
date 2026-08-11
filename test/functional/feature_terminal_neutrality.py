#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that the terminal build changes no consensus rule below its boundary.

This is the evidence behind the central claim of the terminal release: below the
terminal height it is byte-for-byte the same validator as a build without the
halt. It takes no side in the height-840000 question -- it stops before it.

The claim is checked by running two nodes over an identical chain, one with the
halt armed and one without, and asserting they agree on everything: the tip, the
work, the UTXO set commitment, and the whole of getblockchaininfo.

Two block shapes are included deliberately, because they are the ones a reader
might suspect a fork-aware build of treating specially:

  * a coinbase carrying an extra zero-value OP_RETURN output, alongside the
    ordinary witness commitment -- the shape a coinbase branch marker would take;
  * a header with an unusual but valid nVersion -- the shape a lineage bit would
    take.

Neither is given any meaning here. Both nodes must accept both, identically.
"""

from test_framework.blocktools import (
    add_witness_commitment,
    create_block,
    create_coinbase,
)
from test_framework.script import CScript, OP_RETURN
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

TERMINAL_HEIGHT = 25

# The shape a coinbase branch commitment would take: a zero-value OP_RETURN in
# an unrelated namespace. This build must treat it as the ordinary, meaningless
# data carrier it is.
UNRELATED_OP_RETURN = CScript([OP_RETURN, b"not-a-branch-marker"])

# An unusual but valid header version: top bits set as usual, plus a high bit no
# deployment in this build uses. A lineage-bit build would care; this one must not.
UNUSUAL_BLOCK_VERSION = 0x20000000 | (1 << 30)

# The ordinary version this chain's blocks carry. create_block() defaults to 1,
# which regtest rejects outright as bad-version, so be explicit.
ORDINARY_BLOCK_VERSION = 0x20000000


class TerminalNeutralityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        # node0 has the halt armed; node1 is an ordinary build. Everything below
        # the boundary must be indistinguishable between them.
        self.extra_args = [
            ["-terminalheight=%d" % TERMINAL_HEIGHT, "-terminalwarninglead=1"],
            [],
        ]

    def build_block(self, node, *, extra_output_script=None, version=None):
        """Build a block on top of node's tip, optionally with an odd shape."""
        tip = node.getbestblockhash()
        height = node.getblockcount() + 1
        block_time = node.getblock(tip)["mediantime"] + 1
        coinbase = create_coinbase(height, extra_output_script=extra_output_script)
        block = create_block(int(tip, 16), coinbase, block_time,
                             version=version or ORDINARY_BLOCK_VERSION)
        add_witness_commitment(block)
        block.solve()
        return block

    def submit_to_both(self, block, description):
        """Submit the same block to both nodes and require identical acceptance."""
        results = [node.submitblock(block.serialize().hex()) for node in self.nodes]
        assert_equal(results[0], results[1])
        assert results[0] is None, \
            "%s was rejected (%r) -- the terminal build must not add rules" % (
                description, results[0])
        self.log.info("  both nodes accepted %s", description)

    def assert_nodes_agree(self, where):
        terminal_info = self.nodes[0].getblockchaininfo()
        ordinary_info = self.nodes[1].getblockchaininfo()

        # The warning field is the one legitimate difference: the terminal build
        # is allowed to say it is going to stop. Nothing about validation may
        # differ, so everything else must match exactly.
        terminal_info.pop("warnings", None)
        ordinary_info.pop("warnings", None)
        assert_equal(terminal_info, ordinary_info)

        assert_equal(self.nodes[0].getbestblockhash(), self.nodes[1].getbestblockhash())
        assert_equal(self.nodes[0].gettxoutsetinfo()["hash_serialized_2"],
                     self.nodes[1].gettxoutsetinfo()["hash_serialized_2"])
        self.log.info("  nodes agree %s (height %d)", where, self.nodes[0].getblockcount())

    def run_test(self):
        self.log.info("Both nodes start from the same genesis")
        self.assert_nodes_agree("at genesis")

        self.log.info("Ordinary blocks are accepted identically")
        for _ in range(3):
            self.submit_to_both(self.build_block(self.nodes[0]), "an ordinary block")
        self.assert_nodes_agree("after ordinary blocks")

        self.log.info("A coinbase with an unrelated OP_RETURN is accepted identically")
        block = self.build_block(self.nodes[0], extra_output_script=UNRELATED_OP_RETURN)
        # The coinbase carries both the witness commitment and the unrelated
        # OP_RETURN, which is the case a commitment-aware build would have to
        # disambiguate. This build has nothing to disambiguate.
        assert len(block.vtx[0].vout) >= 3
        self.submit_to_both(block, "a coinbase with an extra OP_RETURN")
        self.assert_nodes_agree("after the OP_RETURN coinbase")

        self.log.info("A header with an unusual nVersion is accepted identically")
        block = self.build_block(self.nodes[0], version=UNUSUAL_BLOCK_VERSION)
        assert_equal(block.nVersion, UNUSUAL_BLOCK_VERSION)
        self.submit_to_both(block, "a block with an unusual nVersion")
        self.assert_nodes_agree("after the unusual-version block")
        assert_equal(self.nodes[0].getblock(self.nodes[0].getbestblockhash())["version"],
                     UNUSUAL_BLOCK_VERSION)

        self.log.info("Both shapes together, still identical")
        block = self.build_block(self.nodes[0],
                                 extra_output_script=UNRELATED_OP_RETURN,
                                 version=UNUSUAL_BLOCK_VERSION)
        self.submit_to_both(block, "an OP_RETURN coinbase with an unusual nVersion")
        self.assert_nodes_agree("after the combined block")

        self.log.info("Fill up to the last block below the boundary")
        while self.nodes[0].getblockcount() < TERMINAL_HEIGHT - 1:
            self.submit_to_both(self.build_block(self.nodes[0]), "an ordinary block")
        assert_equal(self.nodes[0].getblockcount(), TERMINAL_HEIGHT - 1)

        self.log.info("At H-1 the two builds are still in exact agreement")
        self.assert_nodes_agree("at the last block below the boundary")


if __name__ == '__main__':
    TerminalNeutralityTest().main()
