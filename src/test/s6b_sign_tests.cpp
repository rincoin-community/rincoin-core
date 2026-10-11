// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// The signing side of the replay-protected signature hash (SIGHASH_FORKID, height-840,000
// transition).
//
// Signing and checking have to agree on the signature-hash regime at every step, and
// a mismatch does not show up as a wrong signature but as a transaction that silently
// cannot be completed. These tests pin the places where the two meet:
//   - the wallet-facing creator adds SIGHASH_FORKID and signs the new digest where the
//     regime is in force, leaves the historical signature alone where it is not or is
//     not given, and never produces a SIGHASH_FORKID signature outside the regime;
//   - SignTransaction() verifies what it has just signed under the same regime, and
//     needs the amount of every spent output where the regime is in force;
//   - signatures made by another party -- a co-signer's signature in a partially
//     signed input, the partial signatures of a combined PSBT -- are recognized
//     under the regime they were made for, so that multi-party signing completes
//     on both sides of the transition.

#include <chainparams.h>
#include <key.h>
#include <policy/policy.h>
#include <psbt.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(s6b_sign_tests, BasicTestingSetup)

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

namespace {

/** The hash type byte of the signature in a P2PKH scriptSig (<signature> <public key>). */
int HashTypeOfFirstPush(const CScript& script_sig)
{
    CScript::const_iterator pc = script_sig.begin();
    opcodetype opcode;
    std::vector<unsigned char> sig;
    BOOST_REQUIRE(script_sig.GetOp(pc, opcode, sig) && !sig.empty());
    return sig.back();
}

} // namespace

// The regime-aware MutableTransactionSignatureCreator constructor must produce a
// replay-protected signature where the regime is in force, and the default
// constructor's output must stay the historical signature.
BOOST_AUTO_TEST_CASE(forkid_creator_changes_signature)
{
    SimpleSpend s = MakeSimpleSpend();
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    SignatureData sigdata_plain;
    MutableTransactionSignatureCreator plain_creator(&s.tx, 0, s.tx.vout[0].nValue, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, plain_creator, s.scriptPubKey, sigdata_plain));

    SignatureData sigdata_forkid;
    const SigForkId sig_fork_id = SIG_FORK_ID_840K;
    MutableTransactionSignatureCreator forkid_creator(&s.tx, 0, s.tx.vout[0].nValue, sig_fork_id, /*sig_fork_id_active=*/true, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, forkid_creator, s.scriptPubKey, sigdata_forkid));

    BOOST_CHECK(sigdata_plain.scriptSig != sigdata_forkid.scriptSig);
    BOOST_CHECK_EQUAL(HashTypeOfFirstPush(sigdata_plain.scriptSig), SIGHASH_ALL);
    BOOST_CHECK_EQUAL(HashTypeOfFirstPush(sigdata_forkid.scriptSig), SIGHASH_ALL | SIGHASH_FORKID);
    // Asking for the flag explicitly makes no difference there.
    {
        SignatureData sigdata_explicit;
        MutableTransactionSignatureCreator explicit_creator(&s.tx, 0, s.tx.vout[0].nValue, sig_fork_id, /*sig_fork_id_active=*/true, SIGHASH_ALL | SIGHASH_FORKID);
        BOOST_REQUIRE(ProduceSignature(keystore, explicit_creator, s.scriptPubKey, sigdata_explicit));
        BOOST_CHECK_EQUAL(HashTypeOfFirstPush(sigdata_explicit.scriptSig), SIGHASH_ALL | SIGHASH_FORKID);
    }

    // And a forkid-aware creator with sig_fork_id_active=false must be
    // byte-identical to the plain constructor's output (same message
    // signed, modulo ECDSA's own randomness -- so compare via re-derivation
    // rather than raw bytes: both must verify against the same digest).
    SignatureData sigdata_forkid_inactive;
    MutableTransactionSignatureCreator inactive_creator(&s.tx, 0, s.tx.vout[0].nValue, sig_fork_id, /*sig_fork_id_active=*/false, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(keystore, inactive_creator, s.scriptPubKey, sigdata_forkid_inactive));
    BOOST_CHECK_EQUAL(HashTypeOfFirstPush(sigdata_forkid_inactive.scriptSig), SIGHASH_ALL);

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

    const SigForkId sig_fork_id = SIG_FORK_ID_840K;
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
// confirms every one of the optional parameters really does default to the
// historical behavior.
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

