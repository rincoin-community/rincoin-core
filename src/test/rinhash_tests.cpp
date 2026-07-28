// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <crypto/rinhash.h>
#include <primitives/block.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <version.h>

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

// Deserialize an 80-byte block header from its hex encoding.
CBlockHeader HeaderFromHex(const std::string& hex)
{
    CBlockHeader header;
    CDataStream ss(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
    ss >> header;
    return header;
}

} // namespace

BOOST_AUTO_TEST_CASE(rinhash_canonical_pow_vector)
{
    const uint256 expected = uint256S(
        "02b229adf0a67d35cfd176d5ee46b750ca698b97e1edc479787090856ca33222");
    BOOST_CHECK_EQUAL(RinHash(MakeFixedHeader()).GetHex(), expected.GetHex());
}

BOOST_AUTO_TEST_CASE(rinhash_mainnet_header_vectors)
{
    // Real main-network block headers (80 bytes each) with their known RinHash.
    // On Rincoin GetHash() == RinHash(), so these are also the canonical block
    // ids at heights 0 and 1, and block 1 links back to the genesis.
    const CBlockHeader genesis = HeaderFromHex(
        "0100000000000000000000000000000000000000000000000000000000000000"
        "00000000adcd471c60b9dc56b5dc049e567106388fdf078f936a722b42edd230"
        "85c0908500e8e467ffff001f28850000");
    BOOST_CHECK_EQUAL(RinHash(genesis).GetHex(),
        "000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db");

    const CBlockHeader block1 = HeaderFromHex(
        "00000020dbc9a8c73baa8093e74eb368f8e36fba9a606fbd4e079ba83c61e4d6"
        "bd960000902e3faad09b8f350a530702e126b19107be3218521dbf9eb5b394ca"
        "40e11278d9ebe767ffff001fa1070100");
    BOOST_CHECK_EQUAL(RinHash(block1).GetHex(),
        "00002adfb206d5d942abc963b93fa2edb479eb7b6f589f5318ddda5cd732ec19");

    BOOST_CHECK_EQUAL(block1.hashPrevBlock.GetHex(),
        "000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db");
}

BOOST_AUTO_TEST_CASE(rinhash_peer_proto_floor_params)
{
    struct Case { std::string net; int height; int floor; };
    const Case cases[] = {
        {CBaseChainParams::MAIN,    840000, 70018},
        {CBaseChainParams::TESTNET,   4200, 70018},
        {CBaseChainParams::REGTEST,    600, 70018},
        {CBaseChainParams::PREVIEW,    600, 70018},
    };
    for (const auto& c : cases) {
        SelectParams(c.net);
        const auto& consensus = Params().GetConsensus();
        // 70017 MWEB baseline holds from genesis up to just below the bump height.
        BOOST_CHECK_EQUAL(consensus.MinPeerProtoVersionFloorAt(0), 70017);
        BOOST_CHECK_EQUAL(consensus.MinPeerProtoVersionFloorAt(c.height - 1), 70017);
        // The RinHash floor applies at and after the bump height.
        BOOST_CHECK_EQUAL(consensus.MinPeerProtoVersionFloorAt(c.height), c.floor);
        BOOST_CHECK_EQUAL(consensus.MinPeerProtoVersionFloorAt(c.height + 1), c.floor);
    }
}

BOOST_AUTO_TEST_SUITE_END()
