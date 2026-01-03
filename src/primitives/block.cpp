// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <crypto/common.h>
#include "crypto/rinhash.h"

uint256 CBlockHeader::GetHash() const
{
    // Both GetHash() and GetPoWHash() return the same RinHash result
    // Use the cached version to avoid recomputation
    return GetPoWHash();
}

uint256 CBlockHeader::GetPoWHash() const
{
    // Check if we have a valid cached hash
    if (m_hashCacheValid) {
        return m_cachedPoWHash;
    }
    
    // Compute the expensive RinHash and cache it
    m_cachedPoWHash = RinHash(*this);
    m_hashCacheValid = true;
    
    return m_cachedPoWHash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

CTransactionRef CBlock::GetHogEx() const noexcept
{
    if (vtx.size() >= 2 && vtx.back()->IsHogEx()) {
        assert(!vtx.back()->vout.empty());
        return vtx.back();
    }

    return nullptr;
}