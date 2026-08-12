// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/terminal.h>

#include <tinyformat.h>
#include <util/time.h>

#include <cstdlib>

std::atomic_bool g_terminal_halted{false};
std::atomic_bool g_terminal_notify_pending{false};

const std::set<std::string> g_terminal_rpc_allowlist{
    "help",
    "logging",
    "stop",
    "uptime",
};

namespace {
//! Tier boundaries, expressed in time so they stay meaningful on a chain with a
//! different block spacing.
constexpr int64_t SECONDS_PER_DAY = 24 * 60 * 60;
constexpr int64_t APPROACHING_THRESHOLD_SECONDS = 7 * SECONDS_PER_DAY;
constexpr int64_t IMMINENT_THRESHOLD_SECONDS = SECONDS_PER_DAY;

//! How often a loud warning repeats within each tier, in blocks. Roughly: every
//! 17 hours while far out, every 2.4 hours inside a week, every half hour in the
//! final day -- about 140 loud events across a 30-day window at 60s spacing.
constexpr int DISTANT_INTERVAL_BLOCKS = 1000;
constexpr int APPROACHING_INTERVAL_BLOCKS = 144;
constexpr int IMMINENT_INTERVAL_BLOCKS = 30;
} // namespace

TerminalWarningTier TerminalTierFor(int blocks_remaining, int64_t seconds_per_block)
{
    if (blocks_remaining < 0) return TerminalWarningTier::NONE;
    if (seconds_per_block <= 0) seconds_per_block = 60;

    const int64_t seconds_remaining = static_cast<int64_t>(blocks_remaining) * seconds_per_block;
    if (seconds_remaining <= IMMINENT_THRESHOLD_SECONDS) return TerminalWarningTier::IMMINENT;
    if (seconds_remaining <= APPROACHING_THRESHOLD_SECONDS) return TerminalWarningTier::APPROACHING;
    return TerminalWarningTier::DISTANT;
}

bool TerminalLoudWarningDue(int height, int blocks_remaining, int64_t seconds_per_block)
{
    // The last block before the halt always warns, whatever the interval says:
    // it is the final chance to say anything at all.
    if (blocks_remaining <= 1) return true;

    int interval;
    switch (TerminalTierFor(blocks_remaining, seconds_per_block)) {
    case TerminalWarningTier::IMMINENT: interval = IMMINENT_INTERVAL_BLOCKS; break;
    case TerminalWarningTier::APPROACHING: interval = APPROACHING_INTERVAL_BLOCKS; break;
    case TerminalWarningTier::DISTANT: interval = DISTANT_INTERVAL_BLOCKS; break;
    case TerminalWarningTier::NONE: return false;
    }
    if (interval <= 1) return true;

    // Key the cadence off the absolute height rather than a counter, so it is a
    // pure function of the chain: a restart, a reorg or a resync cannot make the
    // warning fire more or less often than the tier says.
    return height % interval == 0;
}

bilingual_str TerminalHaltMessage(int terminal_height)
{
    return strprintf(_("This release stops at block height %d and does not implement the "
                       "height-%d consensus rules. Install a release that implements the "
                       "selected rules to continue."),
                     terminal_height, terminal_height);
}

bilingual_str TerminalWarningMessage(int terminal_height, int blocks_remaining, int64_t seconds_per_block)
{
    if (seconds_per_block <= 0) seconds_per_block = 60;
    const int64_t seconds_remaining = static_cast<int64_t>(blocks_remaining) * seconds_per_block;
    const int64_t days_remaining = seconds_remaining / SECONDS_PER_DAY;
    const int64_t hours_remaining = (seconds_remaining % SECONDS_PER_DAY) / 3600;

    // An estimate, and labelled as one: real block spacing wanders, so the date
    // will drift. It is still far more actionable than a block count alone.
    const std::string estimated_date = FormatISO8601Date(GetTime() + seconds_remaining);

    return strprintf(_("This release stops at block height %d. About %d blocks left "
                       "(roughly %dd %dh, estimated %s). It does not implement the height-%d "
                       "consensus rules; install a release that implements the selected rules "
                       "before then or this node will stop."),
                     terminal_height, blocks_remaining, days_remaining, hours_remaining,
                     estimated_date, terminal_height);
}
