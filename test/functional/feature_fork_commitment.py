#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the height-840,000 fork coinbase commitment (Scenario 1: one node).

From FORK_H1_HEIGHT onward, exactly one zero-value coinbase output must carry
a 30-byte `OP_RETURN <28-byte RINF payload>` script identifying this branch
(rincoin-consensus840k/technology/consensus-transition.md §5). This test
covers the acceptance/rejection matrix described in the S1 implementation
plan: missing / duplicate / malformed / wrong-field commitments, placement
independence, the "underclaiming is always valid" invariant, and GBT
integration.

Pre-fork blocks (built by the node's own miner, which doesn't yet know about
the commitment) are used to reach FORK_H1_HEIGHT - 1; hand-built blocks via
test_framework.fork_util are used at and after it, since only hand-built
blocks let us control the commitment byte-for-byte.
"""

from test_framework.fork_scenario import (
    FORK_H1_HEIGHT,
    FORK_H1_EXTRA_ARG,
    FORK_BRANCH_ID,
    FORK_NO,
    FORK_SCENARIO_ID,
    build_fork_commitment_payload,
)
from test_framework.fork_util import (
    build_fork_test_block,
    get_tip_template,
    mine_to_height,
    submit_and_check,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

# This branch's own pre-migration ad-hoc branch_id -- guaranteed different
# from the current FORK_BRANCH_ID (the Rev 4.0 design doc's canonical
# synthetic test vector, which FORK_BRANCH_ID was migrated to), and, like
# FORK_BRANCH_ID, explicitly never a mainnet value either way.
WRONG_BRANCH_ID = bytes.fromhex("6f2908c82838dab02cae3b9e527a600c")


class ForkCommitmentTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[FORK_H1_EXTRA_ARG]]

    def run_test(self):
        node = self.nodes[0]

        self.log.info(f"Mining to just before H1 ({FORK_H1_HEIGHT - 1}) with ordinary (no-commitment) blocks")
        mine_to_height(node, FORK_H1_HEIGHT - 1)
        assert_equal(node.getblockcount(), FORK_H1_HEIGHT - 1)

        self.log.info("Missing commitment at H1 is rejected")
        block = build_fork_test_block(node, omit_commitment=True)
        submit_and_check(node, block, expect_accept=False, reject_reason="bad-fork-commitment-missing")

        self.log.info("Wrong branch_id at H1 is rejected")
        block = build_fork_test_block(node, extra_commitment_kwargs={"branch_id": WRONG_BRANCH_ID})
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Wrong fork_no at H1 is rejected")
        block = build_fork_test_block(node, extra_commitment_kwargs={"fork_no": FORK_NO + 1})
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Wrong scenario_id at H1 is rejected")
        block = build_fork_test_block(node, extra_commitment_kwargs={"scenario_id": FORK_SCENARIO_ID + 1})
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Wrong format_version at H1 is rejected")
        block = build_fork_test_block(node, extra_commitment_kwargs={"format_version": 2})
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Nonzero flags byte at H1 is rejected")
        block = build_fork_test_block(node, extra_commitment_kwargs={"flags": 1})
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Truncated payload (27 bytes) at H1 is rejected")
        truncated = build_fork_commitment_payload()[:-1]
        block = build_fork_test_block(node, raw_commitment_payload=truncated)
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Extended payload (29 bytes) at H1 is rejected")
        extended = build_fork_commitment_payload() + b"\x00"
        block = build_fork_test_block(node, raw_commitment_payload=extended)
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Nonzero commitment output value at H1 is rejected")
        block = build_fork_test_block(node, commitment_value=1)
        submit_and_check(node, block, expect_accept=False)

        self.log.info("Two correct commitment outputs at H1 is rejected (duplicate)")
        block = build_fork_test_block(node, duplicate_commitment=True)
        submit_and_check(node, block, expect_accept=False, reject_reason="bad-fork-commitment-duplicate")

        self.log.info("One correct + one wrong-scenario commitment at H1 is rejected "
                       "(a correct output does not neutralize an incorrect duplicate)")
        block = build_fork_test_block(
            node, duplicate_commitment=True,
            extra_commitment_kwargs={},
            second_commitment_kwargs={"scenario_id": FORK_SCENARIO_ID + 1},
        )
        submit_and_check(node, block, expect_accept=False, reject_reason="bad-fork-commitment-duplicate")

        self.log.info("Correct commitment as the first output is accepted (placement-independent)")
        first_block = build_fork_test_block(node, commitment_index="first")
        submit_and_check(node, first_block, expect_accept=True)

        self.log.info("Rolling back to re-test the same height with the commitment as the last output")
        node.invalidateblock(first_block.hash)
        assert_equal(node.getblockcount(), FORK_H1_HEIGHT - 1)

        self.log.info("Correct commitment at H1 (as last output) is accepted, chain advances")
        block = build_fork_test_block(node, commitment_index="last")
        submit_and_check(node, block, expect_accept=True)
        assert_equal(node.getblockcount(), FORK_H1_HEIGHT)

        self.log.info("getblocktemplate advertises the ready-made fork-commitment fields at H1+1")
        tmpl = get_tip_template(node)
        assert_equal(bytes.fromhex(tmpl["fork_branch_id"]), FORK_BRANCH_ID)
        assert_equal(tmpl["fork_no"], FORK_NO)
        assert_equal(tmpl["fork_scenario_id"], FORK_SCENARIO_ID)
        assert "default_fork_commitment" in tmpl
        expected_script = build_fork_commitment_payload()
        assert bytes.fromhex(tmpl["default_fork_commitment"]).endswith(expected_script)

        self.log.info("Zero-value coinbase claim with a correct commitment is still valid post-H1")
        block = build_fork_test_block(node)
        block.vtx[0].vout[0].nValue = 0
        block.vtx[0].rehash()
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        submit_and_check(node, block, expect_accept=True)


if __name__ == '__main__':
    ForkCommitmentTest().main()
