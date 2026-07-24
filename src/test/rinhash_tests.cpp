// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <crypto/rinhash.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

BOOST_FIXTURE_TEST_SUITE(rinhash_tests, BasicTestingSetup)

namespace {

// Fixed header used to pin the canonical RinHash output. The Argon2d
// parameters (t_cost=2, m_cost=64, lanes=1, salt="RinCoinSalt") are fixed for
// the whole chain, so this vector is stable.
CBlockHeader MakeFixedHeader()
{
    CBlockHeader h;
    h.nVersion = 0x20000000;
    h.hashPrevBlock.SetNull();
    h.hashMerkleRoot = uint256S("0102030405060708090a0b0c0d0e0f10111213141516171819202122232425fe");
    h.nTime  = 1750000000;
    h.nBits  = 0x1e0fffff;
    h.nNonce = 0xdeadbeef;
    return h;
}

} // namespace

BOOST_AUTO_TEST_CASE(rinhash_canonical_pow_vector)
{
    const uint256 expected = uint256S(
        "02b229adf0a67d35cfd176d5ee46b750ca698b97e1edc479787090856ca33222");
    BOOST_CHECK_EQUAL(RinHash(MakeFixedHeader()).GetHex(), expected.GetHex());
}

BOOST_AUTO_TEST_CASE(rinhash_peer_proto_floor_params)
{
    struct Case { const char* net; int height; int floor; };
    const Case cases[] = {
        {CBaseChainParams::MAIN,    840000, 70018},
        {CBaseChainParams::TESTNET,   4200, 70018},
        {CBaseChainParams::REGTEST,    600, 70018},
        {CBaseChainParams::PREVIEW,    600, 70018},
    };
    for (const auto& c : cases) {
        SelectParams(c.net);
        const auto& consensus = Params().GetConsensus();
        BOOST_CHECK_EQUAL(consensus.nMinPeerProtoVersionFloorHeight, c.height);
        BOOST_CHECK_EQUAL(consensus.nMinPeerProtoVersionFloor,       c.floor);
    }
}

BOOST_AUTO_TEST_SUITE_END()
