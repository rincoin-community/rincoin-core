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

BOOST_AUTO_TEST_CASE(terminal_enabled_on_every_network_at_its_own_4th_halving)
{
    // Every network stops at its own 4th subsidy halving, not just mainnet.
    // Testnet shares mainnet's halving interval and spacing, so it lands on
    // the same height; regtest and preview use their own fast interval (150).
    // The Python functional test harness (test_framework/test_node.py)
    // disables this by default for every node it starts, so the existing
    // regtest-based functional suite -- which routinely mines well past 600,
    // e.g. regtest's own BIP65/66 activation heights -- is unaffected unless a
    // test opts in with its own -terminalheight.
    const std::pair<std::string, int> expected[] = {
        {CBaseChainParams::TESTNET, MAINNET_TERMINAL_HEIGHT},
        {CBaseChainParams::REGTEST, 600},
        {CBaseChainParams::PREVIEW, 600},
    };
    for (const auto& entry : expected) {
        const std::string& net = entry.first;
        const int height = entry.second;
        SelectParams(net);
        const auto& consensus = Params().GetConsensus();
        BOOST_CHECK(consensus.TerminalEnabled());
        BOOST_CHECK_EQUAL(consensus.nTerminalHeight, height);
        BOOST_CHECK(!consensus.TerminalReached(height - 1));
        BOOST_CHECK(consensus.TerminalReached(height));
    }
}

BOOST_AUTO_TEST_CASE(terminal_override_can_still_disable_test_chains)
{
    // A test chain's non-zero default must remain overridable back to 0, since
    // some functional tests (and the harness default) rely on that to
    // represent a build with the halt genuinely absent.
    SelectParams(CBaseChainParams::REGTEST);
    CChainParams& regtest = const_cast<CChainParams&>(Params());

    ArgsManager off;
    off.ForceSetArg("-terminalheight", "0");
    BOOST_CHECK_NO_THROW(regtest.UpdateTerminalParametersFromArgs(off));
    BOOST_CHECK(!Params().GetConsensus().TerminalEnabled());
    for (const int height : {0, 1, 1000, MAINNET_TERMINAL_HEIGHT}) {
        BOOST_CHECK(!Params().GetConsensus().TerminalReached(height));
        BOOST_CHECK(!Params().GetConsensus().TerminalWarning(height));
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
    // SelectParams first: it replaces the globalChainParams unique_ptr, freeing
    // whatever Params() previously returned. Taking the reference before the
    // call leaves it dangling, which ASan correctly flags as a use-after-free.
    SelectParams(CBaseChainParams::MAIN);
    CChainParams& mainnet = const_cast<CChainParams&>(Params());

    ArgsManager args;
    args.ForceSetArg("-terminalheight", "1");
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

    const bilingual_str warning = TerminalWarningMessage(MAINNET_TERMINAL_HEIGHT, 720, 60);
    BOOST_CHECK(warning.original.find("840000") != std::string::npos);
    BOOST_CHECK(warning.original.find("720") != std::string::npos);
    // 720 blocks at 60s is half a day, and the message says so in time as well
    // as in blocks -- operators plan in hours and dates, not block counts.
    BOOST_CHECK(warning.original.find("12h") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(terminal_warning_tiers)
{
    // Tier boundaries are expressed in time, so they hold for any spacing.
    constexpr int64_t SPACING = 60;
    constexpr int DAY = 24 * 60;          // blocks per day at 60s
    BOOST_CHECK(TerminalTierFor(30 * DAY, SPACING) == TerminalWarningTier::DISTANT);
    BOOST_CHECK(TerminalTierFor(8 * DAY, SPACING) == TerminalWarningTier::DISTANT);
    BOOST_CHECK(TerminalTierFor(7 * DAY, SPACING) == TerminalWarningTier::APPROACHING);
    BOOST_CHECK(TerminalTierFor(2 * DAY, SPACING) == TerminalWarningTier::APPROACHING);
    BOOST_CHECK(TerminalTierFor(DAY, SPACING) == TerminalWarningTier::IMMINENT);
    BOOST_CHECK(TerminalTierFor(1, SPACING) == TerminalWarningTier::IMMINENT);
    BOOST_CHECK(TerminalTierFor(-1, SPACING) == TerminalWarningTier::NONE);

    // Halving the spacing doubles the block count each tier covers.
    BOOST_CHECK(TerminalTierFor(10 * DAY, 30) == TerminalWarningTier::APPROACHING);
}

BOOST_AUTO_TEST_CASE(terminal_loud_warning_cadence)
{
    constexpr int64_t SPACING = 60;
    constexpr int DAY = 24 * 60;
    constexpr int H = 840000;

    // Far out: every 1000 blocks and not in between.
    BOOST_CHECK(TerminalLoudWarningDue(800000, H - 800000, SPACING));
    BOOST_CHECK(!TerminalLoudWarningDue(800001, H - 800001, SPACING));

    // Inside a week the interval tightens to 144, so a height that is a
    // multiple of 144 but not of 1000 now warns where, one tier out, it would
    // have stayed quiet.
    const int near_height = 838512;  // 1488 blocks out: ~24.8h, so APPROACHING not IMMINENT
    BOOST_CHECK_EQUAL(near_height % 144, 0);
    BOOST_CHECK_NE(near_height % 1000, 0);
    BOOST_CHECK(TerminalTierFor(H - near_height, SPACING) == TerminalWarningTier::APPROACHING);
    BOOST_CHECK(TerminalLoudWarningDue(near_height, H - near_height, SPACING));
    BOOST_CHECK(!TerminalLoudWarningDue(near_height + 1, H - near_height - 1, SPACING));

    // Inside the final day, every 30 blocks.
    const int imminent_height = H - 600;
    BOOST_CHECK(TerminalLoudWarningDue(imminent_height, H - imminent_height, SPACING));
    BOOST_CHECK(!TerminalLoudWarningDue(imminent_height + 1, H - imminent_height - 1, SPACING));

    // The last block before the halt always warns, whatever the interval says.
    BOOST_CHECK(TerminalLoudWarningDue(H - 1, 1, SPACING));

    // Cadence is a pure function of height, so a restart or a reorg cannot
    // change how often it fires.
    BOOST_CHECK_EQUAL(TerminalLoudWarningDue(800000, H - 800000, SPACING),
                      TerminalLoudWarningDue(800000, H - 800000, SPACING));
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
