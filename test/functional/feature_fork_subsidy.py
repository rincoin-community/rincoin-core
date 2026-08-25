#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the S5/b post-840,000 subsidy schedule (Scenario 1: one node).

S5/b: extended, 10x-longer halving epoch phase-anchored to H1 - interval,
then plain binary halving per extended epoch, no floor, no tail
(rincoin-consensus840k/analysis/Rincoin_840k_S5B_Consensus_Change_Specification.qmd).
ConnectBlock's coinbase rule stays a ceiling, not an exact amount
(underclaiming, including to zero, is always valid) -- only the ceiling
value itself changes at and after H1.

On regtest, nSubsidyHalvingInterval = 150 (both pre- and post-fork use the
same consensus constant), so this test overrides FORK_H1_HEIGHT/interval
locally rather than mining to a mainnet-scale height. The first actual
subsidy *change* lands FORK_SUBSIDY_NEXT_CHANGE_EPOCHS halving-intervals
past H1, not one interval past H1 as it would for a scenario whose
post-activation epoch length equals nSubsidyHalvingInterval -- see that
constant's definition in fork_scenario.py for why.

For S5/b specifically, that offset (9 * 150 = 1,350 blocks past H1) is
past FIRST_MWEB_HEIGHT (432): the chain has MWEB active by the second
boundary, but build_fork_test_block()'s hand-built blocks don't construct a
valid MWEB extension block/HogEx transaction (real chain-state-dependent
construction, unrelated to what this test exercises -- see
test_framework.rin_util.create_hogex for why that's nontrivial). Rather
than hand-build past MWEB activation, the second boundary check falls back
to a real-miner consistency check (mine one real block, inspect its actual
coinbase value) whenever the target height is at or past FIRST_MWEB_HEIGHT;
below that height (true for S1's much shorter epoch), it keeps using the
stronger hand-built overclaim-rejection check.
"""

from test_framework.fork_scenario import (
    FORK_H1_EXTRA_ARG,
    FORK_SUBSIDY_NEXT_CHANGE_EPOCHS,
    expected_subsidy,
)
from test_framework.fork_util import build_fork_test_block, mine_to_height, submit_and_check
from test_framework.messages import COIN
from test_framework.rin_util import FIRST_MWEB_HEIGHT
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

        next_change_offset = FORK_SUBSIDY_NEXT_CHANGE_EPOCHS * REGTEST_HALVING_INTERVAL
        self.log.info(f"Mining to the next post-fork subsidy change (H1 + {next_change_offset})")
        next_epoch_height = H1 + next_change_offset
        ceiling_next = self.subsidy_at(next_epoch_height)
        self.log.info(f"H1+{next_change_offset} ceiling = {ceiling_next}")
        assert ceiling_next < ceiling_h1, "subsidy must strictly decrease across an epoch boundary"

        if next_epoch_height < FIRST_MWEB_HEIGHT:
            mine_to_height(node, next_epoch_height - 1)
            self.log.info(f"Claiming H1+{next_change_offset} ceiling is accepted")
            accepted = self.try_claim(node, ceiling_next, expect_accept=True)

            self.log.info(f"Rolling back to retest: H1+{next_change_offset} ceiling + 1 is rejected")
            node.invalidateblock(accepted.hash)
            assert_equal(node.getblockcount(), next_epoch_height - 1)
            self.try_claim(node, ceiling_next + 1, expect_accept=False)
        else:
            # Past MWEB activation: hand-built blocks can't easily satisfy
            # the extension-block/HogEx requirement (see module docstring),
            # so confirm the real miner's own block template enforces the
            # new ceiling instead of hand-building an overclaim to reject.
            self.log.info(f"H1+{next_change_offset} is past FIRST_MWEB_HEIGHT ({FIRST_MWEB_HEIGHT}); "
                           "checking the real miner's own coinbase instead of a hand-built overclaim")
            mine_to_height(node, next_epoch_height)
            tip_hash = node.getbestblockhash()
            coinbase_value = node.getblock(tip_hash, 2)["tx"][0]["vout"][0]["value"]
            assert_equal(round(coinbase_value * COIN), ceiling_next)


if __name__ == '__main__':
    ForkSubsidyTest().main()
