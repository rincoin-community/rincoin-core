#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Per-branch override surface for the height-840,000 fork test framework.

Every ``consensus/<codename>`` branch (S1, S5/b, S6/b, ...) that implements a
candidate post-840k ruleset copies this file and edits exactly three things:

  * The four identity constants (FORK_BRANCH_ID / FORK_NO / FORK_SCENARIO_ID /
    FORK_FORMAT_VERSION) to whatever this branch's consensus code was
    compiled with.
  * The body of ``expected_subsidy()`` to match this branch's subsidy
    formula.
  * Any additional scenario-shape data ``expected_subsidy()`` needs (this
    branch's own FORK_SUBSIDY_PHASES table, mirroring
    Consensus::Params::ForkSubsidyPhases -- other branches may need a
    differently-shaped constant, or none at all, for their own formula).

Everything else in this module (the commitment byte layout, sig_fork_id
derivation, script-building helpers) is fixed by the design docs
(rincoin-consensus840k/technology/consensus-transition.md §5/§6) and is not
scenario-specific -- it should not need to change between branches.

The values below belong to the ``consensus/s6b-testing`` branch specifically.
They are test-only placeholders (branch_id is the design docs' shared
canonical synthetic value, never a mainnet value; ForkScenarioId is a
provisional number pending official upstream assignment); do not reuse them
for a different scenario or for any real release.
"""

import hashlib

from test_framework.script import CScript, OP_RETURN

# --- Per-branch identity (edit these per consensus/<codename> branch) -----

# Regtest fork-activation height used by this test suite. The compiled-in
# CRegTestParams default is deliberately much higher than this (see
# chainparams.cpp) so C++ unit tests that build synthetic chains offline are
# unaffected; these Python functional tests instead pass FORK_H1_EXTRA_ARG
# explicitly to every S1-build node's extra_args. Kept below
# FIRST_MWEB_HEIGHT (test_framework/rin_util.py, 432 by default) so
# hand-built negative-test blocks don't also need to satisfy MWEB's
# extension-block/HogEx requirements, which are unrelated to what these
# tests exercise.
FORK_H1_HEIGHT = 200
FORK_H1_EXTRA_ARG = f"-forkh1height={FORK_H1_HEIGHT}"

# Opaque 128-bit lineage id, big-endian byte string. Canonical synthetic test
# value published in technology/consensus-transition.md §5 -- shared,
# scenario-agnostic infrastructure (S5/b and S6/b testing branches use the
# same value), never a mainnet value.
FORK_BRANCH_ID = bytes.fromhex("00112233445566778899aabbccddeeff")

FORK_NO = 1
# Provisional, ad-hoc scenario id pending official assignment upstream (none
# exists yet for any candidate scenario). S1=1, S5/b=2, S6/b=3 by convention
# across this repo's consensus/*-testing branches.
FORK_SCENARIO_ID = 3
FORK_FORMAT_VERSION = 1

# This scenario's own small-scale phase table, mirroring
# Consensus::Params::ForkSubsidyPhases / CRegTestParams::ForkSubsidyPhases in
# chainparams.cpp exactly (same values, same H1-relative offsets) -- not
# mainnet's real table scaled down (mainnet's terminal phase alone is ~228
# million blocks long), but an independently chosen schedule using the same
# four-fixed-phases-plus-derived-cutoff structure. Each entry is
# (offset_from_h1, subsidy_base_units); the last entry's subsidy must be 0
# (the terminal cutoff).
FORK_SUBSIDY_PHASES = [
    (0, 400000000),
    (50, 200000000),
    (100, 100000000),
    (150, 60000000),
    (160, 0),
]

assert len(FORK_BRANCH_ID) == 16, "FORK_BRANCH_ID must be exactly 16 bytes"

# --- Fixed wire format (do not edit per-branch) ----------------------------

FORK_COMMITMENT_MAGIC = b"RINF"
FORK_COMMITMENT_PAYLOAD_LEN = 28  # magic(4) + version(1) + branch_id(16) + fork_no(4) + scenario_id(2) + flags(1)


def build_fork_commitment_payload(branch_id=FORK_BRANCH_ID, fork_no=FORK_NO,
                                   scenario_id=FORK_SCENARIO_ID,
                                   format_version=FORK_FORMAT_VERSION, flags=0):
    """Build the 28-byte fork-commitment payload (the bytes pushed after OP_RETURN)."""
    assert len(branch_id) == 16
    payload = (
        FORK_COMMITMENT_MAGIC
        + format_version.to_bytes(1, "big")
        + branch_id
        + fork_no.to_bytes(4, "big")
        + scenario_id.to_bytes(2, "big")
        + flags.to_bytes(1, "big")
    )
    assert len(payload) == FORK_COMMITMENT_PAYLOAD_LEN
    return payload


def build_fork_commitment_script(**kwargs):
    """Build the canonical CScript for a correct fork-commitment output.

    Uses CScript's normal (minimal-push) serialization of a bytes object, the
    same 30-byte `6a1c<28 bytes>` encoding the C++ side is required to emit.
    """
    return CScript([OP_RETURN, build_fork_commitment_payload(**kwargs)])


def sig_fork_id(branch_id=FORK_BRANCH_ID, fork_no=FORK_NO, scenario_id=FORK_SCENARIO_ID):
    """SHA256(branch_id || fork_no_BE(4) || scenario_id_BE(2))[:8].

    Mirrors Consensus::Params::ForkSigId, computed once per chainparams
    construction on the C++ side. Mixed into the legacy and BIP143 sighash
    preimages for every non-coinbase input from FORK_H1_HEIGHT onward.
    """
    assert len(branch_id) == 16
    preimage = branch_id + fork_no.to_bytes(4, "big") + scenario_id.to_bytes(2, "big")
    return hashlib.sha256(preimage).digest()[:8]


def expected_subsidy(height, h1_height=FORK_H1_HEIGHT, halving_interval=150,
                      base_reward=50 * 10**8, phases=None):
    """S6/b schedule: a small number of fixed-value phases followed by a
    hard cutoff to zero, driven entirely by `phases` (defaults to this
    branch's own FORK_SUBSIDY_PHASES) -- mirrors
    Consensus::Params::ForkSubsidyPhases / GetBlockSubsidyPostFork()'s own
    generic table scan on the C++ side exactly, rather than re-deriving the
    formula (there is no closed-form formula for this scenario; the table
    *is* the specification). `halving_interval`/`base_reward` only apply to
    heights below `h1_height`, where the ordinary pre-fork halving rule
    (untouched by this scenario) still applies.
    """
    if phases is None:
        phases = FORK_SUBSIDY_PHASES

    if height < h1_height:
        halvings = height // halving_interval
        if halvings >= 64:
            return 0
        return base_reward >> halvings

    assert phases[0][0] == 0, "the phase table's first entry must start exactly at h1_height"
    subsidy = 0
    for offset, phase_subsidy in phases:
        if h1_height + offset > height:
            break
        subsidy = phase_subsidy
    return subsidy
