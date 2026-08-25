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

// S5/b scenario (rincoin-consensus840k/analysis/
// Rincoin_840k_S5B_Consensus_Change_Specification.qmd): extended,
// 10x-longer halving epoch phase-anchored to height 630,000 (H1 -
// nSubsidyHalvingInterval), then plain binary halving per extended epoch,
// no floor, no tail. Frozen vectors copied verbatim from
// analysis/data/S5B_normative_test_vectors.csv (the file the specification
// itself names as the conformance target), not derived from this
// implementation.
BOOST_AUTO_TEST_CASE(s5b_subsidy_frozen_vectors)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();
    BOOST_CHECK_EQUAL(params.ForkH1Height, 840000);

    // S5B-001/002/003: activation block and its immediate neighbours --
    // phase 0, flat at the pre-fork base value.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(839999, params), CAmount{625000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000, params), CAmount{625000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840001, params), CAmount{625000000});

    // S5B-004/005: the entire first post-fork epoch is flat -- height
    // 840,000 is only one ordinary epoch past the anchor (630,000), and the
    // post-fork epoch is ten ordinary epochs long, so the first actual
    // halving doesn't land until 2,730,000, not at H1 itself. This is the
    // single most important thing to get right about S5/b's formula (it is
    // NOT "S1 with a different ratio") -- a formula that halved at H1
    // itself would fail this pair immediately.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2729999, params), CAmount{625000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2730000, params), CAmount{312500000});

    // S5B-011: third halving, further out -- guards against an off-by-one
    // in the phase-index arithmetic that a check confined to phase 0/1
    // wouldn't catch.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(6930000, params), CAmount{78125000});

    // S5B-034/035/036: terminal region -- final one-base-unit block, and
    // the first block where integer right-shift exhausts the value to
    // exactly zero.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(63629999, params), CAmount{1});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(63630000, params), CAmount{0});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(63630001, params), CAmount{0});
}

BOOST_AUTO_TEST_CASE(s5b_subsidy_pre_fork_unchanged)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    // Ordinary geometric halving below ForkH1Height, untouched by S5/b.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, params), CAmount{25 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, params), CAmount{1250000000LL});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(630000, params), CAmount{625000000LL});
}

// The ceiling must be flat within a post-fork epoch and strictly decrease
// across each subsequent (10x-length) epoch boundary -- an off-by-one here
// would silently reopen the pre-fork subsidy for post-fork heights, or
// (the S5/b-specific failure mode) decrease every ordinary epoch instead of
// every extended one.
BOOST_AUTO_TEST_CASE(s5b_subsidy_flat_within_epoch_then_strictly_decreasing)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    const int64_t postForkEpochLength = 10LL * params.nSubsidyHalvingInterval;
    const int64_t anchor = int64_t(params.ForkH1Height) - params.nSubsidyHalvingInterval;

    // Flat for the rest of phase 0 (H1 itself through the epoch's last block).
    CAmount atH1 = GetBlockSubsidy(params.ForkH1Height, params);
    CAmount atPhase0End = GetBlockSubsidy(int(anchor + postForkEpochLength - 1), params);
    BOOST_CHECK_EQUAL(atH1, atPhase0End);

    CAmount prev = atH1;
    for (int phase = 1; phase < 6; ++phase) {
        CAmount cur = GetBlockSubsidy(int(anchor + phase * postForkEpochLength), params);
        BOOST_CHECK_LT(cur, prev);
        prev = cur;
    }
}

// Regression guard: GetBlockSubsidyPostFork() must be O(1) in practice for
// any height, not O(nPhase) -- inherited from S1's implementation, whose
// own version of this test caught a real unbounded-loop DoS-relevant bug
// during development (it hung a full local test run for hours). S5/b's
// implementation never loops (bounded right-shift), so this is a guard
// against a future regression back toward a looping formula, not a repeat
// finding.
BOOST_AUTO_TEST_CASE(s5b_subsidy_extreme_height_is_fast_and_zero)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    const auto start = std::chrono::steady_clock::now();
    CAmount subsidy = GetBlockSubsidy(std::numeric_limits<int>::max(), params);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK_EQUAL(subsidy, CAmount{0}); // decayed to zero by any realistic height
    BOOST_CHECK(elapsed < std::chrono::seconds(1));
}

BOOST_AUTO_TEST_SUITE_END()
