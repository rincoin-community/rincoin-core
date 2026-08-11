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

/** The single frozen operator-facing message. Used verbatim by the chain-advance
 *  halt, the startup refusal, the RPC gate and the mining refusal, so that every
 *  route out of this build says exactly the same thing. */
bilingual_str TerminalHaltMessage(int terminal_height);

/** The persistent warning shown for nTerminalWarningLead blocks before the halt. */
bilingual_str TerminalWarningMessage(int terminal_height, int blocks_remaining);

#endif // BITCOIN_NODE_TERMINAL_H
