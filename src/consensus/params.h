// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <amount.h>
#include <consensus/sigforkid.h>
#include <uint256.h>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_TAPROOT, // Deployment of Schnorr/Taproot (BIPs 340-342)
    DEPLOYMENT_MWEB, // Deployment of MWEB (LIPs 0002-0004)
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime = 0;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout = 0;
    /** Start block height for version bits miner confirmation. Should be a retarget block, can be in the past */
    int64_t nStartHeight = 0;
    /** Timeout/expiry block height for the deployment attempt. Should be a retarget block. */
    int64_t nTimeoutHeight = 0;

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;

    /** Special value for nStartTime indicating that the deployment is never active.
     *  This is useful for integrating the code changes for a new feature
     *  prior to deploying it on some or all networks. */
    static constexpr int64_t NEVER_ACTIVE = -2;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height at which BIP16 becomes active */
    int BIP16Height;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;
    /** Block height at which Segwit (BIP141, BIP143 and BIP147) becomes active.
     * Note that segwit v0 script rules are enforced on all blocks except the
     * BIP 16 exception blocks. */
    int SegwitHeight;
    /** Don't warn about unknown BIP 9 activations below this height.
     * This prevents us from warning about the CSV and segwit activations. */
    int MinBIP9WarningHeight;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    /** The best chain should have at least this much work */
    uint256 nMinimumChainWork;
    /** By default assume that the signatures in ancestors of this block are valid */
    uint256 defaultAssumeValid;

    /** Optional one-block grandfather for the known MWEB input-metadata exploit. */
    uint256 mweb_input_metadata_grandfather_blockhash;

    /** MWEB kernels signaling pegouts must contain at least one pegout at and after this height. */
    int mweb_pegout_feature_activation_height{0};

    /** MWEB kernels signaling extra data must contain non-empty extra data at and after this height. */
    int mweb_extradata_feature_activation_height{0};

    /** Frozen MWEB output IDs that may not be spent. */
    std::vector<uint256> frozen_mweb_output_ids;

    /**
     * If true, witness commitments contain a payload equal to a Bitcoin Script solution
     * to the signet challenge. See BIP325.
     */
    bool signet_blocks{false};
    std::vector<uint8_t> signet_challenge;
    int DGWHeight;

    /**
     * Peer-protocol-version floor schedule: a height-sorted list of
     * {activation_height, min_version} pairs. From each activation_height
     * onward, peers advertising a protocol version below the associated
     * min_version are disconnected during the version handshake. This lets the
     * floor be raised at successive heights as the protocol is bumped over time.
     * An empty schedule disables the floor. This is a networking policy and does
     * not affect block validity.
     */
    std::vector<std::pair<int, int>> vMinPeerProtoVersionFloors;

    /**
     * Height-840,000 transition (S6/b). From nS6bHeight onward:
     *  - GetBlockSubsidy() follows vS6bSubsidyPhases instead of the historical
     *    halving rule (src/validation.cpp);
     *  - the block at exactly nS6bHeight must claim the full subsidy plus all
     *    fees, no more and no less (ConnectBlock() in src/validation.cpp);
     *  - every ECDSA signature (pre-SegWit and SegWit v0) must set SIGHASH_FORKID and
     *    is hashed with the BIP143 algorithm with sigForkId in the hash type
     *    (src/script/interpreter.cpp, src/consensus/sigforkid.h).
     * Everything below nS6bHeight validates exactly as before. A network
     * without the transition keeps the default (never).
     *
     * The mainnet height is 4 halving intervals (840,000). Test networks use the
     * same multiples of their own interval; see SetS6bSchedule() in
     * src/chainparams.cpp and doc/rincoin-parameters.md.
     */
    int nS6bHeight{std::numeric_limits<int>::max()};

    /** Fork ID of the replay-protected signature hash that applies from nS6bHeight. */
    SigForkId sigForkId{SIG_FORK_ID_840K};

    /**
     * One entry of the S6/b subsidy table: from nStartHeight (inclusive) until
     * the next entry's nStartHeight (exclusive; forever for the last entry) the
     * maximum block subsidy is nSubsidy. Entries are sorted ascending, the first
     * one starts at nS6bHeight and the last one has nSubsidy == 0 (the terminal
     * height derived from the issuance ceiling).
     */
    struct S6bSubsidyPhase {
        int nStartHeight;
        CAmount nSubsidy;
    };
    std::vector<S6bSubsidyPhase> vS6bSubsidyPhases;

    /** Protocol-version floor in effect at nHeight (0 if none). Entries must be
     *  sorted ascending by activation height. */
    int MinPeerProtoVersionFloorAt(int nHeight) const
    {
        int floor = 0;
        for (const auto& entry : vMinPeerProtoVersionFloors) {
            if (nHeight >= entry.first) floor = entry.second; else break;
        }
        return floor;
    }
};

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
