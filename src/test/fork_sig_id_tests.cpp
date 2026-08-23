// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/fork_commitment.h>
#include <key.h>
#include <pubkey.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace {

CMutableTransaction MakeSimpleSpend()
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = InsecureRand256();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1000;
    tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    return tx;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(fork_sig_id_tests, BasicTestingSetup)

// The single most important invariant of this change: a caller that never
// calls SetSigForkId() (i.e. every pre-existing call site, and every
// existing unit test) must see byte-identical SignatureHash() output to a
// build without sig_fork_id at all. cache=nullptr, and a cache with
// m_sig_fork_id_active left at its default (false), must agree.
BOOST_AUTO_TEST_CASE(inactive_forkid_is_byte_identical_to_no_cache)
{
    CMutableTransaction tx = MakeSimpleSpend();
    CScript scriptCode = CScript() << OP_TRUE;

    uint256 hash_no_cache = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, nullptr);

    PrecomputedTransactionData inactive_cache;
    BOOST_CHECK(!inactive_cache.m_sig_fork_id_active); // default
    uint256 hash_inactive_cache = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, &inactive_cache);

    BOOST_CHECK_EQUAL(hash_no_cache.ToString(), hash_inactive_cache.ToString());

    // Same invariant for the WITNESS_V0 branch, which previously ignored
    // `cache` entirely when m_bip143_segwit_ready was false.
    uint256 hash_no_cache_wv0 = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 1000, SigVersion::WITNESS_V0, nullptr);
    uint256 hash_inactive_cache_wv0 = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 1000, SigVersion::WITNESS_V0, &inactive_cache);
    BOOST_CHECK_EQUAL(hash_no_cache_wv0.ToString(), hash_inactive_cache_wv0.ToString());
}

BOOST_AUTO_TEST_CASE(active_forkid_changes_the_digest)
{
    CMutableTransaction tx = MakeSimpleSpend();
    CScript scriptCode = CScript() << OP_TRUE;
    std::array<unsigned char, 8> sig_fork_id = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}};

    PrecomputedTransactionData active_cache;
    active_cache.SetSigForkId(sig_fork_id, true);

    uint256 hash_plain = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, nullptr);
    uint256 hash_forkid = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, &active_cache);
    BOOST_CHECK(hash_plain != hash_forkid);

    uint256 hash_plain_wv0 = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 1000, SigVersion::WITNESS_V0, nullptr);
    uint256 hash_forkid_wv0 = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 1000, SigVersion::WITNESS_V0, &active_cache);
    BOOST_CHECK(hash_plain_wv0 != hash_forkid_wv0);
}

BOOST_AUTO_TEST_CASE(different_sig_fork_id_values_diverge)
{
    CMutableTransaction tx = MakeSimpleSpend();
    CScript scriptCode = CScript() << OP_TRUE;

    PrecomputedTransactionData cache_a;
    cache_a.SetSigForkId({{1, 1, 1, 1, 1, 1, 1, 1}}, true);
    PrecomputedTransactionData cache_b;
    cache_b.SetSigForkId({{2, 2, 2, 2, 2, 2, 2, 2}}, true);

    uint256 hash_a = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, &cache_a);
    uint256 hash_b = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, &cache_b);
    BOOST_CHECK(hash_a != hash_b);
}

// End-to-end: a signature computed against the active-forkid digest must
// verify against that exact digest, and must NOT verify against the
// inactive (pre-fork-shaped) digest for the same transaction -- this is the
// core "old-style signature rejected post-H1, new-style required" property,
// exercised directly at the ECDSA layer rather than through full mempool
// plumbing.
BOOST_AUTO_TEST_CASE(signature_bound_to_forkid_activation_state)
{
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();

    CMutableTransaction tx = MakeSimpleSpend();
    CScript scriptCode = CScript() << OP_TRUE;

    PrecomputedTransactionData active_cache;
    active_cache.SetSigForkId(ForkCommitment::ComputeForkSigId({{0x11}}, 1, 1), true);

    uint256 hash_active = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, &active_cache);
    uint256 hash_inactive = SignatureHash(scriptCode, CTransaction(tx), 0, SIGHASH_ALL, 0, SigVersion::BASE, nullptr);
    BOOST_CHECK(hash_active != hash_inactive);

    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash_active, sig));

    // Verifies against the digest it was actually signed over...
    BOOST_CHECK(pubkey.Verify(hash_active, sig));
    // ...but not against the differently-shaped (inactive-forkid) digest for
    // the same transaction, which is exactly what makes an old-style
    // signature fail post-H1 (and a post-H1 signature fail pre-H1/on a
    // foreign chain) without any special-casing needed.
    BOOST_CHECK(!pubkey.Verify(hash_inactive, sig));
}

BOOST_AUTO_TEST_SUITE_END()