// A SIGHASH_FORKID signature is never produced outside the regime (below the transition
// height it would be a non-standard historical signature). Within it, SIGHASH_SINGLE for
// an input without a matching output is an ordinary signature, as under BIP143.
BOOST_AUTO_TEST_CASE(creator_and_the_flag)
{
    SimpleSpend s = MakeSimpleSpend();
    s.tx.vin.resize(2);
    s.tx.vin[1].prevout = COutPoint(InsecureRand256(), 1); // second input, but only one output
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    for (int hash_type : std::vector<int>{SIGHASH_ALL | SIGHASH_FORKID, SIGHASH_SINGLE | SIGHASH_FORKID | SIGHASH_ANYONECANPAY}) {
        SignatureData sigdata;
        MutableTransactionSignatureCreator plain_creator(&s.tx, 0, 1000, hash_type);
        BOOST_CHECK(!ProduceSignature(keystore, plain_creator, s.scriptPubKey, sigdata));
        MutableTransactionSignatureCreator inactive_creator(&s.tx, 0, 1000, SIG_FORK_ID_840K, /*sig_fork_id_active=*/false, hash_type);
        BOOST_CHECK(!ProduceSignature(keystore, inactive_creator, s.scriptPubKey, sigdata));
    }

    for (int hash_type : std::vector<int>{SIGHASH_SINGLE, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY}) {
        SignatureData sigdata;
        MutableTransactionSignatureCreator active_creator(&s.tx, 1, 1000, SIG_FORK_ID_840K, /*sig_fork_id_active=*/true, hash_type);
        BOOST_CHECK(ProduceSignature(keystore, active_creator, s.scriptPubKey, sigdata));
        BOOST_CHECK_EQUAL(HashTypeOfFirstPush(sigdata.scriptSig), hash_type | SIGHASH_FORKID);
    }
}

// Where the regime is in force the signature hash commits to the amount of every input,
// so signing without it must fail visibly instead of producing a wrong signature.
BOOST_AUTO_TEST_CASE(sign_transaction_needs_the_amount_when_active)
{
    SimpleSpend s = MakeSimpleSpend();
    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKeyPubKey(s.key, s.key.GetPubKey()));

    Coin coin;
    coin.out.scriptPubKey = s.scriptPubKey;
    coin.out.nValue = MAX_MONEY; // how the raw-transaction RPCs mark "amount not given"
    coin.nHeight = 1;
    std::map<COutPoint, Coin> coins;
    coins[s.tx.vin[0].prevout] = coin;

    {
        std::map<int, std::string> input_errors;
        CMutableTransaction mtx{s.tx};
        BOOST_CHECK(!SignTransaction(mtx, &keystore, coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, /*sig_fork_id_active=*/true));
        BOOST_REQUIRE_EQUAL(input_errors.count(0), 1U);
        BOOST_CHECK_EQUAL(input_errors[0], "Missing amount");
    }
    {
        // Below the transition height a pre-SegWit input does not need it, as ever.
        std::map<int, std::string> input_errors;
        CMutableTransaction mtx{s.tx};
        BOOST_CHECK(SignTransaction(mtx, &keystore, coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, /*sig_fork_id_active=*/false));
        BOOST_CHECK(input_errors.empty());
    }
}

namespace {

/** A 2-of-2 P2SH multisig output with one key per party, and a transaction spending it. */
struct TwoPartySpend {
    CKey key[2];
    CScript redeem_script;
    Coin coin;
    CMutableTransaction tx;
    std::map<COutPoint, Coin> coins;

    TwoPartySpend()
    {
        std::vector<CPubKey> pubkeys;
        for (CKey& k : key) {
            k.MakeNewKey(true);
            pubkeys.push_back(k.GetPubKey());
        }
        redeem_script = GetScriptForMultisig(2, pubkeys);
        coin.out.scriptPubKey = GetScriptForDestination(ScriptHash(redeem_script));
        coin.out.nValue = 5000;
        coin.nHeight = 1;

        tx.nVersion = 1;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(InsecureRand256(), 0);
        tx.vout.resize(1);
        tx.vout[0].nValue = 4000;
        tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        coins[tx.vin[0].prevout] = coin;
    }

    /** What one party knows: its own key and the redeem script. */
    void FillParty(int i, FillableSigningProvider& provider) const
    {
        provider.AddKeyPubKey(key[i], key[i].GetPubKey());
        provider.AddCScript(redeem_script);
    }
};

} // namespace

