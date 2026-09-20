// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Height-840,000 transition: the S6/b subsidy schedule and the coinbase rule of
// the transition block.
//
// Expected values are taken from the frozen normative vectors of the S6/b
// specification (consensus-840k repository,
// analysis/data/S6B_normative_test_vectors.csv, SHA-256
// 2802045fde1e8ef0cd061a7e01dded6488fca5f2ca9c5573ca9d46dfeadefb70), and from
// arithmetic spelled out in this file -- never from GetBlockSubsidy() itself.

#include <amount.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <miner.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <limits>
#include <vector>

namespace {

struct NormativeVector {
    int height;
    CAmount subsidy;
    CAmount issued_before; // cumulative maximum scheduled issuance before this block
};

// S6B-001 .. S6B-016, copied verbatim from the frozen CSV.
const std::vector<NormativeVector> NORMATIVE_VECTORS = {
    {839999, 625000000, 1968749375000000LL},
    {840000, 400000000, 1968750000000000LL},
    {840001, 400000000, 1968750400000000LL},
    {2099999, 400000000, 2472749600000000LL},
    {2100000, 200000000, 2472750000000000LL},
    {2100001, 200000000, 2472750200000000LL},
    {4199999, 200000000, 2892749800000000LL},
    {4200000, 100000000, 2892750000000000LL},
    {4200001, 100000000, 2892750100000000LL},
    {6299999, 100000000, 3102749900000000LL},
    {6300000, 60000000, 3102750000000000LL},
    {6300001, 60000000, 3102750060000000LL},
    {234587498, 60000000, 16799999880000000LL},
    {234587499, 60000000, 16799999940000000LL},
    {234587500, 0, 16800000000000000LL},
    {234587501, 0, 16800000000000000LL},
};

/** The historical rule, written out independently of validation.cpp. */
CAmount LegacySubsidy(int height, int interval)
{
    const int halvings = height / interval;
    if (halvings >= 64) return 0;
    return (50 * COIN) >> halvings;
}

/** Piecewise-constant segments of the mainnet schedule as the specification states them. */
struct Segment {
    int64_t start; // inclusive
    int64_t end;   // exclusive
    CAmount subsidy;
};
const std::vector<Segment> MAINNET_SEGMENTS = {
    {0, 210000, 5000000000LL},
    {210000, 420000, 2500000000LL},
    {420000, 630000, 1250000000LL},
    {630000, 840000, 625000000LL},
    {840000, 2100000, 400000000LL},
    {2100000, 4200000, 200000000LL},
    {4200000, 6300000, 100000000LL},
    {6300000, 234587500, 60000000LL},
};

CAmount IssuedBefore(int64_t height)
{
    CAmount total = 0;
    for (const Segment& s : MAINNET_SEGMENTS) {
        const int64_t upto = std::min<int64_t>(height, s.end);
        if (upto > s.start) total += (upto - s.start) * s.subsidy;
    }
    return total;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(s6b_subsidy_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(normative_vectors)
{
    const auto params = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = params->GetConsensus();
    BOOST_CHECK_EQUAL(consensus.nS6bHeight, 840000);

    for (const NormativeVector& v : NORMATIVE_VECTORS) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(v.height, consensus), v.subsidy);
        // The test's own segment arithmetic must agree with the frozen cumulative values...
        BOOST_CHECK_EQUAL(IssuedBefore(v.height), v.issued_before);
    }
}

BOOST_AUTO_TEST_CASE(ceiling_is_exactly_168_million_rin)
{
    // Integer arithmetic only: 168,000,000 RIN = 16,800,000,000,000,000 base units.
    const CAmount ceiling = 16800000000000000LL;
    BOOST_CHECK_EQUAL(ceiling, 168000000 * COIN);
    BOOST_CHECK_EQUAL(MAX_MONEY, ceiling);
    BOOST_CHECK_EQUAL(IssuedBefore(234587500), ceiling);
    BOOST_CHECK_EQUAL(IssuedBefore(std::numeric_limits<int>::max()), ceiling);
    // One block earlier the ceiling is not reached yet; the last paying block is 234,587,499.
    BOOST_CHECK_EQUAL(IssuedBefore(234587499), ceiling - 60000000);
}

BOOST_AUTO_TEST_CASE(schedule_matches_the_segments_everywhere_sampled)
{
    const auto params = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = params->GetConsensus();

    for (const Segment& s : MAINNET_SEGMENTS) {
        // Both edges, their neighbors, and a coarse prime-stride sample inside.
        for (int64_t h : {s.start, s.start + 1, s.end - 2, s.end - 1}) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(static_cast<int>(h), consensus), s.subsidy);
        }
        const int64_t stride = std::max<int64_t>(1, (s.end - s.start) / 997);
        for (int64_t h = s.start; h < s.end; h += stride) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(static_cast<int>(h), consensus), s.subsidy);
        }
    }
    // Zero forever after the terminal height, and the lookup does not depend on the height.
    for (int h : {234587500, 234587501, 300000000, 1000000000, std::numeric_limits<int>::max()}) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, consensus), 0);
    }
}

