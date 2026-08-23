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

// S1 scenario (rincoin-consensus840k/technology/consensus-transition.md §3):
// recursive integer-floor x19/20 per 210,000-block epoch from height
// 840,000 onward, no floor, no tail. Frozen vectors cross-checked directly
// against the authoritative source (Open Risk R2, now closed): the
// rincoin-whitepaper repo's data/boundary_vectors.csv at commit
// 9533f6b33d53fd78affc092ac04c04dcfb8c2797 (the same commit
// tmp/fork-design-audit's test plan cites for these rows), rows 114-116 and
// 141-142 for scenario S1. Every value below is copied verbatim from that
// file, not derived from this implementation.
BOOST_AUTO_TEST_CASE(s1_subsidy_frozen_vectors)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();
    BOOST_CHECK_EQUAL(params.ForkH1Height, 840000);

    BOOST_CHECK_EQUAL(GetBlockSubsidy(839999, params), CAmount{625000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000, params), CAmount{593750000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840001, params), CAmount{593750000});
    // Second post-fork epoch boundary, still within the first csv block.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000 + 210000, params), CAmount{(593750000LL * 19) / 20});

    // Multi-epoch boundary pair (9 vs. 10 recursive x19/20 steps applied) --
    // a much stronger check of the recursive formula than the H1-adjacent
    // values above, since an off-by-one in the epoch count would only ever
    // show up this far out.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2729999, params), CAmount{393905878});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(2730000, params), CAmount{374210584});
}

BOOST_AUTO_TEST_CASE(s1_subsidy_pre_fork_unchanged)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    // Ordinary geometric halving below ForkH1Height, untouched by S1.
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, params), CAmount{50 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, params), CAmount{25 * COIN});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, params), CAmount{1250000000LL});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(630000, params), CAmount{625000000LL});
}

// The ceiling must strictly decrease across the H1 epoch boundary and every
// subsequent epoch boundary -- an off-by-one here would silently reopen the
// pre-fork subsidy for post-fork heights.
BOOST_AUTO_TEST_CASE(s1_subsidy_strictly_decreasing_across_epochs)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CAmount prev = GetBlockSubsidy(params.ForkH1Height - 1, params);
    for (int epoch = 0; epoch < 5; ++epoch) {
        CAmount cur = GetBlockSubsidy(params.ForkH1Height + epoch * 210000, params);
        BOOST_CHECK_LT(cur, prev);
        prev = cur;
    }
}

// Regression guard: GetBlockSubsidyPostFork() must be O(1) in practice for
// any height, not O(nEpoch) -- an unbounded per-epoch loop previously made
// this function take unbounded time for very large heights (near INT_MAX),
// which is a real DoS-relevant concern for a function on the block
// validation path, not just a theoretical one (it hung a full local test
// run for hours before being caught here).
BOOST_AUTO_TEST_CASE(s1_subsidy_extreme_height_is_fast_and_zero)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    const auto start = std::chrono::steady_clock::now();
    CAmount subsidy = GetBlockSubsidy(std::numeric_limits<int>::max(), params);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    BOOST_CHECK_EQUAL(subsidy, CAmount{0}); // long decayed to zero by any realistic height
    BOOST_CHECK(elapsed < std::chrono::seconds(1));
}

BOOST_AUTO_TEST_SUITE_END()
