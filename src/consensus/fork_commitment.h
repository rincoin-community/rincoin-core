// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_FORK_COMMITMENT_H
#define BITCOIN_CONSENSUS_FORK_COMMITMENT_H

// Height-840,000-style fork branch marker
// (rincoin-consensus840k/technology/consensus-transition.md §5).
//
// From Consensus::Params::ForkH1Height onward, exactly one coinbase output
// must carry a 30-byte `OP_RETURN <28-byte payload>` script identifying this
// build's branch/fork/scenario:
//
//   offset  size  field            value
//   0       4     magic            "RINF"
//   4       1     format_version   0x01
//   5       16    branch_id        opaque, ForkBranchId
//   21      4     fork_no          ForkNo, big-endian
//   25      2     scenario_id      ForkScenarioId, big-endian
//   27      1     flags            0x00
//
// This coexists with, and is unrelated to, the BIP141 witness commitment.
// Zero matches, more than one match (even if one of them is correct), or any
// byte mismatch in the single match all fail contextual block validation
// (ValidateForkCommitment(), called from ContextualCheckBlock() in
// validation.cpp).

#include <consensus/params.h>
#include <consensus/validation.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace ForkCommitment {

constexpr unsigned char MAGIC[4] = {'R', 'I', 'N', 'F'};
constexpr size_t PAYLOAD_LEN = 28;
constexpr unsigned char CURRENT_FORMAT_VERSION = 1;

/** SHA256(branch_id || fork_no_BE(4) || scenario_id_BE(2))[:8]. Computed
 *  once per CChainParams construction (chainparams.cpp), never per-sighash. */
