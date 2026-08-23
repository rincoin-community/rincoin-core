// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/fork_commitment.h>
#include <consensus/validation.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace {

CMutableTransaction MakeCoinbase()
{
    CMutableTransaction coinbase;
    coinbase.vin.resize(1);
    coinbase.vin[0].prevout.SetNull();
    coinbase.vout.resize(1);
    coinbase.vout[0].nValue = 0;
    coinbase.vout[0].scriptPubKey = CScript() << OP_TRUE;
    return coinbase;
}

CBlock BlockWithCoinbase(const CMutableTransaction& coinbase)
{
    CBlock block;
    block.vtx.push_back(MakeTransactionRef(coinbase));
    return block;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(fork_commitment_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(build_and_parse_round_trip)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CScript script = ForkCommitment::BuildForkCommitmentScript(params);
    BOOST_CHECK_EQUAL(script.size(), 2 + ForkCommitment::PAYLOAD_LEN); // OP_RETURN + push-len byte + payload
    BOOST_CHECK(ForkCommitment::LooksLikeForkCommitment(script));

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK(state.IsValid());
}

BOOST_AUTO_TEST_CASE(below_h1_always_valid)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CBlock block = BlockWithCoinbase(MakeCoinbase()); // no commitment output at all
    BlockValidationState state;
    BOOST_CHECK(ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height - 1, state));
}

BOOST_AUTO_TEST_CASE(missing_commitment_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CBlock block = BlockWithCoinbase(MakeCoinbase());
    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-missing");
}

BOOST_AUTO_TEST_CASE(duplicate_commitment_rejected_even_if_one_correct)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(params)));
    // A correct output does not neutralize an incorrect duplicate.
    Consensus::Params wrong_params = params;
    wrong_params.ForkScenarioId = params.ForkScenarioId + 1;
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(wrong_params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-duplicate");
}

BOOST_AUTO_TEST_CASE(two_correct_commitments_still_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(params)));
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-duplicate");
}

BOOST_AUTO_TEST_CASE(wrong_branch_id_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();
    Consensus::Params wrong_params = params;
    wrong_params.ForkBranchId[0] ^= 0xff;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(wrong_params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-wrong-branch");
}

BOOST_AUTO_TEST_CASE(wrong_fork_no_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();
    Consensus::Params wrong_params = params;
    wrong_params.ForkNo += 1;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(wrong_params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-wrong-fork-no");
}

BOOST_AUTO_TEST_CASE(wrong_scenario_id_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();
    Consensus::Params wrong_params = params;
    wrong_params.ForkScenarioId += 1;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(wrong_params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-wrong-scenario");
}

BOOST_AUTO_TEST_CASE(nonzero_commitment_value_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(1, ForkCommitment::BuildForkCommitmentScript(params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(truncated_payload_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    std::vector<unsigned char> payload = ForkCommitment::BuildForkCommitmentPayload(params);
    payload.pop_back(); // 27 bytes
    CScript script = CScript() << OP_RETURN << payload;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(extended_payload_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    std::vector<unsigned char> payload = ForkCommitment::BuildForkCommitmentPayload(params);
    payload.push_back(0x00); // 29 bytes
    CScript script = CScript() << OP_RETURN << payload;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(wrong_format_version_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    std::vector<unsigned char> payload = ForkCommitment::BuildForkCommitmentPayload(params);
    payload[4] = 2; // format_version
    CScript script = CScript() << OP_RETURN << payload;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(nonzero_flags_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    std::vector<unsigned char> payload = ForkCommitment::BuildForkCommitmentPayload(params, /*flags=*/1);
    CScript script = CScript() << OP_RETURN << payload;

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(non_minimal_push_rejected)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    std::vector<unsigned char> payload = ForkCommitment::BuildForkCommitmentPayload(params);
    // Hand-build OP_RETURN OP_PUSHDATA1 <len> <payload> instead of the
    // canonical direct length-prefixed push.
    CScript script;
    script << OP_RETURN;
    script.push_back((unsigned char)OP_PUSHDATA1);
    script.push_back((unsigned char)payload.size());
    script.insert(script.end(), payload.begin(), payload.end());

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout.push_back(CTxOut(0, script));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(!ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-fork-commitment-malformed");
}

BOOST_AUTO_TEST_CASE(zero_value_coinbase_claim_still_valid)
{
    // Regression guard: the fork commitment check is independent of the
    // "underclaiming is always valid" subsidy rule -- a correctly-committed
    // coinbase claiming zero must pass commitment validation (subsidy
    // ceiling enforcement is a separate check in ConnectBlock).
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    CMutableTransaction coinbase = MakeCoinbase();
    coinbase.vout[0].nValue = 0;
    coinbase.vout.push_back(CTxOut(0, ForkCommitment::BuildForkCommitmentScript(params)));
    CBlock block = BlockWithCoinbase(coinbase);

    BlockValidationState state;
    BOOST_CHECK(ForkCommitment::ValidateForkCommitment(block, params, params.ForkH1Height, state));
}

BOOST_AUTO_TEST_CASE(sig_fork_id_deterministic_and_branch_dependent)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    auto id1 = ForkCommitment::ComputeForkSigId(params.ForkBranchId, params.ForkNo, params.ForkScenarioId);
    auto id2 = ForkCommitment::ComputeForkSigId(params.ForkBranchId, params.ForkNo, params.ForkScenarioId);
    BOOST_CHECK(id1 == id2);
    BOOST_CHECK(id1 == params.ForkSigId); // matches what chainparams.cpp precomputed

    auto id3 = ForkCommitment::ComputeForkSigId(params.ForkBranchId, params.ForkNo + 1, params.ForkScenarioId);
    BOOST_CHECK(id1 != id3);
}

BOOST_AUTO_TEST_SUITE_END()
