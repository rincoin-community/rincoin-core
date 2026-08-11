// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/terminal.h>

#include <tinyformat.h>

std::atomic_bool g_terminal_halted{false};

const std::set<std::string> g_terminal_rpc_allowlist{
    "help",
    "logging",
    "stop",
    "uptime",
};

bilingual_str TerminalHaltMessage(int terminal_height)
{
    return strprintf(_("This release stops at block height %d and does not implement the "
                       "height-%d consensus rules. Install a release that implements the "
                       "selected rules to continue."),
                     terminal_height, terminal_height);
}

bilingual_str TerminalWarningMessage(int terminal_height, int blocks_remaining)
{
    return strprintf(_("This release stops at block height %d (about %d blocks away). Install a "
                       "release that implements the selected height-%d rules before then."),
                     terminal_height, blocks_remaining, terminal_height);
}