BOOST_AUTO_TEST_CASE(history_below_the_transition_is_unchanged)
{
    const auto params = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = params->GetConsensus();
    // Every height in a window around each historical halving, plus a stride over the rest.
    for (int boundary : {0, 210000, 420000, 630000, 840000}) {
        for (int h = std::max(0, boundary - 300); h < std::min(840000, boundary + 300); h++) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, consensus), LegacySubsidy(h, 210000));
        }
    }
    for (int h = 0; h < 840000; h += 1013) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, consensus), LegacySubsidy(h, 210000));
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(839999, consensus), 625000000);
    // The historical rule would have paid 3.125 RIN at 840,000; the new rule pays 4 RIN.
    BOOST_CHECK_EQUAL(LegacySubsidy(840000, 210000), 312500000);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000, consensus), 400000000);
}

// Test networks use the same multiples of their own halving interval.
BOOST_AUTO_TEST_CASE(scaled_networks)
{
    struct Expect {
        std::string chain;
        int interval;
        int terminal;
    };
    // Terminal height: 30 intervals plus (scaled ceiling - issuance before 30 intervals) / 0.6 RIN,
    // rounded down. Interval 210: ceiling 168,000 RIN, issued 31,027.5 RIN, remainder
    // 136,972.5 / 0.6 = 228,287.5 -> 6,300 + 228,287. Interval 2,100: exact, 63,000 + 2,282,875.
    for (const Expect& e : {Expect{CBaseChainParams::REGTEST, 210, 234587}, Expect{CBaseChainParams::PREVIEW, 210, 234587}, Expect{CBaseChainParams::TESTNET, 2100, 2345875}}) {
        const auto params = CreateChainParams(*m_node.args, e.chain);
        const Consensus::Params& c = params->GetConsensus();
        BOOST_CHECK_EQUAL(c.nSubsidyHalvingInterval, e.interval);
        BOOST_CHECK_EQUAL(c.nS6bHeight, 4 * e.interval);

        BOOST_CHECK_EQUAL(GetBlockSubsidy(4 * e.interval - 1, c), 625000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(4 * e.interval, c), 400000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(10 * e.interval - 1, c), 400000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(10 * e.interval, c), 200000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(20 * e.interval - 1, c), 200000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(20 * e.interval, c), 100000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(30 * e.interval - 1, c), 100000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(30 * e.interval, c), 60000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(e.terminal - 1, c), 60000000);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(e.terminal, c), 0);
        for (int h = 0; h < 4 * e.interval; h++) {
            BOOST_CHECK_EQUAL(GetBlockSubsidy(h, c), LegacySubsidy(h, e.interval));
        }

        // Total issuance never exceeds the scaled ceiling (800 RIN per block of interval).
        CAmount total = 0;
        for (int k = 0; k < 4; k++) total += ((50 * COIN) >> k) * e.interval;
        total += (4 * COIN) * 6 * e.interval + (2 * COIN) * 10 * e.interval + (1 * COIN) * 10 * e.interval;
        total += CAmount(60000000) * (e.terminal - 30 * e.interval);
        const CAmount ceiling = 800 * COIN * e.interval;
        BOOST_CHECK(total <= ceiling);
        BOOST_CHECK(ceiling - total < 60000000); // less than one more block of subsidy is left
    }
    // A network without the transition keeps the historical rule (default-constructed params).
    Consensus::Params plain;
    plain.nSubsidyHalvingInterval = 150;
    BOOST_CHECK_EQUAL(GetBlockSubsidy(600, plain), LegacySubsidy(600, 150));
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------------------------
// The transition block's coinbase rule, through real block connection on regtest (height 840).
// ---------------------------------------------------------------------------------------------

