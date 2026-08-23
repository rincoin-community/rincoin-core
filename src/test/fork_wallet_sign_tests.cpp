// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Regression coverage for the wallet-signing side of sig_fork_id
// (consensus/s1-testing, B.4). Two real bugs were found only by running
// end-to-end functional tests, not by inspection:
//   1. The wallet never called the forkid-aware signing path at all, so it
//      could not spend funds post-fork.
//   2. Once fixed, SignTransaction()'s own internal self-verification step
//      (script/sign.cpp) checked freshly-created forkid-active signatures
//      against the forkid-*inactive* digest, so it rejected its own
//      correct signatures ("Signing transaction failed").
// These tests pin both fixes at the unit level so a future change can't
// silently reintroduce either one.

#include <chainparams.h>
#include <key.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(fork_wallet_sign_tests, BasicTestingSetup)

namespace {

struct SimpleSpend {
    CKey key;
    CScript scriptPubKey;
    CMutableTransaction tx;
};

SimpleSpend MakeSimpleSpend()
{
    SimpleSpend s;
    s.key.MakeNewKey(true);
    s.scriptPubKey = GetScriptForDestination(PKHash(s.key.GetPubKey()));

    s.tx.nVersion = 1;
    s.tx.vin.resize(1);
    s.tx.vin[0].prevout = COutPoint(InsecureRand256(), 0);
    s.tx.vout.resize(1);
    s.tx.vout[0].nValue = 1000;
    s.tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    return s;
}

} // namespace

// Regression guard: the forkid-aware MutableTransactionSignatureCreator
// constructor must produce a *different* signature than the default one
// (proves it actually mixed sig_fork_id into the sighash), and the default
// constructor's own output must be unaffected by the new constructor's
// existence (still byte-identical to a pre-branch build).
BOOST_AUTO_TEST_CASE(forkid_creator_changes_signature)
{
    SimpleSpend s = MakeSimpleSpend();
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    SignatureData sigdata_plain;
    MutableTransactionSignatureCreator plain_creator(&s.tx, 0, s.tx.vout[0].nValue, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, plain_creator, s.scriptPubKey, sigdata_plain));

    SignatureData sigdata_forkid;
    std::array<unsigned char, 8> sig_fork_id = {{1, 2, 3, 4, 5, 6, 7, 8}};
    MutableTransactionSignatureCreator forkid_creator(&s.tx, 0, s.tx.vout[0].nValue, sig_fork_id, /*sig_fork_id_active=*/true, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, forkid_creator, s.scriptPubKey, sigdata_forkid));

    BOOST_CHECK(sigdata_plain.scriptSig != sigdata_forkid.scriptSig);

    // And a forkid-aware creator with sig_fork_id_active=false must be
    // byte-identical to the plain constructor's output (same message
    // signed, modulo ECDSA's own randomness -- so compare via re-derivation
    // rather than raw bytes: both must verify against the same digest).
    SignatureData sigdata_forkid_inactive;
    MutableTransactionSignatureCreator inactive_creator(&s.tx, 0, s.tx.vout[0].nValue, sig_fork_id, /*sig_fork_id_active=*/false, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, inactive_creator, s.scriptPubKey, sigdata_forkid_inactive));

    const CTransaction txConst(s.tx);
    TransactionSignatureChecker checker_plain(&txConst, 0, s.tx.vout[0].nValue);
    CScript scriptSig_inactive(sigdata_forkid_inactive.scriptSig);
    // The inactive-forkid signature must verify under a plain (no-cache)
    // checker, exactly like the original, unmodified constructor's output.
    BOOST_CHECK(VerifyScript(scriptSig_inactive, s.scriptPubKey, nullptr, SCRIPT_VERIFY_NONE, checker_plain, nullptr));
}

// Regression guard for the specific "Signing transaction failed" bug: the
// free SignTransaction() function's own internal VerifyScript self-check
// must use the same sig_fork_id context the signature was just created
// with. If this regresses, a correctly-created forkid-active signature
// fails self-verification and SignTransaction() reports the input as an
// error even though the signature is valid.
BOOST_AUTO_TEST_CASE(sign_transaction_self_verification_matches_forkid_context)
{
    SimpleSpend s = MakeSimpleSpend();
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    Coin coin;
    coin.out.scriptPubKey = s.scriptPubKey;
    coin.out.nValue = s.tx.vout[0].nValue;
    coin.nHeight = 1;
    std::map<COutPoint, Coin> coins;
    coins[s.tx.vin[0].prevout] = coin;

    std::array<unsigned char, 8> sig_fork_id = {{9, 9, 9, 9, 9, 9, 9, 9}};
    std::map<int, std::string> input_errors;
    CMutableTransaction mtx{s.tx};
    bool complete = SignTransaction(mtx, &keystore, coins, SIGHASH_ALL, input_errors, &sig_fork_id, /*sig_fork_id_active=*/true);

    BOOST_CHECK(complete);
    BOOST_CHECK(input_errors.empty());

    // The resulting signature must actually verify under a forkid-active
    // checker (not just "SignTransaction thinks it's fine") -- i.e. this
    // isn't a case where both the signer and the buggy self-check agreed on
    // the wrong digest.
    PrecomputedTransactionData txdata;
    txdata.SetSigForkId(sig_fork_id, true);
    const CTransaction txConst(mtx);
    TransactionSignatureChecker checker(&txConst, 0, coin.out.nValue, txdata);
    BOOST_CHECK(VerifyScript(mtx.vin[0].scriptSig, s.scriptPubKey, &mtx.vin[0].scriptWitness, SCRIPT_VERIFY_NONE, checker, nullptr));
}

// Omitting sig_fork_id/sig_fork_id_active entirely (the default, used by
// every call site not explicitly updated for this branch) must produce a
// transaction that verifies under a plain, forkid-unaware checker --
// confirms every one of the newly-added optional parameters really do
// default to "unchanged from before this branch".
BOOST_AUTO_TEST_CASE(sign_transaction_omitted_forkid_is_unchanged)
{
    SimpleSpend s = MakeSimpleSpend();
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    Coin coin;
    coin.out.scriptPubKey = s.scriptPubKey;
    coin.out.nValue = s.tx.vout[0].nValue;
    coin.nHeight = 1;
    std::map<COutPoint, Coin> coins;
    coins[s.tx.vin[0].prevout] = coin;

    std::map<int, std::string> input_errors;
    CMutableTransaction mtx{s.tx};
    bool complete = SignTransaction(mtx, &keystore, coins, SIGHASH_ALL, input_errors);

    BOOST_CHECK(complete);
    BOOST_CHECK(input_errors.empty());

    const CTransaction txConst(mtx);
    TransactionSignatureChecker checker(&txConst, 0, coin.out.nValue);
    BOOST_CHECK(VerifyScript(mtx.vin[0].scriptSig, s.scriptPubKey, &mtx.vin[0].scriptWitness, SCRIPT_VERIFY_NONE, checker, nullptr));
}

BOOST_AUTO_TEST_SUITE_END()
