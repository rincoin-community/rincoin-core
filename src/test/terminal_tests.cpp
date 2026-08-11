// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/params.h>
#include <node/terminal.h>
#include <test/util/setup_common.h>
#include <util/system.h>

BOOST_FIXTURE_TEST_SUITE(terminal_tests, BasicTestingSetup)

//! The height this release stops at on mainnet.
static constexpr int MAINNET_TERMINAL_HEIGHT = 840000;
//! 30 days of 60-second blocks.
static constexpr int MAINNET_WARNING_LEAD = 43200;

BOOST_AUTO_TEST_CASE(terminal_mainnet_parameters)
{
    SelectParams(CBaseChainParams::MAIN);
    const auto& consensus = Params().GetConsensus();

    BOOST_CHECK(consensus.TerminalEnabled());
    BOOST_CHECK_EQUAL(consensus.nTerminalHeight, MAINNET_TERMINAL_HEIGHT);
    BOOST_CHECK_EQUAL(consensus.nTerminalWarningLead, MAINNET_WARNING_LEAD);
}

BOOST_AUTO_TEST_CASE(terminal_disabled_on_test_chains_by_default)
{
    // Every test chain ships with the halt off, so the existing functional
    // suite is unaffected and a test must opt in with -terminalheight.
    for (const std::string& net : {CBaseChainParams::TESTNET, CBaseChainParams::REGTEST,
                                   CBaseChainParams::PREVIEW}) {
        SelectParams(net);
        const auto& consensus = Params().GetConsensus();
        BOOST_CHECK(!consensus.TerminalEnabled());
        BOOST_CHECK_EQUAL(consensus.nTerminalHeight, 0);
        // A disabled halt must never claim a height is terminal, and must never
        // warn -- including at and around height 0, where a naive
        // "height >= nTerminalHeight - lead" test would fire for every block.
        for (const int height : {0, 1, 1000, MAINNET_TERMINAL_HEIGHT, MAINNET_TERMINAL_HEIGHT + 1}) {
            BOOST_CHECK(!consensus.TerminalReached(height));
            BOOST_CHECK(!consensus.TerminalWarning(height));
        }
    }
}

BOOST_AUTO_TEST_CASE(terminal_reached_boundary)
{
    SelectParams(CBaseChainParams::MAIN);
    const auto& consensus = Params().GetConsensus();

    // The last block this release will ever connect is H-1.
    BOOST_CHECK(!consensus.TerminalReached(MAINNET_TERMINAL_HEIGHT - 2));
    BOOST_CHECK(!consensus.TerminalReached(MAINNET_TERMINAL_HEIGHT - 1));
    BOOST_CHECK(consensus.TerminalReached(MAINNET_TERMINAL_HEIGHT));
    BOOST_CHECK(consensus.TerminalReached(MAINNET_TERMINAL_HEIGHT + 1));
    // A far-future block is terminal too: the halt is a floor on the height, not
    // an equality test that a chain could jump over.
    BOOST_CHECK(consensus.TerminalReached(MAINNET_TERMINAL_HEIGHT * 2));
}

BOOST_AUTO_TEST_CASE(terminal_warning_window)
{
    SelectParams(CBaseChainParams::MAIN);
    const auto& consensus = Params().GetConsensus();
    const int window_start = MAINNET_TERMINAL_HEIGHT - MAINNET_WARNING_LEAD;

    BOOST_CHECK_EQUAL(window_start, 796800);
    BOOST_CHECK(!consensus.TerminalWarning(window_start - 1));
    BOOST_CHECK(consensus.TerminalWarning(window_start));
    BOOST_CHECK(consensus.TerminalWarning(MAINNET_TERMINAL_HEIGHT - 1));
}

BOOST_AUTO_TEST_CASE(terminal_overrides_rejected_on_mainnet)
{
    // Fail closed: the terminal height of a release build is fixed. A mainnet
    // invocation that tries to move, disable, or re-enable it must be refused
    // loudly rather than silently ignored.
    ArgsManager args;
    args.ForceSetArg("-terminalheight", "1");
    CChainParams& mainnet = const_cast<CChainParams&>(Params());
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK_THROW(mainnet.UpdateTerminalParametersFromArgs(args), std::runtime_error);

    ArgsManager lead_args;
    lead_args.ForceSetArg("-terminalwarninglead", "1");
    BOOST_CHECK_THROW(mainnet.UpdateTerminalParametersFromArgs(lead_args), std::runtime_error);

    // Mainnet parameters are untouched by the refused attempts.
    BOOST_CHECK_EQUAL(Params().GetConsensus().nTerminalHeight, MAINNET_TERMINAL_HEIGHT);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nTerminalWarningLead, MAINNET_WARNING_LEAD);
}

BOOST_AUTO_TEST_CASE(terminal_overrides_applied_on_test_chains)
{
    SelectParams(CBaseChainParams::REGTEST);
    CChainParams& regtest = const_cast<CChainParams&>(Params());

    ArgsManager args;
    args.ForceSetArg("-terminalheight", "150");
    args.ForceSetArg("-terminalwarninglead", "5");
    BOOST_CHECK_NO_THROW(regtest.UpdateTerminalParametersFromArgs(args));

    const auto& consensus = Params().GetConsensus();
    BOOST_CHECK(consensus.TerminalEnabled());
    BOOST_CHECK_EQUAL(consensus.nTerminalHeight, 150);
    BOOST_CHECK(!consensus.TerminalReached(149));
    BOOST_CHECK(consensus.TerminalReached(150));
    BOOST_CHECK(!consensus.TerminalWarning(144));
    BOOST_CHECK(consensus.TerminalWarning(145));

    // 0 turns the halt back off on a test chain.
    ArgsManager off;
    off.ForceSetArg("-terminalheight", "0");
    BOOST_CHECK_NO_THROW(regtest.UpdateTerminalParametersFromArgs(off));
    BOOST_CHECK(!Params().GetConsensus().TerminalEnabled());

    // Out-of-range values are refused rather than truncated into something
    // surprising.
    ArgsManager negative;
    negative.ForceSetArg("-terminalheight", "-1");
    BOOST_CHECK_THROW(regtest.UpdateTerminalParametersFromArgs(negative), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(terminal_messages_name_the_height)
{
    // Every route out of this build must quote the same height, so an operator
    // who sees one message can match it against the others.
    const bilingual_str halt = TerminalHaltMessage(MAINNET_TERMINAL_HEIGHT);
    BOOST_CHECK(halt.original.find("840000") != std::string::npos);
    BOOST_CHECK(!halt.original.empty());

    const bilingual_str warning = TerminalWarningMessage(MAINNET_TERMINAL_HEIGHT, 720);
    BOOST_CHECK(warning.original.find("840000") != std::string::npos);
    BOOST_CHECK(warning.original.find("720") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(terminal_rpc_allowlist_is_minimal)
{
    // An operator must be able to see why the node stopped and to stop it, but
    // must not be able to make it do any more work.
    BOOST_CHECK(g_terminal_rpc_allowlist.count("stop"));
    BOOST_CHECK(g_terminal_rpc_allowlist.count("help"));
    BOOST_CHECK(!g_terminal_rpc_allowlist.count("getblocktemplate"));
    BOOST_CHECK(!g_terminal_rpc_allowlist.count("generatetoaddress"));
    BOOST_CHECK(!g_terminal_rpc_allowlist.count("submitblock"));
    BOOST_CHECK(!g_terminal_rpc_allowlist.count("sendtoaddress"));
}

BOOST_AUTO_TEST_SUITE_END()
