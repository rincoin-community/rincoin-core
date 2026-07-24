// Copyright (c) 2024-2025 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef RINHASH_H
#define RINHASH_H

#include "uint256.h"
#include "primitives/block.h"

//! Compute RinHash (BLAKE3 -> Argon2d -> SHA3-256) for a block header using the
//! network's fixed Argon2d parameters (t_cost=2, m_cost=64, lanes=1,
//! salt="RinCoinSalt").
uint256 RinHash(const CBlockHeader& block);

#endif // RINHASH_H