inline std::array<unsigned char, 8> ComputeForkSigId(const std::array<unsigned char, 16>& branch_id,
                                                       uint32_t fork_no, uint16_t scenario_id)
{
    unsigned char preimage[16 + 4 + 2];
    memcpy(preimage, branch_id.data(), 16);
    preimage[16] = static_cast<unsigned char>(fork_no >> 24);
    preimage[17] = static_cast<unsigned char>(fork_no >> 16);
    preimage[18] = static_cast<unsigned char>(fork_no >> 8);
    preimage[19] = static_cast<unsigned char>(fork_no);
    preimage[20] = static_cast<unsigned char>(scenario_id >> 8);
    preimage[21] = static_cast<unsigned char>(scenario_id);

    unsigned char digest[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(preimage, sizeof(preimage)).Finalize(digest);

    std::array<unsigned char, 8> sig_id{};
    memcpy(sig_id.data(), digest, sig_id.size());
    return sig_id;
}

inline std::vector<unsigned char> BuildForkCommitmentPayload(const Consensus::Params& params,
                                                               unsigned char flags = 0)
{
    std::vector<unsigned char> payload;
    payload.reserve(PAYLOAD_LEN);
    payload.insert(payload.end(), MAGIC, MAGIC + 4);
    payload.push_back(CURRENT_FORMAT_VERSION);
    payload.insert(payload.end(), params.ForkBranchId.begin(), params.ForkBranchId.end());
    payload.push_back(static_cast<unsigned char>(params.ForkNo >> 24));
    payload.push_back(static_cast<unsigned char>(params.ForkNo >> 16));
    payload.push_back(static_cast<unsigned char>(params.ForkNo >> 8));
    payload.push_back(static_cast<unsigned char>(params.ForkNo));
    payload.push_back(static_cast<unsigned char>(params.ForkScenarioId >> 8));
    payload.push_back(static_cast<unsigned char>(params.ForkScenarioId));
    payload.push_back(flags);
    assert(payload.size() == PAYLOAD_LEN);
    return payload;
}

/** OP_RETURN + canonical minimal push of the 28-byte payload (6a1c...). */
inline CScript BuildForkCommitmentScript(const Consensus::Params& params)
{
    return CScript() << OP_RETURN << BuildForkCommitmentPayload(params);
}

/** Namespace discovery: does this scriptPubKey push data beginning with the
 *  RINF magic right after OP_RETURN? Deliberately permissive about the rest
 *  of the script (non-minimal push, wrong length, trailing data) -- those
 *  are ValidateForkCommitment()'s job to reject, but a namespace match must
 *  still be counted for duplicate detection even when malformed, so an
 *  attacker cannot hide a bad commitment from the duplicate rule by
 *  malforming it. */
inline bool LooksLikeForkCommitment(const CScript& scriptPubKey)
{
    if (scriptPubKey.empty() || scriptPubKey[0] != OP_RETURN) return false;
    CScript::const_iterator pc = scriptPubKey.begin() + 1;
    opcodetype opcode;
    std::vector<unsigned char> data;
    if (!scriptPubKey.GetOp(pc, opcode, data)) return false;
    if (data.size() < 4) return false;
    return memcmp(data.data(), MAGIC, 4) == 0;
}

/** Scan every coinbase output for the RINF namespace. Returns {count, index}
 *  where index is the single match's output index if count == 1, else -1. */
inline std::pair<int, int> FindForkCommitment(const CTransaction& coinbase)
{
    int count = 0;
    int index = -1;
    for (size_t o = 0; o < coinbase.vout.size(); o++) {
        if (LooksLikeForkCommitment(coinbase.vout[o].scriptPubKey)) {
            count++;
            index = static_cast<int>(o);
        }
    }
    return std::make_pair(count, count == 1 ? index : -1);
}

/** Full byte-exact validation of the coinbase's fork commitment. A no-op
 *  (always valid) below Consensus::Params::ForkH1Height. */
inline bool ValidateForkCommitment(const CBlock& block, const Consensus::Params& params,
                                    int nHeight, BlockValidationState& state)
{
    if (nHeight < params.ForkH1Height) return true;
    if (block.vtx.empty()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-missing", "block has no transactions");
    }

    const CTransaction& coinbase = *block.vtx[0];
    const std::pair<int, int> match = FindForkCommitment(coinbase);
    const int count = match.first;
    const int index = match.second;
    if (count == 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-missing", "no RINF commitment output found");
    }
    if (count > 1) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-duplicate", "more than one RINF commitment output found");
    }

    const CTxOut& out = coinbase.vout[index];
    if (out.nValue != 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "commitment output value must be zero");
    }

    CScript::const_iterator pc = out.scriptPubKey.begin() + 1;
    opcodetype opcode;
    std::vector<unsigned char> data;
    out.scriptPubKey.GetOp(pc, opcode, data); // already proven to succeed by LooksLikeForkCommitment
    if (pc != out.scriptPubKey.end()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "trailing data after commitment push");
    }
    if (opcode != data.size()) {
        // Canonical encoding for a <=75-byte push is a direct length-prefixed
        // push (opcode == length); OP_PUSHDATA1/2/4 here is non-minimal.
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "non-minimal push encoding");
    }
    if (data.size() != PAYLOAD_LEN) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "wrong payload length");
    }
    if (data[4] != CURRENT_FORMAT_VERSION) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "wrong format version");
    }
    if (!std::equal(params.ForkBranchId.begin(), params.ForkBranchId.end(), data.begin() + 5)) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-wrong-branch", "wrong branch_id");
    }
    const uint32_t fork_no = (uint32_t(data[21]) << 24) | (uint32_t(data[22]) << 16) |
                              (uint32_t(data[23]) << 8) | uint32_t(data[24]);
    if (fork_no != params.ForkNo) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-wrong-fork-no", "wrong fork_no");
    }
    const uint16_t scenario_id = static_cast<uint16_t>((uint16_t(data[25]) << 8) | uint16_t(data[26]));
    if (scenario_id != params.ForkScenarioId) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-wrong-scenario", "wrong scenario_id");
    }
    if (data[27] != 0) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-fork-commitment-malformed", "nonzero flags byte");
    }
    return true;
}

} // namespace ForkCommitment

#endif // BITCOIN_CONSENSUS_FORK_COMMITMENT_H
