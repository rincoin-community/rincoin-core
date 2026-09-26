// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2024-2025 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <hash.h> // for signet block challenge hash
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <algorithm>
#include <assert.h>
#include <stdexcept>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <arith_uint256.h>
#include <util/system.h>  // for LogPrintf, if you want
#include <consensus/params.h>  // for Consensus::Params
#include "crypto/rinhash.h"  // Change the path according to the location of RinHash

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
 static CBlock CreateMainGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
 {
     const char* pszTimestamp = "RinCoin Genesis Block - RinHash Launch";
     const CScript genesisOutputScript = CScript() 
         << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f")
         << OP_CHECKSIG;
 
     return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
 }

 static CBlock CreateTestNetGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
 {
     const char* pszTimestamp = "RinCoin Genesis Block - RinHash Test1";
     const CScript genesisOutputScript = CScript() 
         << ParseHex("049dcc1230171f40b336c78b70c32ff5109172a9e30d577e4071fb69e30ee40be7732aeaaf5497bf230a4640406a9c1b7c785732c380cd604bfa06802a1ba3894a")
         << OP_CHECKSIG;
 
     return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
 }

 static CBlock CreatePreviewGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
 {
     const char* pszTimestamp = "RinCoin Genesis Block - RinHash Preview2";
     const CScript genesisOutputScript = CScript()
         << ParseHex("049dcc1230171f40b336c78b70c32ff5109172a9e30d577e4071fb69e30ee40be7732aeaaf5497bf230a4640406a9c1b7c785732c380cd604bfa06802a1ba3894a")
         << OP_CHECKSIG;

     return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
 }

 static CBlock CreateRegTestGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
 {
     const char* pszTimestamp = "RinCoin Genesis Block - RinHash RegTest1";
     const CScript genesisOutputScript = CScript() 
         << ParseHex("04b1c2d3e4f5a6b7c8d9eaf1b2c3d4e5f6a7b8c9dae1f2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9eaf1b2c3d4e5f6a7b8c9dae1f2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1")
         << OP_CHECKSIG;
 
     return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
 }

static std::vector<uint256> GetFrozenMWEBOutputIDs()
{
    // Rincoin: MWEB has never been active on any Rincoin network, so there is
    // no output to freeze. Upstream Litecoin lists its own mainnet output here.
    return {};
}

// Regtest-only test vector shared with the upstream functional test
// mweb_p2p_mutated_block_submitblock.py (upstream uses the same ID on every network).
static std::vector<uint256> GetRegTestFrozenMWEBOutputIDs()
{
    return {
        uint256(ParseHex("2f3a08d9f5ef5f388386c11efe935394b14b524220cff4ec5c81942b82e694f7")),
    };
}

/**
 * Height-840,000 transition (S6/b): derive the activation height and the subsidy
 * table from the network's halving interval, using the same multiples on every
 * network. On mainnet (interval 210,000) this yields 840,000 / 2,100,000 /
 * 4,200,000 / 6,300,000 and the terminal height 234,587,500, i.e. exactly the
 * schedule of analysis/Rincoin_840k_S6B_Consensus_Change_Specification in the
 * consensus-840k repository. The terminal height is where the issuance ceiling
 * (168,000,000 RIN on mainnet, scaled by interval / 210,000 elsewhere) is
 * reached; where the scaled ceiling is not a multiple of the final subsidy the
 * division rounds down, so the remainder stays unclaimable.
 */
static void SetS6bSchedule(Consensus::Params& consensus)
{
    const int64_t interval = consensus.nSubsidyHalvingInterval;
    const CAmount tail_subsidy = 60000000; // 0.6 RIN

    // Maximum issuance below 30 intervals: 50, 25, 12.5, 6.25 RIN for one interval
    // each, then 4 RIN for 6, 2 RIN for 10 and 1 RIN for 10 intervals.
    CAmount issued = 0;
    for (int k = 0; k < 4; ++k) issued += ((50 * COIN) >> k) * interval;
    issued += (4 * COIN) * 6 * interval;
    issued += (2 * COIN) * 10 * interval;
    issued += (1 * COIN) * 10 * interval;

    // 168,000,000 RIN / 210,000 blocks = 800 RIN of ceiling per block of interval.
    const CAmount ceiling = 800 * COIN * interval;
    assert(ceiling > issued);
    const int64_t terminal = 30 * interval + (ceiling - issued) / tail_subsidy;
    assert(terminal < std::numeric_limits<int>::max());

    consensus.nS6bHeight = static_cast<int>(4 * interval);
    consensus.vS6bSubsidyPhases = {
        {static_cast<int>(4 * interval), 4 * COIN},
        {static_cast<int>(10 * interval), 2 * COIN},
        {static_cast<int>(20 * interval), 1 * COIN},
        {static_cast<int>(30 * interval), tail_subsidy},
        {static_cast<int>(terminal), 0},
    };
}

/** Scale a mainnet height to a test network by the ratio of the halving intervals, rounding down. */
static int ScaleMainnetHeight(int64_t mainnet_height, const Consensus::Params& consensus)
{
    return static_cast<int>(mainnet_height * consensus.nSubsidyHalvingInterval / 210000);
}

/**
 * Height-based start and timeout of a version-bits deployment on a test network, from the
 * mainnet heights: scaled like every other height, then rounded down to a multiple of the
 * network's confirmation window. A version-bits state only changes on a window boundary, so
 * with aligned heights the configured numbers are the heights at which something happens,
 * as on mainnet. The timeout is kept at least one window after the start, so that every
 * deployment has a signalling period. consensus.nMinerConfirmationWindow has to be set first.
 */
std::pair<int64_t, int64_t> AlignDeploymentHeights(int64_t start, int64_t timeout, int64_t window)
{
    assert(window > 0);
    const int64_t aligned_start = start / window * window;
    return {aligned_start, std::max<int64_t>(timeout / window * window, aligned_start + window)};
}

static void SetScaledDeploymentHeights(Consensus::Params& consensus, Consensus::DeploymentPos pos, int64_t mainnet_start, int64_t mainnet_timeout)
{
    const int64_t window = consensus.nMinerConfirmationWindow;
    assert(window > 0);
    const auto aligned = AlignDeploymentHeights(ScaleMainnetHeight(mainnet_start, consensus), ScaleMainnetHeight(mainnet_timeout, consensus), window);
    consensus.vDeployments[pos].nStartHeight = aligned.first;
    consensus.vDeployments[pos].nTimeoutHeight = aligned.second;
}

