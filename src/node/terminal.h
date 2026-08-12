// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_TERMINAL_H
#define BITCOIN_NODE_TERMINAL_H

#include <util/translation.h>

#include <atomic>
#include <set>
#include <string>

/**
 * Terminal-release support.
 *
 * This build stops at Consensus::Params::nTerminalHeight. It introduces no
 * consensus rule and no protocol change: it validates normally below that
 * height, never connects a block at or above it, and shuts down instead. A
 * block at the terminal height is left in the block index as an ordinary
 * unconnected candidate -- it is not marked invalid, and the peer that sent it
 * is not punished -- so the chain this node leaves behind remains valid for any
 * successor release to continue from.
 *
 * The height is not an activation height. Nothing changes about how blocks are
 * validated; the node simply refuses to go on.
 */

/**
 * Set once the terminal height has been reached and shutdown has been
 * requested. Read by the RPC layer to refuse work during the brief window
 * between the halt and the process actually exiting.
 */
extern std::atomic_bool g_terminal_halted;

/**
 * RPC methods still answered after the halt. Deliberately tiny: an operator has
 * to be able to see why the node stopped and to stop it themselves, but must not
 * be able to make it do any further work.
 */
extern const std::set<std::string> g_terminal_rpc_allowlist;

/**
 * Set by the core when a loud warning is due, consumed and cleared by the GUI,
 * which turns it into one non-blocking desktop notification.
 *
 * The core deliberately does not raise the notification itself. The obvious way
 * to do that -- uiInterface.ThreadSafeMessageBox without MODAL -- reaches the
 * GUI's notificator, but in a daemon the same call lands in noui, which writes
 * unconditionally to stderr. A long-running daemon should say this in
 * debug.log, not on stderr, so the two frontends are fed separately: LogPrintf
 * for the daemon, this flag for the GUI.
 */
extern std::atomic_bool g_terminal_notify_pending;

/** The single frozen operator-facing message. Used verbatim by the chain-advance
 *  halt, the startup refusal, the RPC gate and the mining refusal, so that every
 *  route out of this build says exactly the same thing. */
bilingual_str TerminalHaltMessage(int terminal_height);

/** The persistent warning shown for nTerminalWarningLead blocks before the halt.
 *  Carries an estimated calendar date as well as the block count: operators plan
 *  in dates, and "43200 blocks away" means nothing without doing the arithmetic. */
bilingual_str TerminalWarningMessage(int terminal_height, int blocks_remaining, int64_t seconds_per_block);

/**
 * How loudly to warn at a given distance from the terminal height.
 *
 * A fixed cadence over a 30-day window is the recipe for alarm fatigue -- by the
 * second week an unchanging message is wallpaper. The interval instead tightens
 * as the deadline approaches, so the warning is quiet while there is plenty of
 * time and insistent when there is not.
 */
enum class TerminalWarningTier {
    NONE,     //!< outside the warning window
    DISTANT,  //!< more than 7 days out
    NEAR,     //!< inside 7 days
    IMMINENT, //!< inside 24 hours
};

/** The tier that applies `blocks_remaining` before the halt. */
TerminalWarningTier TerminalTierFor(int blocks_remaining, int64_t seconds_per_block);

/** Whether a loud warning (log line, stderr, GUI notification) is due at this
 *  height. Quiet blocks still carry the warning on the per-block UpdateTip line
 *  and in the `warnings` RPC field, which cost nothing extra. */
bool TerminalLoudWarningDue(int height, int blocks_remaining, int64_t seconds_per_block);

/** Human-readable tier name, for logs. */
const char* TerminalTierName(TerminalWarningTier tier);

#endif // BITCOIN_NODE_TERMINAL_H