namespace {

struct S6bTransitionSetup : public TestChain100Setup {
    CScript script_pubkey;

    S6bTransitionSetup()
    {
        script_pubkey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    }

    void MineTo(int height)
    {
        while (::ChainActive().Height() < height) {
            CreateAndProcessBlock({}, script_pubkey);
        }
    }

    /** Build the next block from a template and set its coinbase output values. */
    CBlock BuildNext(const std::vector<CAmount>& coinbase_values)
    {
        const CChainParams& chainparams = Params();
        CBlock block = BlockAssembler(*m_node.mempool, chainparams).CreateNewBlock(script_pubkey)->block;
        CMutableTransaction coinbase(*block.vtx[0]);
        // Keep any commitment outputs the template added; replace the paying output(s).
        std::vector<CTxOut> others(coinbase.vout.begin() + 1, coinbase.vout.end());
        coinbase.vout.clear();
        for (CAmount v : coinbase_values) coinbase.vout.emplace_back(v, script_pubkey);
        coinbase.vout.insert(coinbase.vout.end(), others.begin(), others.end());
        block.vtx[0] = MakeTransactionRef(std::move(coinbase));
        block.hashMerkleRoot = BlockMerkleRoot(block);
        while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;
        return block;
    }

    std::string RejectReason(const CBlock& block)
    {
        BlockValidationState state;
        LOCK(cs_main);
        if (TestBlockValidity(state, Params(), block, ::ChainActive().Tip(), /* fCheckPOW */ true, /* fCheckMerkleRoot */ true)) return "";
        return state.GetRejectReason();
    }

    bool Connect(const CBlock& block)
    {
        const auto shared = std::make_shared<const CBlock>(block);
        Assert(m_node.chainman)->ProcessNewBlock(Params(), shared, true, nullptr);
        return WITH_LOCK(cs_main, return ::ChainActive().Tip()->GetBlockHash()) == block.GetHash();
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(s6b_transition_block_tests, S6bTransitionSetup)

BOOST_AUTO_TEST_CASE(exact_claim_only_in_the_transition_block)
{
    const int H = Params().GetConsensus().nS6bHeight;
    BOOST_REQUIRE_EQUAL(H, 840);
    const CAmount old_subsidy = 625000000; // 6.25 RIN below the transition on regtest (interval 210)
    const CAmount new_subsidy = 400000000;

    // Two blocks before the transition: underclaiming is valid as ever.
    MineTo(H - 3);
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({old_subsidy - 1})), "");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({0})), "");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({old_subsidy + 1})), "bad-cb-amount");
    BOOST_CHECK(Connect(BuildNext({old_subsidy - 12345}))); // height H-2, underclaimed
    MineTo(H - 1);
    BOOST_REQUIRE_EQUAL(::ChainActive().Height(), H - 1);

    // The transition block (no fees here): exactly the maximum, nothing else.
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy - 1})), "bad-cb-amount-transition");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({0})), "bad-cb-amount-transition");
    // What the historical rule would allow at this height (3.125 RIN) is rejected too.
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({312500000})), "bad-cb-amount-transition");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy + 1})), "bad-cb-amount");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy})), "");
    // Only the sum counts, not the layout.
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy - 7, 7})), "");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy - 7, 6})), "bad-cb-amount-transition");

    // An underclaiming transition block does not become the tip; the exact one does.
    BOOST_CHECK(!Connect(BuildNext({new_subsidy - 1})));
    BOOST_CHECK_EQUAL(::ChainActive().Height(), H - 1);
    BOOST_CHECK(Connect(BuildNext({100000000, new_subsidy - 100000000})));
    BOOST_CHECK_EQUAL(::ChainActive().Height(), H);

    // Every later block is back to the plain upper bound.
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy - 1})), "");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({0})), "");
    BOOST_CHECK_EQUAL(RejectReason(BuildNext({new_subsidy + 1})), "bad-cb-amount");
    BOOST_CHECK(Connect(BuildNext({1})));
    BOOST_CHECK_EQUAL(::ChainActive().Height(), H + 1);
}

BOOST_AUTO_TEST_SUITE_END()
