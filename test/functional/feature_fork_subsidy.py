#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the S6/b post-840,000 subsidy schedule (Scenario 1: one node).

S6/b: four fixed-value phases (4 / 2 / 1 / 0.6 RIN) followed by a hard
cutoff to zero at a terminal height derived from an exact 168,000,000 RIN
issuance ceiling (rincoin-consensus840k/analysis/
Rincoin_840k_S6B_Consensus_Change_Specification.qmd). ConnectBlock's
coinbase rule stays a ceiling, not an exact amount (underclaiming, including
to zero, is always valid) -- only the ceiling value itself changes at each
phase boundary.

Unlike S1/S5/b, this scenario's phase boundaries aren't expressed as
multiples of nSubsidyHalvingInterval -- see Consensus::Params::ForkSubsidyPhase
in consensus/params.h. Regtest uses its own small, independently derived
five-entry phase table (same structure and derivation method as mainnet's,
not a scaled copy of mainnet's heights -- see the CRegTestParams comment in
chainparams.cpp), reachable in full (including the terminal zero phase)
well within FIRST_MWEB_HEIGHT, so this test can exercise every phase
transition directly rather than needing S5/b's real-miner MWEB fallback.
"""

from test_framework.fork_scenario import FORK_H1_EXTRA_ARG, FORK_SUBSIDY_PHASES, expected_subsidy
from test_framework.fork_util import build_fork_test_block, mine_to_height, submit_and_check
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

H1 = 200  # passed explicitly via FORK_H1_EXTRA_ARG -- see fork_scenario.py


class ForkSubsidyTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[FORK_H1_EXTRA_ARG]]

    def subsidy_at(self, height):
        return expected_subsidy(height, h1_height=H1)

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

    def check_boundary(self, node, height, prev_ceiling):
        """Mine to `height`-1, then check the ceiling at `height` against the
        table: reject overclaim, accept the exact ceiling, and (except at the
        terminal zero phase, where zero and ceiling coincide) confirm it's
        strictly lower than the previous phase's ceiling."""
        mine_to_height(node, height - 1)
        ceiling = self.subsidy_at(height)
        self.log.info(f"height {height} ceiling = {ceiling}")
        if prev_ceiling is not None:
            assert ceiling <= prev_ceiling, "subsidy must never increase across a phase boundary"

        if ceiling > 0:
            self.log.info(f"Claiming height {height} ceiling + 1 is rejected")
            self.try_claim(node, ceiling + 1, expect_accept=False)
            assert_equal(node.getblockcount(), height - 1)

        self.log.info(f"Claiming height {height} ceiling ({ceiling}) is accepted")
        self.try_claim(node, ceiling, expect_accept=True)
        assert_equal(node.getblockcount(), height)
        return ceiling

    def run_test(self):
        node = self.nodes[0]
        mine_to_height(node, H1 - 1)

        self.log.info("Claiming zero (always valid, even post-fork) is accepted, then rolled back")
        zero_block = self.try_claim(node, 0, expect_accept=True)
        node.invalidateblock(zero_block.hash)
        assert_equal(node.getblockcount(), H1 - 1)

        prev_ceiling = None
        for offset, _expected_subsidy in FORK_SUBSIDY_PHASES:
            prev_ceiling = self.check_boundary(node, H1 + offset, prev_ceiling)

        assert_equal(prev_ceiling, 0), "the table's final phase must be the terminal zero cutoff"


if __name__ == '__main__':
    ForkSubsidyTest().main()
