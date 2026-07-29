// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/feerate.h>

#include <tinyformat.h>

static const CAmount BASE_MWEB_FEE = 100;

// Fee charged for a given MWEB weight. Saturates to INT64_MAX instead of
// overflowing (which is undefined behaviour) on pathological weights; real
// MWEB weights are many orders of magnitude below the saturation bound, so
// this never changes the result for valid transactions.
static CAmount MWEBFee(uint64_t mweb_weight)
{
    if (mweb_weight > uint64_t(std::numeric_limits<int64_t>::max() / BASE_MWEB_FEE)) {
        return std::numeric_limits<int64_t>::max();
    }
    return CAmount(mweb_weight) * BASE_MWEB_FEE;
}

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nBytes_, uint64_t mweb_weight)
    : m_nFeePaid(nFeePaid), m_nBytes(nBytes_), m_weight(mweb_weight)
{
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    assert(mweb_weight <= uint64_t(std::numeric_limits<int64_t>::max()));

    CAmount mweb_fee = MWEBFee(mweb_weight);
    if (mweb_fee > 0 && nFeePaid < mweb_fee) {
        nSatoshisPerK = 0;
    } else {
        CAmount ltc_fee = (nFeePaid - mweb_fee);

        int64_t nSize = int64_t(nBytes_);
        if (nSize > 0)
            nSatoshisPerK = ltc_fee * 1000 / nSize;
        else
            nSatoshisPerK = 0;
    }
}

CAmount CFeeRate::GetFee(size_t nBytes_) const
{
    assert(nBytes_ <= uint64_t(std::numeric_limits<int64_t>::max()));
    int64_t nSize = int64_t(nBytes_);

    CAmount nFee = nSatoshisPerK * nSize / 1000;

    if (nFee == 0 && nSize != 0) {
        if (nSatoshisPerK > 0)
            nFee = CAmount(1);
        if (nSatoshisPerK < 0)
            nFee = CAmount(-1);
    }

    return nFee;
}

CAmount CFeeRate::GetMWEBFee(uint64_t mweb_weight) const
{
    assert(mweb_weight <= uint64_t(std::numeric_limits<int64_t>::max()));
    return MWEBFee(mweb_weight);
}

CAmount CFeeRate::GetTotalFee(size_t nBytes, uint64_t mweb_weight) const
{
    return GetFee(nBytes) + GetMWEBFee(mweb_weight);
}

bool CFeeRate::MeetsFeePerK(const CAmount& min_fee_per_k) const
{
    // (mweb_weight * BASE_MWEB_FEE) litoshis are required as fee for MWEB transactions.
    // Anything beyond that can be used to calculate nSatoshisPerK.
    CAmount mweb_fee = MWEBFee(m_weight);
    if (m_weight > 0 && m_nFeePaid < mweb_fee) {
        return false;
    }

    // MWEB-to-MWEB transactions don't have a size to calculate nSatoshisPerK.
    // Since we got this far, we know the transaction meets the minimum MWEB fee, so return true.
    if (m_nBytes == 0 && m_weight > 0) {
        return true;
    }

    return nSatoshisPerK >= min_fee_per_k;
}

std::string CFeeRate::ToString(const FeeEstimateMode& fee_estimate_mode) const
{
    switch (fee_estimate_mode) {
    case FeeEstimateMode::SAT_VB: return strprintf("%d.%03d %s/vB", nSatoshisPerK / 1000, nSatoshisPerK % 1000, CURRENCY_ATOM);
    default:                      return strprintf("%d.%08d %s/kvB", nSatoshisPerK / COIN, nSatoshisPerK % COIN, CURRENCY_UNIT);
    }
}
