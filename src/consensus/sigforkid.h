// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_SIGFORKID_H
#define BITCOIN_CONSENSUS_SIGFORKID_H

#include <cstdint>

/**
 * Fork ID of the replay-protected signature hash that applies from the
 * height-840,000 transition onward (Consensus::Params::nS6bHeight).
 *
 * The scheme is the one Bitcoin Cash introduced and Bitcoin Gold uses on a
 * SegWit chain: a signature sets SIGHASH_FORKID (0x40) in its hash type, the
 * digest of every ECDSA-signed input (pre-SegWit and SegWit v0 alike) is
 * computed with the BIP143 algorithm, and the four-byte hash type that ends the
 * BIP143 preimage carries the fork ID in its upper three bytes:
 *
 *     hash_type_in_preimage = hash_type_byte | (fork_id << 8)
 *
 * The fork ID is a 24-bit number. It must not be zero (with zero a SegWit v0
 * signature would also verify under the historical rules) and it stays below
 * 2^23 so that the shifted value is positive in a signed 32-bit integer. The
 * value lives in Consensus::Params::sigForkId and is identical on every network.
 */
using SigForkId = uint32_t;

/** The fork ID of the height-840,000 transition: 840 (0x000348). With
 *  SIGHASH_ALL | SIGHASH_FORKID the preimage ends in the bytes 41 48 03 00. */
static constexpr SigForkId SIG_FORK_ID_840K{840};

#endif // BITCOIN_CONSENSUS_SIGFORKID_H
