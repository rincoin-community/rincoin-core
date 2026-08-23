#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the S1 post-840,000 subsidy schedule (Scenario 1: one node).

S1: recursive integer-floor x19/20 per 210,000-block epoch from H1 onward,
no floor, no tail (rincoin-consensus840k/technology/consensus-transition.md
§3). ConnectBlock's coinbase rule stays a ceiling, not an exact amount
(underclaiming, including to zero, is always valid) -- only the ceiling
value itself changes at and after H1.

On regtest, nSubsidyHalvingInterval = 150 (both pre- and post-fork use the
same consensus constant), so this test overrides FORK_H1_HEIGHT/interval
locally rather than mining to a mainnet-scale height.
"""

from test_framework.fork_scenario import FORK_H1_EXTRA_ARG, expected_subsidy
from test_framework.fork_util import build_fork_test_block, mine_to_height, submit_and_check
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

REGTEST_HALVING_INTERVAL = 150
H1 = 200  # passed explicitly via FORK_H1_EXTRA_ARG -- see fork_scenario.py


class ForkSubsidyTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[FORK_H1_EXTRA_ARG]]

    def subsidy_at(self, height):
        return expected_subsidy(height, h1_height=H1, halving_interval=REGTEST_HALVING_INTERVAL,
                                 base_reward=50 * 10**8)

    def try_claim(self, node, value, *, expect_accept):
        """Submit a block claiming `value`. Returns the block, so an accepted
        claim's caller can invalidateblock() it to retry the same height."""
        block = build_fork_test_block(node)
        block.vtx[0].vout[0].nValue = value
        block.vtx[0].rehash()
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        submit_and_check(node, block, expect_accept=expect_accept,
                          reject_reason=None if expect_accept else "bad-cb-amount")
        return block

    def run_test(self):
        node = self.nodes[0]
        mine_to_height(node, H1 - 1)

        ceiling_h1 = self.subsidy_at(H1)
        self.log.info(f"H1 ceiling = {ceiling_h1}")

        self.log.info("Claiming exactly the H1 ceiling is accepted")
        accepted = self.try_claim(node, ceiling_h1, expect_accept=True)

        self.log.info("Rolling back to retest the same height: H1 ceiling + 1 is rejected")
        node.invalidateblock(accepted.hash)
        assert_equal(node.getblockcount(), H1 - 1)
        self.try_claim(node, ceiling_h1 + 1, expect_accept=False)

        self.log.info("Claiming zero (always valid, even post-fork) is accepted")
        self.try_claim(node, 0, expect_accept=True)
        assert_equal(node.getblockcount(), H1)

        self.log.info("Mining to the next post-fork epoch boundary (H1 + halving_interval)")
        next_epoch_height = H1 + REGTEST_HALVING_INTERVAL
        mine_to_height(node, next_epoch_height - 1)
        ceiling_next = self.subsidy_at(next_epoch_height)
        self.log.info(f"H1+{REGTEST_HALVING_INTERVAL} ceiling = {ceiling_next}")
        assert ceiling_next < ceiling_h1, "subsidy must strictly decrease across an epoch boundary"

        self.log.info(f"Claiming H1+{REGTEST_HALVING_INTERVAL} ceiling is accepted")
        accepted = self.try_claim(node, ceiling_next, expect_accept=True)

        self.log.info(f"Rolling back to retest: H1+{REGTEST_HALVING_INTERVAL} ceiling + 1 is rejected")
        node.invalidateblock(accepted.hash)
        assert_equal(node.getblockcount(), next_epoch_height - 1)
        self.try_claim(node, ceiling_next + 1, expect_accept=False)


if __name__ == '__main__':
    ForkSubsidyTest().main()