/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 26500; // 87afb798a3ad9378fcd56123c81fb31cfd9a8df4719b9774d71730c16315a092 - October 1, 2012
        consensus.BIP34Height = 26500;
        consensus.BIP34Hash = uint256S("0x00");
        consensus.BIP65Height = 26500; // bab3041e8977e0dc3eeff63fe707b92bde1dd449d8efafb248c27c8264cc311a
        consensus.BIP66Height = 26500; // 7aceee012833fa8952f8835d8b1b3ae233cd6ab08fdb27a771d2bd7bdc491894
        consensus.CSVHeight = 26500; // 53e0af7626f7f51ce9f3b6cfc36508a5b1d2f6c4a75ac215dc079442692a4c0b
        consensus.SegwitHeight = 26500; // 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893
        consensus.MinBIP9WarningHeight = 25000; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 33 * 60 * 60; // 33hour
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 6048; // 75% of 8064
        consensus.nMinerConfirmationWindow = 8064; // nPowTargetTimespan / nPowTargetSpacing * 4
        consensus.DGWHeight = 30000; // Dark Gravity Wave (DGW) difficulty adjustment algorithm
        // Peer-protocol-version floor schedule (height -> min version): 70017 is
        // the MWEB-capable baseline required from genesis (symbolic: the network
        // already runs >= 70017); 70018 (RinHash-aware) is required from the
        // fourth-halving boundary onward.
        // Height-840,000 transition (S6/b subsidy, sig_fork_id, activation-block coinbase rule).
        SetS6bSchedule(consensus);
        assert(consensus.nS6bHeight == 840000);
        assert(consensus.vS6bSubsidyPhases.back().nStartHeight == 234587500);
        assert(800 * COIN * (int64_t)consensus.nSubsidyHalvingInterval == MAX_MONEY);
        consensus.vMinPeerProtoVersionFloors = {{0, 70017}, {consensus.nS6bHeight, 70018}};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartHeight = 2161152; // End November 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeoutHeight = 2370816; // 364 days later

        // Deployment of MWEB (LIP-0002, LIP-0003, and LIP-0004): not activated on mainnet, in
        // line with other implementations of this chain. The deployment heights inherited from
        // Litecoin would have activated it at the timeout height even without signalling.
        // In 2026 Litecoin had to fix a consensus flaw in its MWEB validation that allowed the
        // MWEB balance to be broken on its mainnet (Litecoin Core 0.21.5.4 to 0.21.5.6, all of
        // it included here); activation is left to a later release that decides it deliberately. MWEB
        // transactions stay non-standard and MWEB data in a block stays invalid, as they are
        // before any activation. The test networks keep their scaled heights so that MWEB
        // remains testable there.
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Both from block 750,000 (2026-09-22), read from a node that validated the whole
        // chain itself (assumevalid at the genesis block). Update them for every release, to a
        // block some thousand blocks below the tip at that time and never at or above 840,000.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000790415d8f4bee");
        consensus.defaultAssumeValid = uint256S("0x00000001115a0298260b3f6d0ed73a1174aefcc337aaea997c79ce7e7c3d683a"); // 750,000

        // Rincoin: no grandfathered block (the upstream value is a Litecoin mainnet block hash).
        consensus.mweb_input_metadata_grandfather_blockhash = uint256();
        // Rincoin: MWEB has never been active, so both rules apply from MWEB activation
        // (upstream Litecoin uses mainnet flag-day heights here).
        consensus.mweb_pegout_feature_activation_height = 0;
        consensus.mweb_extradata_feature_activation_height = 0;
        consensus.frozen_mweb_output_ids = GetFrozenMWEBOutputIDs();

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x52; // R
        pchMessageStart[1] = 0x49; // I
        pchMessageStart[2] = 0x4E; // N
        pchMessageStart[3] = 0x43; // C
        nDefaultPort = 9555;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        // CreateGenesisBlock(nTime, nNonce, nBits, nVersion, reward)
        genesis = CreateMainGenesisBlock(1743054848, 34088, 0x1f00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db"));
        assert(genesis.hashMerkleRoot == uint256S("0x8590c08530d2ed422b726a938f07df8f380671569e04dcb556dcb9601c47cdad"));

        
        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        
        vSeeds.emplace_back("seed.rincoin.tech");  // Community Forge DNS seeder
        
        base58Prefixes[PUBKEY_ADDRESS] = {60};  // "R..."
        base58Prefixes[SCRIPT_ADDRESS] = {122}; // "r..."
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,50);
        base58Prefixes[SECRET_KEY] =     {188}; // "7J../7K..."
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};  // "xpub..."
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};  // "xprv..."

        bech32_hrp = "rin";
        mweb_hrp = "rinmweb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0, uint256S("0x000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db")},
                {1934, uint256S("0x0000ee5e0179767c5acc40332ddbd2b78c43c8b4341f479d2ae86bdcbb997b0d")},
                {4396, uint256S("0x000013e35a9a731f5ee9fdda5c3202b577708f1bb0e74c0874ec73d8ccad85c6")},
                {6543, uint256S("0x00000010ab92475f713b8b25dad8c0a7a9742c7339357a4c848f57d72cf3165e")},
                {9353, uint256S("0x0000064ed9d26478185cddd55c790b5a5b9fd5b9193bfe2fbe93be1724a21fca")},
                {14434, uint256S("0x000005fd9a44022f51d359792ce404030523aaa1dacf5c583e6a9dbb286b5fbf")},
                {17164, uint256S("0x000000bbcf32e4cd6b97f6cda7eab5ee89efdf79b5e87e608d69607fb6d75d63")},
                {19331, uint256S("0x0000056ef607edb8de720ed7c9b11657013344c64c400e07905d2696c2b1326e")},
                {21232, uint256S("0x0000006250cae03904ffa3dde06ec90f10f629f9ce2034bda4ca8c073d622f3d")},
                {26085, uint256S("0x00000008919e381c843b4ce1ba352f5d3efa53198c67fa9fa999969cfd32a427")},
                {28981, uint256S("0x0000001038a39f8b2eb9acaa733e5f926ef1ca07e21d98c73c5b2de9071ed6a1")},
                {31280, uint256S("0x000000000b7498a3babfebb23f007e56e222229ab57ec2ec706510e45c46e4bc")},
                {36222, uint256S("0x000000001352ca8863903ba0d1cc622dd046a6de04f5831a7bd3844091b7909d")},
                {38659, uint256S("0x000000001c3b3db7bb67b7a85805776b7efb4d9ee5ff76f859e30de562fb7e18")},
                {40943, uint256S("0x00000000240d8c7a31d5032858584f9da349d7f17071080817136aaf06fda8b8")},
                {45115, uint256S("0x00000000123b27dcea3980d3a59a22bbe56cc136675a9c6f8b8698f045e5d420")},
                {50132, uint256S("0x00000000382e8971735548f01397bb07064152da09ecec4f7d42e803506f7743")},
                {52405, uint256S("0x0000000022458d4379b119e89a583a977688126579b7ad35ab208d8f7ae42865")},
                {55012, uint256S("0x0000000022a2edb20e712bf5ad330a1697cb11e304e8d75a824b8871dc63b05b")},
                {57710, uint256S("0x000000002fc6c6dc01e2a7f377dcb0f3a347a031a776d4d9961bc42e00910033")},
                {60879, uint256S("0x000000005e5527a68653b6e3550685eb5b1578c497b68a1fe60efc4ef2bd8126")},
                {63984, uint256S("0x00000000158f932956aa838b4732913ad137a985ea824fbb7bb144e192e1e50e")},
                {67806, uint256S("0x0000000036e433f6e4bed6e216bd424a3d780b94e34352d16ddb66d26a20990a")},
                {72001, uint256S("0x0000000117f581b1718436f146a17a9e25f67cd1bae119ebd6a5a2948137dbc2")},
                {74513, uint256S("0x000000004e2dccee21c598cd8afeeb1d127b0fffebfab202ba2dca6596c54d77")},
                {78202, uint256S("0x00000000138fdf5727423df41f63ad0f2e2625a21d669041cd4316a3d3550594")},
                {82028, uint256S("0x0000000019e3561a51cf039193c0742ae60862c73905975895a72d90bd9ed08f")},
                {87176, uint256S("0x000000004d7064f12ad79af87edffd1283d0426d902b4ee1f170919a2a3f4a6f")},
                {89789, uint256S("0x00000000787a16b83ee98caaec64507f12a31b2293b1927a0b3fd54116dcf56a")},
                {92168, uint256S("0x000000002948a47bc6804685f1bc99daba2d058e42d0e637dfb475788ce40537")},
                {96950, uint256S("0x00000000b4681b6f8f8d49143ca16f231f072359a4fd1e6c1beb33e3180594c8")},
                {99968, uint256S("0x0000000105e3a6f8a5a331e65d6a425c409f30ae337bdc4bd91992829561cc6a")},
                {104868, uint256S("0x00000000ab78c2931bc7ca6a6f6ec547bc898f18164f9d08ddf67900e5bb8537")},
                {107656, uint256S("0x00000000ef34bda4359f9c06a66123ab793989eda33c12abbd467d8e77315908")},
                {112388, uint256S("0x00000002a847e6d3af1cdba624471dab69cce38d946d764bb3892277b485eb88")},
                {114910, uint256S("0x0000000007aa827f1d72a12ba9de71ca899d8e97be62f10aacec8c5f0f6465da")},
                {118270, uint256S("0x0000000232fec3de1461fc4b91aeaf40cd53a24dad717148e23c9c925006195d")},
                {121771, uint256S("0x00000000c0971e5b0fde5ff8ee3476ea2e9643672bc4e8b3be1a98a416aee7d9")},
                {126481, uint256S("0x000000018968cecdec5a32baa178b7611fd086f2abc28f7a582bc43da11268e3")},
                {129635, uint256S("0x0000000113d53b859bfc24c7a7968a8f257497db0877633b5d0b65da95242c93")},
                {133542, uint256S("0x0000000187b67de05f527e0e454a1fe8f9f5603d1eaab7f2250ed376bd7be205")},
                {136650, uint256S("0x000000006dcfc097c7bd482c0337866ffb93e7a84eb8f09cfc9d84b79448a906")},
                {140352, uint256S("0x000000003ec3020ed3cd48f0c5e75dc194c52b307763ef311d31bfb29300dcd5")},
                {142385, uint256S("0x0000000076f9bc76a796938f44fcd9681a27060d1797858f60e861ebfe45535d")},
                {144270, uint256S("0x0000000084bc18bc5386eba677b2dcb244484cbe6944036a789be8926b0da4a0")},
                {147261, uint256S("0x0000000044af605e4d49f2aaf818ee076d3e8b0a52a35a9eafa68f404ddd330e")},
                {152003, uint256S("0x00000000a3689154d658241b925926a529675255c2ff3d731536f16463b4feea")},
                {154695, uint256S("0x000000018911afe5b9d3cfba55e5c3228d7465effd30b8f768e9f60d0e729d8f")},
                {157689, uint256S("0x00000001546573134cb026e1093d4d6e795ab72f835004b1c6cc2c65fa9e5fcd")},
                {162663, uint256S("0x0000000164b0e4a8790f1ac42be4f7cb3f4bb8ff3484bd38638422bc6167b95d")},
                {165591, uint256S("0x00000001395c8605f697ebb46bfd35b61f19d6e8e30708e9fcfc0915bb94efbf")},
                {170182, uint256S("0x00000000e554d4b18b90ceeb9d09a2fc31a15b7396759843804f6635c85f1518")},
                {174121, uint256S("0x0000000009befbbade61a7e1b74836cc4b484d0948afd07e576a79960ec60381")},
                {178235, uint256S("0x000000010eeb6284d11facbd9c19c91746401af0f7da17eb38850911b7d9f74b")},
                {181273, uint256S("0x000000000530b49518ed7e6b5aaf86dfe3f6b1f43e9ac96fc11124637b57bbc2")},
                {184972, uint256S("0x00000000fd47d50148efdd21f71b25045a63746690df361adb5d46c65a389eac")},
                {189399, uint256S("0x000000001262a951c92ecf9721c89e9f6d1ba3f63f44e32624ff2b440bda21db")},
                {192518, uint256S("0x000000002faaadd3a71229b79568e9998732a709f3bc017b3a1876ea45bbcaea")},
                {197565, uint256S("0x0000000091df023ced297fcdaa2929745f027872311c0d799e5049bf4cdd8899")},
                {199453, uint256S("0x00000000be64dbde02b723c1399956e8249845ba756aa54566435c0c4711cc43")},
                {202446, uint256S("0x000000011a24c56889fa6c1f2c16d15cdfe80d6b0b2075bd2ca46beb7392574e")},
                {206964, uint256S("0x000000003c7ccfb47034d7fad1285555b371f4c05e4e4f5c6f3db5cc47320112")},
                {211194, uint256S("0x0000000152cbf3b914f0603dead75f7ab5fcce2d33542ff2f96fd4f2b996844f")},
                {214853, uint256S("0x00000001ccb9c78e57c7f3320d1e969ae4fc6418270212d19cd71776d70c29c8")},
                {217608, uint256S("0x0000000126e6c1bdf47439ee780b0a4d9502455a6f1161b077a77f142ffe1d52")},
                {219466, uint256S("0x000000009064ba595789a9e144df95a1b2e51100f005c4e3287cbe317da5f571")},
                {223771, uint256S("0x000000000e0742937ba4c0e22b1daec1b371a9b9d55f3cd88ff580d442218b8e")},
                {228788, uint256S("0x000000012b078219f1d1d7e18be2f1a68a581ccfeaec8e59f4169c7a793fcef2")},
                {233369, uint256S("0x0000000099e40948b46bab68a128a5d9294060274abe820a9b2d814abb2196de")},
                {236884, uint256S("0x00000000ae4f2cddfd2185617c7a15ae2d00146d7c4b1d1735705b0dc3127b0b")},
                {239516, uint256S("0x00000001247837d303618fa21f393dd60871ecf181b1701e2b6d7499f373fe2d")},
                {243432, uint256S("0x00000001371508706e40545386cf0b6fa80d3bb25c998274de2e7d277a42d1f0")},
                {246968, uint256S("0x0000000008287976f4a13a445dc237b271a160f1dfd6cb3604a8c42fc37f4f75")},
                {249441, uint256S("0x00000000799c356f4c104be502cc846118f7aaf257c099fcc12b4fb1296b9c3e")},
                {253022, uint256S("0x00000000c79557ec69a87ef70a70eadf47a7c299bc844245199d37cbf8450894")},
                {256361, uint256S("0x00000000365107ed5eb2ef978c7e741a0ed125a3fc81485c49fb2ac9f87a2677")},
                {258699, uint256S("0x00000001186289a302405fcaf753c4bc9cf2f164a226a7dd61c34a4b4f1d8322")},
                {261515, uint256S("0x00000001130918f40f22f792be505416076d71b89be462735c69d87a8ec8e64e")},
                {264377, uint256S("0x00000000224d9b6756cad5121cb106367b9bc4e05dee239dbb4670057b366f29")},
                {268755, uint256S("0x000000017372f90ec9b8e2f71196f5f57bf1753a4789d5b245dfda674339842a")},
                {272887, uint256S("0x0000000093f880418d898e4042b2e5ab2d85694a5f03f18529094957c1c942c5")},
                {277860, uint256S("0x000000003f6bf20782fc1f8ffc208102eb5007276448b0211d9866bea63bf22f")},
                {280929, uint256S("0x0000000053f9a800ba8c0ce1cd544d9d1dd0f9e2389be7df096c7194b1698f0a")},
                {283773, uint256S("0x000000003deb0bf68b6825bc9b55181e33b84fc0d40246c633cd1588ab4af0f7")},
                {288325, uint256S("0x0000000132f0808d021279f221419cb1f19a37cac75848f845b3478ba291ed09")},
                {292711, uint256S("0x000000000ad10e07fffea334d826cf28ad219d13dd9260030874b61c39c87f7f")},
                {295822, uint256S("0x00000000769f7443ab7848e3247182cab2afbb045d82dc530e0c1dad0c5b8042")},
                {300834, uint256S("0x000000004159dbf16ddb83eadfd11531227c295c84abaa281256e97a7bd78a90")},
                {305287, uint256S("0x00000000631af08bb53cf7734b62ee8724ce6351cbed32b46627e8157a0d1283")},
                {309465, uint256S("0x000000005cbd2741f2386a1f9396b4b6f0c85c86fce5738cedfbc7fdfed62e98")},
                {313272, uint256S("0x00000000b140de0acb2df9c571257177bddb8d8d321452f8f8b1e27906afda12")},
                {317893, uint256S("0x00000000c753220c966915d7159af7ada50e4de75baebcc612e2cac834f350ec")},
                {321595, uint256S("0x000000014d4eb8ccc7ea3ebce90bf829458d24f2087580dd5eb936ccc93f96cf")},
                {326623, uint256S("0x000000016115233a6da0f09ab0e50c3180c215daaaca0e57ecdedf4a97f4a246")},
                {329409, uint256S("0x00000000eaa284d2caf4f6239e96be089e022ce894679ee62c1a21a2ff47b8bd")},
                {334449, uint256S("0x00000000e34a7db3c8e72d4c721eacbe49046df404776727171a74108a489a58")},
                {339280, uint256S("0x000000003637bbcf6796fe353019b7ac16dc1874d8ffaa280f1b0cbe112086de")},
                {342792, uint256S("0x00000000177b411daf1c7035e8e780f3b6821fceb82adf203df00ac45e4e4de2")},
                {347566, uint256S("0x000000015d67244b2aa0fc54c893f25bc2f37f034331397597e40d1d69204fb2")},
                {349417, uint256S("0x000000002544fc5e7603950850e217b9e971539cfcc9aa8ed6b9a52cde86a1a4")},
                {351407, uint256S("0x00000000987aed17fb200d4a2b9439188a90e49d2fa629d5fef7be895d484c54")},
                {354221, uint256S("0x00000000cde0a7aba4d124309e1956765709ee6031c6f42a6d68c183233bfffb")},
                {358011, uint256S("0x00000000384de01c68a2400393a2d52116712b4cc37ad712bbc013a55b16710a")},
                {361444, uint256S("0x000000018e4e7deb9e0c0d2b6e512ef83918faece8be8ba384d70529b10e8434")},
                {363968, uint256S("0x000000005e03638eb214bac75818443d4a267b86bd1ae5785f5f2029ef617ef0")},
                {369204, uint256S("0x000000016579e15e4959c7df1eea3b1d63c83016b24c94c67604abd09a297505")},
                {372270, uint256S("0x00000001ddd9dabd2a2b6ab867cb833068a7b048834f366962dd7684d4ca1689")},
                {376124, uint256S("0x00000001bdabe1eb7c2fe828f1d9eaf06f63c78a6ffa0fcc4fbb1dc5d2ec2c42")},
                {378409, uint256S("0x0000000245c283c4abccab0f5902716735ee64230d45f2d61646be85694727f9")},
                {382970, uint256S("0x000000012f54f98ae59bd5a3dc1b93354dd05df533c588575c7388c90aac2c0a")},
                {386494, uint256S("0x0000000188437fc5abe56f534698eb7ad99303095a4f3de3d176679e18ec5dc1")},
                {389025, uint256S("0x00000001507dc8e710e08151ae3b97fa2eba97d7f125247d9ae2f34a88ea6920")},
                {394146, uint256S("0x00000000b467fff679005dd05c51fd3ce6b9797aa9746a70ab7c2b234c9351b9")},
                {398422, uint256S("0x00000001413443e7902933eaf82fd2508c37a2d3060582545d6639deaf85baf6")},
                {401479, uint256S("0x000000010188a23490ebc7edf79cda541bafd240ef1e53ffe00e76c480e196d7")},
                {405293, uint256S("0x00000002208601b9464a80dc6c708879d2008c64f684be281b5e0cf13e969f24")},
                {407724, uint256S("0x00000000017f843fc817777433e619e893d4b37642adbc5f56bc8622665d7531")},
                {412656, uint256S("0x00000000d5303d1b3aab8964553f39c4f9d1b8237d894aa8d357cca7c6030e94")},
                {417840, uint256S("0x0000000177b184939ac89f4e9e09285a95a2221db8557e9a3d8ff5d50fcf4ab9")},
                {420299, uint256S("0x000000022c33916ed5814f5834f19b0497d399b2762b5e4cd19809a21ac38dc6")},
                {423311, uint256S("0x000000029390ce2534e565f6022144a494daafa1b6e8f033c1ad64469eabc8ae")},
                {425102, uint256S("0x000000017239bf2a67613fae8001bb6829c738b977b17655bfd4c6f3930222db")},
                {428257, uint256S("0x0000000106efeb97bc659c4a64854fd87496ddffd34b9d364f5075ab669c1812")},
                {431349, uint256S("0x000000000ba202bb8e21b2d1692771005d5068a034442908582ad1217f0def8c")},
                {435935, uint256S("0x000000011e8878e186627ae2a8044aaf34634d4130d9a6c9a9d321edcf73f80d")},
                {440000, uint256S("0x00000001d05a9500a185fe4fe9ec3cf9b3325d0add4012a1b35a93ec98c204fe")},
                {443670, uint256S("0x000000033a8f66bde87b4aba7100a23a1edb20dd71483d7665cc30051af00871")},
                {447836, uint256S("0x0000000266a6ad7f17d826cb3d9531a54bd386f181858f25cd072249ca93e6ec")},
                {449861, uint256S("0x00000000a4734aaf439c7a29b503011ae18d3ab34e7da9ead4b44086b608a2cb")},
                {452087, uint256S("0x00000002d6592176adc0a38d3c4dc3d098fd69fc1b8cb5948d7bae48721edb17")},
                {454271, uint256S("0x00000002571d25e9d8ec91be3bc06a0ab4a701a411ea79313fed9f0ad4b03f20")},
                {460093, uint256S("0x0000000315081393ea2b5804a6ef409f1eaee5d61a912ac93cc45ff060dac0a8")},
                {465167, uint256S("0x000000026ba8ec4eefc79ca3adfd597bf128c3338d92a5aa21486487a920ef80")},
                {470400, uint256S("0x00000001c367ba480f54c956dbf09caba12c6ffb3df74ecafbb56ebd316b34b7")},
                {475934, uint256S("0x000000024d2fe66357c3f5690461efd7a421f73848fb2aa3d2228d25e89f3c35")},
                {479505, uint256S("0x0000000067b3a8f69359399067dfbb7c57885ff9d4f99c73f6034a9ce0e21186")},
                {483566, uint256S("0x00000002f4a97503b2b8136c2399bddb259897761c8f7e5d2b711ed82aabb53c")},
                {488849, uint256S("0x000000021e3c7025cc192e5a61de1cf98b186290b96b6942db2de1ae855c6d80")},
                {492108, uint256S("0x00000001012944d3f34a3cf449248b7b48dbcbed2d8bf9f928ca50df1fde4574")},
                {495034, uint256S("0x00000001d806e95591e8c25e36e100a2a8a950ac691d53575acdf6b4ddef0228")},
                {498147, uint256S("0x000000012f6642cfb300fd67e74c95c6575aac910a8242cb73d067b1204068d4")},
                {500649, uint256S("0x00000000758c1c5cabd04da5332f07f2491e5098f20856967a54b9d6514c50b2")},
                {505935, uint256S("0x000000015e63e37fd88947045999fb7794d2bb9a42f8f3f406521e9d1f6a1649")},
                {511411, uint256S("0x00000001ddc6441d76fdd8115dde41b62eab8fcf93bb536e8ebb1a335a93ec8f")},
                {514520, uint256S("0x0000000179e2da5e4a368a2dc5a68a21e82bbb7925e99fe52ccf1ce9c44e092e")},
                {520154, uint256S("0x000000003626b1e03bad426c2d60016ee111d30c11b1c15fddbb4daa87facd09")},
                {523044, uint256S("0x00000003989d1bbaa78ddce066e149aadea2c0697d611bd7dd79f9ebdd5f276a")},
                {528918, uint256S("0x0000000168f341bf13f3ee7ff4b88edd9f991fdcab1b166467fc9631fad87099")},
                {532996, uint256S("0x000000039d9d6bf6120a905680c9aa794b910fa55a8ebfe2d4046346c6520a47")},
                {536378, uint256S("0x00000001fb9a118cc73b3ba0b22f22300e67599689bac307caa23b62474ac0d6")},
                {538902, uint256S("0x000000001906e83bbf7309fa1fa4bbc9884fc27348ab1527da14adaf0f968c0d")},
                {542411, uint256S("0x0000000113ceed0b0d08ecd7e407f414ecc1df254f4a1d6e5c61be8dde54337c")},
                {545496, uint256S("0x00000002aa3106eecbc41bb3e10de0c92b95fe8aa508c611dc9d407033c0a9b9")},
                {547611, uint256S("0x000000020a8b4e910548f031e7747772b38cc353c71833422913babb13296ed7")},
                {550845, uint256S("0x000000019a83dc7bbc6b79b3d11a830b009400d1689b6f80b6866ecd6df3e744")},
                {555385, uint256S("0x00000001dc883d0d9c72ef1d5cb916b3fd3a4910406985f1083464e298591efd")},
                {559517, uint256S("0x000000017338c4ce1fdf825781e55ca7491a45b5514880c9561b9f1b48d7ee0c")},
                {564769, uint256S("0x000000037387ff8ad5c59c142ce95e2d0d0dcef9e3a6dd56e08579eaf8c167bb")},
                {568403, uint256S("0x00000002793dded4556d52593b245928f19ddf57925a5cd91606d885a80b1272")},
                {574313, uint256S("0x00000003b2bd297540daac7dfa419a5a3a89f6b88471fc6bbd6e1cdd8e9c9b53")},
                {579575, uint256S("0x00000000acfb9788901e8517e3830b62346849ce4e6fa0565a5b73f2d37d8d41")},
                {583521, uint256S("0x00000001d2d4fbad34a80810e9c98b6c5183e131ddda25120ff2a51172cf913c")},
                {586910, uint256S("0x00000001fc13c35c232d7b71a439bbb58ce87990498eda5cf4e69a45b802f52c")},
                {590848, uint256S("0x000000039c65ec08d79b250378cdce31092372eafaa7a411ec2abc7c340449f4")},
                {593182, uint256S("0x00000001e750803583e162c552a5b63123e14b43805ae7e9cf693394fc5215ce")},
                {595375, uint256S("0x0000000302990a82811c7e6ab7d17ca7592e4a6dbcebc81d95a685d70ba6ad84")},
                {600948, uint256S("0x00000001d7b5f1564113d304d0181b17076755fbb83399c745c440588d1491dc")},
                {606594, uint256S("0x000000019c304fc6277b5e776b95c34e4fa6730609a7cd5660f2fb26b73765bd")},
                {609217, uint256S("0x0000000337305d0b08900283d1cc7004d8dcd8bd807da6c8ba8f11df0765b154")},
                {613594, uint256S("0x000000002b69d767bb2cde52360e558b8917cec776e200a129a55fff16f48a4b")},
                {616895, uint256S("0x000000071a40309b9513f230de47d741b7eed4acd26486e901f093f681017cc3")},
                {621001, uint256S("0x0000000216458c787d45acf1b4c07111b1c39218fca428b415edf0d340059031")},
                {626397, uint256S("0x00000004951583c3e7e6b9886dd43f2305023be568f421f378f7d52463c2ebe5")},
                {631143, uint256S("0x00000000058327914905707763b3d38ab84c4ea51baecb5ea3f835760e218708")},
                {635591, uint256S("0x000000042c38628436edc34266579eca0daffb24521ba17fece6ccba6e680625")},
                {638450, uint256S("0x0000000348c4b9c5c7630de7455e09544fc8d9a08e8993b9c9b23b8dd7b08852")},
                {643475, uint256S("0x00000000fb5585002893839a4c5ff3802282db604f7dcc68a27d1454177eeecd")},
                {648138, uint256S("0x000000014ea5d7dad52630244d55f82f399d376afabab1a8133f7b12f4dfc2d7")},
                {651412, uint256S("0x00000002cc514441a2ccc85bba29bbf1e1974b9e3a1b8096e9ecba5273fb21b1")},
                {655641, uint256S("0x000000019c49152f13a2fab0ebcaed5eda3681f036b888d4a44d81bc3c136105")},
                {658202, uint256S("0x000000035596b55b9b68c44b837c5d0586657ce507161360ced7b4fe030dd426")},
                {663795, uint256S("0x00000002406033bf973316b72b90e72283b5c2b383fce7a93971cfb482e3ac92")},
                {666768, uint256S("0x00000001cb60ea1eb7074733a4489773149fb79ed24258b93c941db85cf9a9e3")},
                {672726, uint256S("0x0000000025ff80fb65fa75aa6a146dd071f96c52f06b60f9a2e471c0635df6ff")},
                {675576, uint256S("0x00000002ef4c408162f3454f6872579d4244c96696d782f7ee06dde7c092d11e")},
                {680076, uint256S("0x000000000454748939278b84a1f57a38c41780da7c30fce4ec7a82bf155147f4")},
                {682293, uint256S("0x000000079cc52db33fb00ab3364743b7763dbf5edd181d5c17f490dc7ad89c44")},
                {685420, uint256S("0x000000018acb9e7e75539e4d31fc114c20b6cc893a73f17f213e884404379953")},
                {689289, uint256S("0x000000001636e6e77b1b465494d563b68f121e441dd9b82c5c56da3d78dac062")},
                {692577, uint256S("0x000000029db1e84453974231ababb6173e440142e68e7109ae19f0a9c9d17af8")},
                {698096, uint256S("0x00000003b0ee86239b57e973ea1bebec897edc423f9d7eb3f90d203e062ffc44")},
                {702169, uint256S("0x0000000127450ea257151768bef7d8bb32e0fd3c1c85262c18ddfdc7ea60ab0b")},
                {707938, uint256S("0x000000041894e6d1199f77241f56e23ecfcb665ac7ff8dd109c43b852b670af1")},
                {710024, uint256S("0x000000040bdda51c32fb48b388c04be659eaf9dcb7d192a80568106088a3e768")},
                {714538, uint256S("0x00000003cf592b0b982e01dfeddf06d0cc0ba75e88c185704e24625bd164c345")},
                {719191, uint256S("0x000000039fdb1898dd9aafa5c33a6aa4a91d8e5473f2476d9ca8c749a1b847c2")},
                {725101, uint256S("0x000000014d2f8b413cbb137f76319a4c9f6203ffd0a08c0ceaac6f859805d898")},
                {727785, uint256S("0x00000001ee990ea1daa6132102fd1b48cd28aebd070a24307eeaded0180256ca")},
                {733619, uint256S("0x00000000501157521c802551e34e0ee606347b7b9051ab535cd5fd251fa72615")},
                {739357, uint256S("0x000000030ed41167ab0ea66e25a1a32ded4cae6297ae47812205d14ceb97c110")},
                {741773, uint256S("0x000000050a8dc4e40f0ea88244f4c19e7d23b8e8acf7c55ec031b540e58da809")},
                {744278, uint256S("0x00000000fdbd9b735140b274e0e2fd616ef8efedbc6edf14f764aef20c739dd9")},
            }
        };
        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000001115a0298260b3f6d0ed73a1174aefcc337aaea997c79ce7e7c3d683a
            /* nTime    */ 1790048235,
            /* nTxCount */ 836911,
            /* dTxRate  */ 0.01450442100705602
        };
        
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // 1/100 of mainnet's halving interval. Every scheduled height below is the
        // mainnet height scaled by that ratio (rounded down); see doc/rincoin-parameters.md.
        consensus.nSubsidyHalvingInterval = 2100;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on testnet
        consensus.BIP34Height = ScaleMainnetHeight(26500, consensus); // 265
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = ScaleMainnetHeight(26500, consensus);
        consensus.BIP66Height = ScaleMainnetHeight(26500, consensus);
        consensus.CSVHeight = ScaleMainnetHeight(26500, consensus);
        consensus.SegwitHeight = ScaleMainnetHeight(26500, consensus);
        consensus.MinBIP9WarningHeight = consensus.SegwitHeight + 2016; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 33 * 60 * 60; // 33hour
        consensus.nPowTargetSpacing = 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.DGWHeight = ScaleMainnetHeight(30000, consensus); // 300; Dark Gravity Wave (DGW) difficulty adjustment algorithm
        // Height-840,000 transition at 4 intervals (8,400), like mainnet's 840,000.
        SetS6bSchedule(consensus);
        assert(consensus.nS6bHeight == 8400);
        assert(consensus.vS6bSubsidyPhases.back().nStartHeight == 2345875);
        // Peer-protocol-version floor schedule (height -> min version): 70017
        // MWEB-capable baseline from genesis, 70018 from the transition height.
        consensus.vMinPeerProtoVersionFloors = {{0, 70017}, {consensus.nS6bHeight, 70018}};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of Taproot (BIPs 340-342), scaled from mainnet; height-based
        // deployments flag-day activate by nTimeoutHeight even without signaling.
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        SetScaledDeploymentHeights(consensus, Consensus::DEPLOYMENT_TAPROOT, 2161152, 2370816); // 21,611 / 23,708 -> 20,160 / 22,176
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartHeight == 20160);
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeoutHeight == 22176);

        // Deployment of MWEB (LIP-0002, LIP-0003, and LIP-0004), scaled from mainnet
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        SetScaledDeploymentHeights(consensus, Consensus::DEPLOYMENT_MWEB, 2217600, 2427264); // 22,176 / 24,272 -> 22,176 / 24,192
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight == 22176);
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight == 24192);

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00009d5fbc8579e8b4292f1bab22437d9468c0cc615cb5b0242d8159b31760ad");

        consensus.mweb_pegout_feature_activation_height = 0;
        consensus.mweb_extradata_feature_activation_height = 0;
        pchMessageStart[0] = 0x72; // 'r'
        pchMessageStart[1] = 0x69; // 'i'
        pchMessageStart[2] = 0x6E; // 'n'
        pchMessageStart[3] = 0x74; // 't'
        nDefaultPort = 19555;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 4;
        m_assumed_chain_state_size = 1;

        genesis = CreateTestNetGenesisBlock(1743059000, 27864, 0x1f00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00009d5fbc8579e8b4292f1bab22437d9468c0cc615cb5b0242d8159b31760ad"));
        assert(genesis.hashMerkleRoot == uint256S("0x7a2a292324679fdd5b843a9daf72acc7b2801ab95321e863e545f69ced707b0e"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top

        base58Prefixes[PUBKEY_ADDRESS] = {65};  // "T...""
        base58Prefixes[SCRIPT_ADDRESS] = {127}; // "t...""
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,50);
        base58Prefixes[SECRET_KEY] =     {193}; // "7U.../7W..." (WIF)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};  // "tpub..."
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};  // "tprv..."

        bech32_hrp = "trin";
        mweb_hrp = "trmweb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0, uint256S("0x00009d5fbc8579e8b4292f1bab22437d9468c0cc615cb5b0242d8159b31760ad")}  //TODO put hashGenesisBlock value here
            }
        };

        chainTxData = ChainTxData{
            /* nTime    */ 1743059000, // It's OK to use the same timestamp as RinCoin's Genesis
            /* nTxCount */ 1,          // Only the Genesis coinbase
            /* dTxRate  */ 0.0         // Actually, there are no transactions yet
        };
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID =  CBaseChainParams::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // 1/1000 of mainnet's halving interval (upstream regtest uses 150). The
        // height-840,000 transition, the peer-version floor and the MWEB deployment
        // use the mainnet heights scaled by that ratio, so their order matches
        // mainnet. The BIP34/65/66/CSV/SegWit heights, always-active Taproot and the
        // disabled DGW below are the upstream regtest conventions that the inherited
        // test suite depends on, and are deliberately left alone.
        consensus.nSubsidyHalvingInterval = 210;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        consensus.CSVHeight = 432; // CSV activated on regtest (Used in rpc activation tests)
        consensus.SegwitHeight = 0; // SEGWIT is always activated on regtest unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.DGWHeight = std::numeric_limits<int>::max();  // Turns off Dark Gravity Wave (DGW) difficulty adjustment algorithm for regtest
        // Height-840,000 transition at 4 intervals (840), like mainnet's 840,000.
        SetS6bSchedule(consensus);
        assert(consensus.nS6bHeight == 840);
        assert(consensus.vS6bSubsidyPhases.back().nStartHeight == 234587);
        // Peer-protocol-version floor schedule (height -> min version): 70017
        // MWEB-capable baseline from genesis, 70018 from the transition height.
        consensus.vMinPeerProtoVersionFloors = {{0, 70017}, {consensus.nS6bHeight, 70018}};
        consensus.nPowTargetTimespan = 33 * 60 * 60; // 33hour
        consensus.nPowTargetSpacing = 60; // match mainnet spacing (regtest convention)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of MWEB (LIP-0002 and LIP-0003)
        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        // Height-based like mainnet and scaled from it, so MWEB activates after the
        // height-840,000 transition here too: STARTED at 2,160, LOCKED_IN at 2,304,
        // ACTIVE at 2,448. Tests that want MWEB earlier or never use -vbparams=mweb:...
        SetScaledDeploymentHeights(consensus, Consensus::DEPLOYMENT_MWEB, 2217600, 2427264); // 2,217 / 2,427 -> 2,160 / 2,304
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight == 2160);
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight == 2304);

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};
        consensus.mweb_pegout_feature_activation_height = 0;
        consensus.mweb_extradata_feature_activation_height = 0;
        // Regtest-only test vector so the upstream functional tests can exercise the rule.
        consensus.frozen_mweb_output_ids = GetRegTestFrozenMWEBOutputIDs();

        pchMessageStart[0] = 0x72; // 'r'
        pchMessageStart[1] = 0x72; // 'r'
        pchMessageStart[2] = 0x63; // 'c'
        pchMessageStart[3] = 0x74; // 't'
        nDefaultPort = 29555;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateActivationParametersFromArgs(args);

        genesis = CreateRegTestGenesisBlock(1743059120, 0, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x7d2c8c57ce2597f86c9fe41f9865ad664b04d2aad4321fdaab48ed3da1805fe7"));
        assert(genesis.hashMerkleRoot == uint256S("0xe3c12cbf8b33dc3a00cbe56699682fa6b2f7b03b981539dd079394df8315ff12"));
        
        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("0x7d2c8c57ce2597f86c9fe41f9865ad664b04d2aad4321fdaab48ed3da1805fe7")},  //TODO put hashGenesisBlock value here
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rrin";
        mweb_hrp = "rrmweb";
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nStartHeight, int64_t nTimeoutHeight)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
        consensus.vDeployments[d].nStartHeight = nStartHeight;
        consensus.vDeployments[d].nTimeoutHeight = nTimeoutHeight;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    if (args.IsArgSet("-segwitheight")) {
        int64_t height = args.GetArg("-segwitheight", consensus.SegwitHeight);
        if (height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for segwit is out of valid range. Use -1 to disable segwit.", height));
        } else if (height == -1) {
            LogPrintf("Segwit disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }
        consensus.SegwitHeight = static_cast<int>(height);
    }

    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() < 3 || 5 < vDeploymentParams.size()) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end[:heightstart:heightend]");
        }
        int64_t nStartTime, nTimeout;
        int64_t nStartHeight = 0, nTimeoutHeight = 0;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        if (vDeploymentParams.size() > 3 && !ParseInt64(vDeploymentParams[3], &nStartHeight)) {
            throw std::runtime_error(strprintf("Invalid nStartHeight (%s)", vDeploymentParams[3]));
        }
        if (vDeploymentParams.size() > 4 && !ParseInt64(vDeploymentParams[4], &nTimeoutHeight)) {
            throw std::runtime_error(strprintf("Invalid nTimeoutHeight (%s)", vDeploymentParams[4]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout, nStartHeight, nTimeoutHeight);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld, start_height=%d, timeout_height=%d\n", vDeploymentParams[0], nStartTime, nTimeout, nStartHeight, nTimeoutHeight);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}


/**
 * Preview network (publicly reachable rehearsal chain).
 *
 * Real proof of work as on testnet (mainnet's powLimit and block spacing, DGW
 * retargeting) with the compressed schedule of regtest: every scheduled height
 * is the mainnet height divided by 1,000, and the versionbits window is the
 * regtest one. It has its own genesis block, message start, ports and address
 * prefixes. A rehearsal that needs a fresh chain gets a new genesis block in a
 * new build: a fresh chain on the same genesis block would lose against any
 * node that kept the earlier, longer one.
 */
class CPreviewParams : public CChainParams {
public:
    explicit CPreviewParams(const ArgsManager& /*args*/) {
        strNetworkID = CBaseChainParams::PREVIEW;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        // Public rehearsal network: 1/1000 of mainnet's halving interval (~3.5h per
        // interval). Every scheduled height below is the mainnet height scaled by
        // that ratio (rounded down); see doc/rincoin-parameters.md.
        consensus.nSubsidyHalvingInterval = 210;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = ScaleMainnetHeight(26500, consensus); // 26
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = ScaleMainnetHeight(26500, consensus);
        consensus.BIP66Height = ScaleMainnetHeight(26500, consensus);
        consensus.CSVHeight = ScaleMainnetHeight(26500, consensus);
        consensus.SegwitHeight = ScaleMainnetHeight(26500, consensus);
        consensus.MinBIP9WarningHeight = consensus.SegwitHeight + 144; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 33 * 60 * 60; // 33h
        consensus.nPowTargetSpacing = 60;
        // (Without effect here, as on testnet: the difficulty is the powLimit until DGW
        // takes over, and DGW does not know the min-difficulty rule.)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // as on regtest
        consensus.DGWHeight = ScaleMainnetHeight(30000, consensus); // 30
        // Height-840,000 transition at 4 intervals (840), like mainnet's 840,000.
        SetS6bSchedule(consensus);
        assert(consensus.nS6bHeight == 840);
        assert(consensus.vS6bSubsidyPhases.back().nStartHeight == 234587);
        // Peer-protocol-version floor schedule (height -> min version): 70017
        // MWEB-capable baseline from genesis, 70018 from the transition height.
        consensus.vMinPeerProtoVersionFloors = {{0, 70017}, {consensus.nS6bHeight, 70018}};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Taproot/MWEB — scaled from mainnet, so both come after the height-840,000
        // transition as they do on mainnet. Both round down to the same windows here, as
        // MWEB does on regtest: STARTED at 2,160, LOCKED_IN at 2,304, ACTIVE at 2,448.
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        SetScaledDeploymentHeights(consensus, Consensus::DEPLOYMENT_TAPROOT, 2161152, 2370816); // 2,161 / 2,370 -> 2,160 / 2,304
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartHeight == 2160);
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeoutHeight == 2304);

        consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].bit = 4;
        SetScaledDeploymentHeights(consensus, Consensus::DEPLOYMENT_MWEB, 2217600, 2427264); // 2,217 / 2,427 -> 2,160 / 2,304
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nStartHeight == 2160);
        assert(consensus.vDeployments[Consensus::DEPLOYMENT_MWEB].nTimeoutHeight == 2304);

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00004282aaa888c5b7a1bb210464788510d3c5976a8cec49061a3eb49d04ff33"); // preview genesis

        consensus.mweb_pegout_feature_activation_height = 0;
        consensus.mweb_extradata_feature_activation_height = 0;

        pchMessageStart[0] = 0x72; // 'r'
        pchMessageStart[1] = 0x69; // 'i'
        pchMessageStart[2] = 0x6E; // 'n'
        pchMessageStart[3] = 0x70; // 'p'
        nDefaultPort = 49555;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 4;
        m_assumed_chain_state_size = 1;

        // Preview has its own genesis block (it used to reuse testnet's). Wallets,
        // Electrum-style servers and explorers identify a chain by its genesis
        // hash, and the two networks have different consensus parameters.
        genesis = CreatePreviewGenesisBlock(1789862400, 104436, 0x1f00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00004282aaa888c5b7a1bb210464788510d3c5976a8cec49061a3eb49d04ff33"));
        assert(genesis.hashMerkleRoot == uint256S("0x687b2f9d3bbd319b5d841fa72f2269657606a777d289f1b7f612cc8396de7843"));

        vFixedSeeds.clear();
        vSeeds.clear();

        // Base58 prefixes picked one above neighbouring coins to avoid
        // address-format collisions (Peercoin uses 55/117). The 4-byte
        // extended-key prefixes were brute-forced from the BIP32 layout so
        // that every encodable payload yields the desired 4-char string.
        base58Prefixes[PUBKEY_ADDRESS]  = std::vector<unsigned char>(1, 56);   // "P..."
        base58Prefixes[SCRIPT_ADDRESS]  = std::vector<unsigned char>(1, 118);  // "p..."
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1, 50);
        base58Prefixes[SECRET_KEY]      = std::vector<unsigned char>(1, 184);  // "7A.../7C..." (WIF)
        base58Prefixes[EXT_PUBLIC_KEY]  = {0x03, 0xE2, 0x5D, 0x80};            // "ppub..."
        base58Prefixes[EXT_SECRET_KEY]  = {0x03, 0xE2, 0x59, 0x46};            // "pprv..."

        bech32_hrp = "prin";
        mweb_hrp   = "prmweb";

        // No fixed seeds: preview is a short-lived rehearsal network whose nodes are announced ad hoc.
        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
                {0, uint256S("0x00004282aaa888c5b7a1bb210464788510d3c5976a8cec49061a3eb49d04ff33")}
            }
        };

        chainTxData = ChainTxData{
            /* nTime    */ 1789862400,
            /* nTxCount */ 1,
            /* dTxRate  */ 0.0
        };
    }
};

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN) {
        return std::unique_ptr<CChainParams>(new CMainParams());
    } else if (chain == CBaseChainParams::TESTNET) {
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    } else if (chain == CBaseChainParams::SIGNET) {
        return std::unique_ptr<CChainParams>(new CTestNetParams()); // TODO: Support SigNet
    } else if (chain == CBaseChainParams::REGTEST) {
        return std::unique_ptr<CChainParams>(new CRegTestParams(args));
    } else if (chain == CBaseChainParams::PREVIEW) {
        return std::unique_ptr<CChainParams>(new CPreviewParams(args));
    }
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(gArgs, network);
}