// A co-signer's signature is only carried over when it is recognized, and it is only
// recognized under the signature hash it was made for.
BOOST_AUTO_TEST_CASE(data_from_transaction_recognizes_signatures_of_the_given_regime)
{
    for (const bool active : {false, true}) {
        TwoPartySpend s;
        FillableSigningProvider first;
        s.FillParty(0, first);
        std::map<int, std::string> input_errors;
        CMutableTransaction mtx{s.tx};
        BOOST_CHECK(!SignTransaction(mtx, &first, s.coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, active));
        BOOST_CHECK(!mtx.vin[0].scriptSig.empty());

        const SignatureData right = DataFromTransaction(mtx, 0, s.coin.out, &SIG_FORK_ID_840K, active);
        BOOST_CHECK_EQUAL(right.signatures.size(), 1U);
        BOOST_CHECK(!right.complete);
        const SignatureData wrong = DataFromTransaction(mtx, 0, s.coin.out, &SIG_FORK_ID_840K, !active);
        BOOST_CHECK_EQUAL(wrong.signatures.size(), 0U);
        // Omitting the regime means the historical one
        const SignatureData omitted = DataFromTransaction(mtx, 0, s.coin.out);
        BOOST_CHECK_EQUAL(omitted.signatures.size(), active ? 0U : 1U);
    }
}

// Two parties signing one after the other: the second SignTransaction() has to keep the
// first party's signature, on both sides of the transition.
BOOST_AUTO_TEST_CASE(two_party_sign_transaction_completes)
{
    for (const bool active : {false, true}) {
        TwoPartySpend s;
        FillableSigningProvider first, second;
        s.FillParty(0, first);
        s.FillParty(1, second);
        std::map<int, std::string> input_errors;
        CMutableTransaction mtx{s.tx};
        BOOST_CHECK(!SignTransaction(mtx, &first, s.coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, active));
        input_errors.clear();
        BOOST_CHECK(SignTransaction(mtx, &second, s.coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, active));
        BOOST_CHECK(input_errors.empty());

        PrecomputedTransactionData txdata;
        txdata.SetSigForkId(SIG_FORK_ID_840K, active);
        const CTransaction txConst(mtx);
        TransactionSignatureChecker checker(&txConst, 0, s.coin.out.nValue, txdata);
        BOOST_CHECK(VerifyScript(mtx.vin[0].scriptSig, s.coin.out.scriptPubKey, &mtx.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, checker, nullptr));

        // A second party that assumes the other side of the transition does not get there
        CMutableTransaction mixed{s.tx};
        input_errors.clear();
        BOOST_CHECK(!SignTransaction(mixed, &first, s.coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, active));
        input_errors.clear();
        BOOST_CHECK(!SignTransaction(mixed, &second, s.coins, SIGHASH_ALL, input_errors, &SIG_FORK_ID_840K, !active));
    }
}

// Two parties signing the same PSBT independently; the combined PSBT finalizes under
// the regime the signatures were made for, and only under that one.
BOOST_AUTO_TEST_CASE(finalize_psbt_uses_the_given_regime)
{
    for (const bool active : {false, true}) {
        TwoPartySpend s;
        CMutableTransaction prev;
        prev.nVersion = 1;
        prev.vin.resize(1);
        prev.vout.resize(1);
        prev.vout[0] = s.coin.out;
        s.tx.vin[0].prevout = COutPoint(prev.GetHash(), 0);

        PartiallySignedTransaction base(s.tx);
        base.inputs[0].non_witness_utxo = MakeTransactionRef(prev);

        PartiallySignedTransaction parts[2] = {base, base};
        for (int i = 0; i < 2; ++i) {
            FillableSigningProvider party;
            s.FillParty(i, party);
            BOOST_CHECK(!SignPSBTInput(party, parts[i], 0, SIGHASH_ALL, nullptr, false, &SIG_FORK_ID_840K, active));
            BOOST_CHECK_EQUAL(parts[i].inputs[0].partial_sigs.size(), 1U);
        }
        PartiallySignedTransaction combined = parts[0];
        BOOST_REQUIRE(combined.Merge(parts[1]));
        BOOST_CHECK_EQUAL(combined.inputs[0].partial_sigs.size(), 2U);

        PartiallySignedTransaction wrong = combined;
        BOOST_CHECK(!FinalizePSBT(wrong, &SIG_FORK_ID_840K, !active));
        CMutableTransaction result;
        BOOST_CHECK(FinalizeAndExtractPSBT(combined, result, &SIG_FORK_ID_840K, active));

        PrecomputedTransactionData txdata;
        txdata.SetSigForkId(SIG_FORK_ID_840K, active);
        const CTransaction txConst(result);
        TransactionSignatureChecker checker(&txConst, 0, s.coin.out.nValue, txdata);
        BOOST_CHECK(VerifyScript(result.vin[0].scriptSig, s.coin.out.scriptPubKey, &result.vin[0].scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, checker, nullptr));
    }
}

BOOST_AUTO_TEST_SUITE_END()
