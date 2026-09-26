// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Height-840,000 transition: the coinbase rule of the transition block, through real
// block connection on regtest (transition height 840).

#include <amount.h>
#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <miner.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <vector>

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
