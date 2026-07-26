// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1358118740; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 280223;
    pindexLast.nTime = 1358378777;  // Block #280223
    pindexLast.nBits = 0x1c0ac141;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c178a7fU);
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1317972665; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1318480354;  // Block #2015
    pindexLast.nBits = 0x1e0ffff0;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1e3fffc0U);
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1401682934; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 578591;
    pindexLast.nTime = 1401757934;  // Block #578591
    pindexLast.nBits = 0x1b075cf1;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1b04a5fcU);
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1463690315; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 1001951;
    pindexLast.nTime = 1464900315;  // Block #46367
    pindexLast.nBits = 0x1b015318;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1b054c60U);
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits = ~0x00800000;
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, std::string chainName)
{
    const auto chainParams = CreateChainParams(args, chainName);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing (only relevant when retargeting is enabled)
    if (!consensus.fPowNoRetargeting) {
        BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);
    }

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // check max target * 4*nPowTargetTimespan doesn't overflow -- see pow.cpp:CalculateNextWorkRequired()
    /* rincoin: we allow overflowing by 1 bit
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        targ_max /= consensus.nPowTargetTimespan*4;
        BOOST_CHECK(UintToArith256(consensus.powLimit) < targ_max);
    }
    */
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::SIGNET);
}

namespace {
// Build a fabricated chain of `count` headers, all with the same compact target
// `bits`, spaced `spacing` seconds apart. Lets the Dark Gravity Wave retarget be
// exercised with no real chain state; the tip is the last element and the vector
// owns the CBlockIndex objects.
std::vector<CBlockIndex> MakeDGWChain(int count, uint32_t bits, int64_t spacing)
{
    std::vector<CBlockIndex> blocks(count);
    const int64_t start_time = 1600000000;
    for (int i = 0; i < count; ++i) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = static_cast<uint32_t>(start_time + int64_t(i) * spacing);
        blocks[i].nBits = bits;
    }
    return blocks;
}
} // namespace

// Dark Gravity Wave is the active retarget algorithm but was previously
// untested. Exercise its branches with a fabricated chain, forcing the DGW path
// from height 0. Exact targets are fragile (DGW uses a non-standard running
// average), so assert direction and the pow-limit clamp instead.
BOOST_AUTO_TEST_CASE(dark_gravity_wave_retarget)
{
    Consensus::Params consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    consensus.DGWHeight = 1; // force the DGW path for the whole fabricated chain

    const arith_uint256 powLimit = UintToArith256(consensus.powLimit);
    const uint32_t powLimitBits = powLimit.GetCompact();
    // A target comfortably below the pow limit, so difficulty can move both ways.
    arith_uint256 midTarget = powLimit;
    midTarget >>= 4;
    const uint32_t midBits = midTarget.GetCompact();
    const CBlockHeader dummy; // unused by the DGW path
    const int kBlocks = 30;   // > nPastBlocks (24)

    auto target = [](uint32_t bits) { arith_uint256 t; t.SetCompact(bits); return t; };

    // Fewer than 24 blocks of history: DGW returns the pow limit.
    {
        const auto chain = MakeDGWChain(10, midBits, consensus.nPowTargetSpacing);
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&chain.back(), &dummy, consensus), powLimitBits);
    }

    // Blocks far faster than target -> difficulty rises (target falls).
    {
        const auto chain = MakeDGWChain(kBlocks, midBits, 1);
        const uint32_t next = GetNextWorkRequired(&chain.back(), &dummy, consensus);
        BOOST_CHECK(target(next) < target(midBits));
    }

    // Blocks far slower than target -> difficulty falls (target rises), but never
    // above the pow limit.
    {
        const auto chain = MakeDGWChain(kBlocks, midBits, 100000);
        const uint32_t next = GetNextWorkRequired(&chain.back(), &dummy, consensus);
        BOOST_CHECK(target(next) > target(midBits));
        BOOST_CHECK(target(next) <= powLimit);
    }

    // Slow blocks already at the pow limit clamp exactly at the pow limit.
    {
        const auto chain = MakeDGWChain(kBlocks, powLimitBits, 100000);
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&chain.back(), &dummy, consensus), powLimitBits);
    }
}

// Lock the mainnet network identity and genesis so accidental changes are caught.
BOOST_AUTO_TEST_CASE(chainparams_main_identity)
{
    const auto params = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& consensus = params->GetConsensus();

    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock.ToString(),
                      "000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db");
    BOOST_CHECK_EQUAL(params->GenesisBlock().hashMerkleRoot.ToString(),
                      "8590c08530d2ed422b726a938f07df8f380671569e04dcb556dcb9601c47cdad");

    BOOST_CHECK_EQUAL(params->GetDefaultPort(), 9555);
    const CMessageHeader::MessageStartChars& magic = params->MessageStart();
    BOOST_CHECK_EQUAL(int(magic[0]), 0x52); // R
    BOOST_CHECK_EQUAL(int(magic[1]), 0x49); // I
    BOOST_CHECK_EQUAL(int(magic[2]), 0x4E); // N
    BOOST_CHECK_EQUAL(int(magic[3]), 0x43); // C

    BOOST_CHECK_EQUAL(int(params->Base58Prefix(CChainParams::PUBKEY_ADDRESS).at(0)), 60);
    BOOST_CHECK_EQUAL(int(params->Base58Prefix(CChainParams::SCRIPT_ADDRESS).at(0)), 122);
    BOOST_CHECK_EQUAL(int(params->Base58Prefix(CChainParams::SECRET_KEY).at(0)), 188);

    BOOST_CHECK_EQUAL(consensus.nSubsidyHalvingInterval, 210000);
}

BOOST_AUTO_TEST_SUITE_END()
