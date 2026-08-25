// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chainparams.h>
#include <test/util/setup_common.h>
#include <validation.h>

#include <chrono>
#include <limits>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(fork_subsidy_tests, BasicTestingSetup)

// S6/b scenario (rincoin-consensus840k/analysis/
// Rincoin_840k_S6B_Consensus_Change_Specification.qmd): four fixed-value
// phases (4 / 2 / 1 / 0.6 RIN) followed by a hard cutoff to zero at a
// terminal height derived from an exact 168,000,000 RIN issuance ceiling.
// Frozen vectors copied verbatim from
// analysis/data/S6B_normative_test_vectors.csv (the file the specification
// itself references as its own generated vector set), not derived from
// this implementation.
BOOST_AUTO_TEST_CASE(s6b_subsidy_frozen_vectors)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();
    BOOST_CHECK_EQUAL(params.ForkH1Height, 840000);

    // S6B-001/002/003: activation block and its immediate neighbours.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(839999, params), CAmount{625000000}); // pre-fork, unchanged
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000, params), CAmount{400000000}); // phase 1: 4 RIN
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840001, params), CAmount{400000000});

    // S6B-004/005: first phase boundary.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2099999, params), CAmount{400000000}); // final 4 RIN block
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2100000, params), CAmount{200000000}); // first 2 RIN block

    // S6B-007/008: second phase boundary.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(4199999, params), CAmount{200000000}); // final 2 RIN block
    BOOST_CHECK_EQUAL(GetBlockSubsidy(4200000, params), CAmount{100000000}); // first 1 RIN block

    // S6B-010/011: third phase boundary.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(6299999, params), CAmount{100000000}); // final 1 RIN block
    BOOST_CHECK_EQUAL(GetBlockSubsidy(6300000, params), CAmount{60000000});  // first 0.6 RIN block

    // S6B-014/015/016: terminal region -- the derived cutoff height itself,
    // not a designed round number. Getting this wrong (even by one block)
    // would either shortchange or overpay the entire 168,000,000 RIN
    // ceiling, so this is the single most important pair to get exactly
    // right for this scenario.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(234587499, params), CAmount{60000000}); // final non-zero block
    BOOST_CHECK_EQUAL(GetBlockSubsidy(234587500, params), CAmount{0});        // first zero-subsidy block
    BOOST_CHECK_EQUAL(GetBlockSubsidy(234587501, params), CAmount{0});
}

BOOST_AUTO_TEST_CASE(s6b_subsidy_pre_fork_unchanged)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    // Ordinary geometric halving below ForkH1Height, untouched by S6/b.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, params), CAmount{25 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, params), CAmount{1250000000LL});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(630000, params), CAmount{625000000LL});
}

// The ceiling must strictly decrease across every phase boundary, and the
// table's last phase must be exactly zero -- an off-by-one in the phase
// scan, or a table missing its terminal entry, would either reopen an
// earlier phase's ceiling past its boundary or (worse) never cut off,
// making issuance unbounded.
BOOST_AUTO_TEST_CASE(s6b_subsidy_strictly_decreasing_then_terminates)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    BOOST_REQUIRE(!params.ForkSubsidyPhases.empty());
    BOOST_CHECK_EQUAL(params.ForkSubsidyPhases.back().nSubsidy, CAmount{0});

    CAmount prev = std::numeric_limits<CAmount>::max();
    for (const auto& phase : params.ForkSubsidyPhases) {
        const int height = params.ForkH1Height + phase.nOffsetFromH1;
        CAmount cur = GetBlockSubsidy(height, params);
        BOOST_CHECK_EQUAL(cur, phase.nSubsidy);
        BOOST_CHECK_LT(cur, prev);
        prev = cur;
    }
    BOOST_CHECK_EQUAL(prev, CAmount{0});
}

// Regression guard: GetBlockSubsidyPostFork() must be O(1) in practice for
// any height, not O(height) -- inherited from S1's implementation, whose
// own version of this test caught a real unbounded-loop DoS-relevant bug
// during development. S6/b's implementation scans a small, fixed-size
// table (never loops on the height itself), so this guards against a
// future regression toward a height-dependent loop, not a repeat finding.
BOOST_AUTO_TEST_CASE(s6b_subsidy_extreme_height_is_fast_and_zero)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    const auto start = std::chrono::steady_clock::now();
    CAmount subsidy = GetBlockSubsidy(std::numeric_limits<int>::max(), params);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK_EQUAL(subsidy, CAmount{0}); // past the terminal cutoff
    BOOST_CHECK(elapsed < std::chrono::seconds(1));
}

BOOST_AUTO_TEST_SUITE_END()
