// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <net.h>
#include <signet.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <vector>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

namespace {
// Single source of truth for the expected block-subsidy emission schedule,
// expressed as a table of epochs. Each epoch starts at `first_height` and pays
// `subsidy` per block until the following epoch begins. Keeping the expected
// values in one table lets the tests below validate GetBlockSubsidy() against an
// independent reference instead of re-deriving the formula inline.
struct SubsidyEpoch {
    int first_height;
    CAmount subsidy;
};

// Reference schedule for a given halving interval: the initial 50-coin subsidy
// is halved every `interval` blocks until it reaches zero.
std::vector<SubsidyEpoch> BuildReferenceSchedule(int interval)
{
    std::vector<SubsidyEpoch> schedule;
    CAmount subsidy = 50 * COIN;
    int epoch = 0;
    for (; subsidy > 0; ++epoch) {
        schedule.push_back({epoch * interval, subsidy});
        subsidy >>= 1;
    }
    // Tail epoch: the subsidy has floored to zero from here on.
    schedule.push_back({epoch * interval, 0});
    return schedule;
}

// Expected subsidy at `height`, looked up from the reference table.
CAmount ExpectedSubsidyAt(const std::vector<SubsidyEpoch>& schedule, int height)
{
    CAmount expected = 0;
    for (const SubsidyEpoch& epoch : schedule) {
        if (height < epoch.first_height) break;
        expected = epoch.subsidy;
    }
    return expected;
}

// Number of epoch boundaries to exercise. The current schedule floors to zero
// well before this, so the surplus simply confirms the reward stays at zero and
// leaves head-room for schedules with more epochs.
constexpr int NUM_BOUNDARIES_CHECKED = 100;

// Validate GetBlockSubsidy() against the reference table, with boundary-value
// analysis (last block of the previous epoch, first block of the next epoch,
// and the block right after) around every epoch transition.
void CheckSubsidySchedule(int interval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = interval;
    const std::vector<SubsidyEpoch> schedule = BuildReferenceSchedule(interval);

    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensusParams), 50 * COIN);

    for (int boundary = 1; boundary <= NUM_BOUNDARIES_CHECKED; ++boundary) {
        const int height = boundary * interval;
        for (const int height_at : {height - 1, height, height + 1}) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(height_at, consensusParams),
                              ExpectedSubsidyAt(schedule, height_at));
        }
    }

    // The reward is exactly zero once the right shift is undefined (>= 64).
    BOOST_CHECK_EQUAL(GetBlockSubsidy(64 * interval, consensusParams), 0);
}
} // namespace

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    CheckSubsidySchedule(chainParams->GetConsensus().nSubsidyHalvingInterval); // As in main
    CheckSubsidySchedule(210);  // As in regtest and preview
    CheckSubsidySchedule(2100); // As in testnet
    CheckSubsidySchedule(1000); // Just another interval
}

BOOST_AUTO_TEST_CASE(block_subsidy_monotonic_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensusParams = chainParams->GetConsensus();

    CAmount previous = GetBlockSubsidy(0, consensusParams);
    BOOST_CHECK_EQUAL(previous, 50 * COIN);
    for (int nHeight = 0; nHeight < 56000000; nHeight += 1000) {
        const CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        // Never negative, never above the initial subsidy, never increasing.
        BOOST_CHECK(nSubsidy >= 0);
        BOOST_CHECK(nSubsidy <= 50 * COIN);
        BOOST_CHECK(nSubsidy <= previous);
        previous = nSubsidy;
    }
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    // Rincoin: mainnet follows the S6/b schedule from height 840,000 (see
    // s6b_subsidy_tests.cpp for the schedule itself). This loop samples every
    // 1000th height below 56,000,000; every S6/b boundary is a multiple of 1000,
    // so the sampled sum is exact for that range:
    //   below 840,000:             (50 + 25 + 12.5 + 6.25) RIN x 210,000 = 19,687,500 RIN
    //   840,000 .. 2,099,999:      4 RIN x 1,260,000                     =  5,040,000 RIN
    //   2,100,000 .. 4,199,999:    2 RIN x 2,100,000                     =  4,200,000 RIN
    //   4,200,000 .. 6,299,999:    1 RIN x 2,100,000                     =  2,100,000 RIN
    //   6,300,000 .. 55,999,999:   0.6 RIN x 49,700,000                  = 29,820,000 RIN
    //   total                                                            = 60,847,500 RIN
    // (the historical pure-halving rule gave 2099999997690000 base units here).
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight < 56000000; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, chainParams->GetConsensus());
        BOOST_CHECK(nSubsidy <= 50 * COIN);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, CAmount{6084750000000000});
}

BOOST_AUTO_TEST_CASE(block_subsidy_mainnet_spot_check)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& c = chainParams->GetConsensus();

    // Subsidies observed on the live main network via getblockstats, including
    // the exact halving boundaries; a real-world anchor for GetBlockSubsidy().
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1,      c), CAmount{5000000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(100000, c), CAmount{5000000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(209999, c), CAmount{5000000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(210000, c), CAmount{2500000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(419999, c), CAmount{2500000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000, c), CAmount{1250000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(629999, c), CAmount{1250000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(630000, c), CAmount{625000000});
    BOOST_CHECK_EQUAL(GetBlockSubsidy(672000, c), CAmount{625000000});
}

BOOST_AUTO_TEST_SUITE_END()
