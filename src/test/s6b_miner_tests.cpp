// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Height-840,000 transition: block assembly must survive a mempool entry that was signed
// for the other side of the transition height (regtest transition height 840).
//
// RemoveForSigForkBoundary() (validation.cpp) empties the mempool of such entries when the
// tip crosses the height, and it is exact. It has one deliberate escape hatch, an entry
// whose inputs are not available, and it cannot run at all if it is never reached. A single
// entry left behind would go into the block template, TestBlockValidity() at the end of
// CreateNewBlock() would fail, and block production on that node would stop until the entry
// expires. These tests put such an entry in the mempool directly, which is the only way to
// reach the state at all, and pin that the template is still produced and leaves it out.

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <key.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <test/util/setup_common.h>
#include <txmempool.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

namespace {

struct S6bMinerSetup : public TestChain100Setup {
    CScript script_pubkey;
    FillableSigningProvider keystore;
    TestMemPoolEntryHelper entry;

    S6bMinerSetup()
    {
        script_pubkey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
        BOOST_REQUIRE(keystore.AddKey(coinbaseKey));
    }

    void MineTo(int height)
    {
        while (::ChainActive().Height() < height) CreateAndProcessBlock({}, script_pubkey);
    }

    /** Spend one of the fixture's coinbase outputs, signed for the given side of the transition. */
    CMutableTransaction SpendCoinbase(size_t which, bool sig_fork_id_active)
    {
        const CTransactionRef& prev = m_coinbase_txns.at(which);
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(prev->GetHash(), 0);
        tx.vout.resize(1);
        tx.vout[0].nValue = prev->vout[0].nValue - 10000;
        tx.vout[0].scriptPubKey = script_pubkey;

        const Consensus::Params& consensus = Params().GetConsensus();
        SignatureData sigdata;
        BOOST_REQUIRE(ProduceSignature(keystore,
                                       MutableTransactionSignatureCreator(&tx, 0, prev->vout[0].nValue,
                                                                          consensus.sigForkId, sig_fork_id_active),
                                       prev->vout[0].scriptPubKey, sigdata));
        UpdateInput(tx.vin[0], sigdata);
        return tx;
    }

    /** Put a transaction straight into the mempool, as if it had been accepted at entry_height. */
    void PlantInMempool(const CMutableTransaction& tx, unsigned int entry_height)
    {
        LOCK2(cs_main, m_node.mempool->cs);
        m_node.mempool->addUnchecked(entry.Fee(10000).Time(GetTime()).Height(entry_height).SpendsCoinbase(true).FromTx(tx));
    }

    std::unique_ptr<CBlockTemplate> Template()
    {
        return BlockAssembler(*m_node.mempool, Params()).CreateNewBlock(script_pubkey);
    }

    static bool Contains(const CBlockTemplate& tmpl, const CMutableTransaction& tx)
    {
        const uint256 hash = CTransaction(tx).GetHash();
        for (size_t i = 1; i < tmpl.block.vtx.size(); ++i) {
            if (tmpl.block.vtx[i]->GetHash() == hash) return true;
        }
        return false;
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(s6b_miner_tests, S6bMinerSetup)

BOOST_AUTO_TEST_CASE(a_leftover_from_below_the_height_does_not_stop_block_production)
{
    const int H = Params().GetConsensus().nS6bHeight;
    BOOST_REQUIRE_EQUAL(H, 840);

    // The tip is the last block below the transition height, so the next block is the first
    // one under the new regime. A transaction signed for the old regime is planted with an
    // entry height from before the boundary, which is the state the mempool eviction exists
    // to prevent.
    MineTo(H - 1);
    BOOST_REQUIRE_EQUAL(::ChainActive().Height(), H - 1);
    const CMutableTransaction stale = SpendCoinbase(0, /* sig_fork_id_active */ false);
    PlantInMempool(stale, H - 2);
    BOOST_REQUIRE_EQUAL(m_node.mempool->size(), 1U);

    // Without the safeguard this call throws: the entry goes into the template and
    // TestBlockValidity() rejects the block.
    std::unique_ptr<CBlockTemplate> tmpl;
    BOOST_REQUIRE_NO_THROW(tmpl = Template());
    BOOST_REQUIRE(tmpl != nullptr);
    BOOST_CHECK(!Contains(*tmpl, stale));
    BOOST_CHECK_EQUAL(tmpl->block.vtx.size(), 1U); // coinbase only

    // The entry is left in the mempool, not removed: block assembly does not mutate it.
    BOOST_CHECK_EQUAL(m_node.mempool->size(), 1U);

    // The template is a block the chain accepts.
    CBlock block = tmpl->block;
    block.hashMerkleRoot = BlockMerkleRoot(block); // the assembler leaves this to the miner
    while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, Params().GetConsensus())) ++block.nNonce;
    BOOST_CHECK(Assert(m_node.chainman)->ProcessNewBlock(Params(), std::make_shared<const CBlock>(block), true, nullptr));
    BOOST_CHECK_EQUAL(::ChainActive().Height(), H);
}

BOOST_AUTO_TEST_CASE(an_old_entry_that_is_still_valid_is_kept)
{
    const int H = Params().GetConsensus().nS6bHeight;

    // Same starting point, but the planted transaction is signed for the new regime. Its entry
    // height is still below the boundary, so the safeguard looks at it and must not exclude it:
    // leaving out a transaction that verifies would silently drop it from every later block.
    MineTo(H - 1);
    const CMutableTransaction good = SpendCoinbase(1, /* sig_fork_id_active */ true);
    PlantInMempool(good, H - 2);
    BOOST_REQUIRE_EQUAL(m_node.mempool->size(), 1U);

    std::unique_ptr<CBlockTemplate> tmpl;
    BOOST_REQUIRE_NO_THROW(tmpl = Template());
    BOOST_REQUIRE(tmpl != nullptr);
    BOOST_CHECK(Contains(*tmpl, good));
    BOOST_CHECK_EQUAL(tmpl->block.vtx.size(), 2U);
}

BOOST_AUTO_TEST_CASE(below_the_height_the_old_regime_is_the_one_that_counts)
{
    const int H = Params().GetConsensus().nS6bHeight;

    // Two blocks below the boundary the next block is still under the old regime, so it is the
    // new-style signature that cannot be mined. The safeguard must not fire here at all: it only
    // looks at entries once the block being built is at or above the transition height.
    MineTo(H - 2);
    const CMutableTransaction old_style = SpendCoinbase(2, /* sig_fork_id_active */ false);
    PlantInMempool(old_style, H - 3);
    std::unique_ptr<CBlockTemplate> tmpl;
    BOOST_REQUIRE_NO_THROW(tmpl = Template());
    BOOST_REQUIRE(tmpl != nullptr);
    BOOST_CHECK(Contains(*tmpl, old_style));
}

BOOST_AUTO_TEST_SUITE_END()
