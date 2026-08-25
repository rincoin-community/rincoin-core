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
  * FORK_SUBSIDY_NEXT_CHANGE_EPOCHS, if this scenario's post-activation epoch
    length differs from nSubsidyHalvingInterval (S1's does not; S5/b's is
    10x longer, so its first post-activation subsidy change lands later than
    one plain halving_interval past H1 -- see that constant's own comment).

Everything else in this module (the commitment byte layout, sig_fork_id
derivation, script-building helpers) is fixed by the design docs
(rincoin-consensus840k/technology/consensus-transition.md §5/§6) and is not
scenario-specific -- it should not need to change between branches.

The values below belong to the ``consensus/s5b-testing`` branch specifically.
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
FORK_SCENARIO_ID = 2
FORK_FORMAT_VERSION = 1

# How many nSubsidyHalvingInterval-multiples after H1 the first post-
# activation subsidy *change* occurs -- used by feature_fork_subsidy.py to
# find a real "next epoch boundary" without hardcoding a scenario-specific
# distance in that scenario-agnostic-by-design test file. S5/b's phase 0
# starts at anchor = H1 - halving_interval (one ordinary epoch before H1)
# and is 10*halving_interval long, so phase 0 doesn't end until
# anchor + 10*halving_interval = H1 + 9*halving_interval -- H1 is already
# one interval *into* phase 0, not at its start.
FORK_SUBSIDY_NEXT_CHANGE_EPOCHS = 9

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


def expected_subsidy(height, h1_height=FORK_H1_HEIGHT, halving_interval=210000, base_reward=50 * 10**8):
    """S5/b schedule: an extended, 10x-longer halving epoch, phase-anchored
    to the pre-activation epoch boundary at h1_height - halving_interval
    (not to h1_height itself), then plain binary halving per extended
    epoch, no floor, no tail (rincoin-consensus840k/analysis/
    Rincoin_840k_S5B_Consensus_Change_Specification.qmd).

    The post-fork series starts from the pre-fork subsidy value at that
    anchor epoch (the same "base" quantity S1's formula also starts from).
    Because h1_height is exactly one ordinary epoch past the anchor, and the
    post-fork epoch is ten ordinary epochs long, the subsidy is flat at
    `base` for the entire first post-fork epoch (h1_height through
    h1_height + 9*halving_interval - 1), not stepped down immediately.
    """
    if height < h1_height:
        halvings = height // halving_interval
        if halvings >= 64:
            return 0
        return base_reward >> halvings

    pre_fork_halvings = (h1_height - 1) // halving_interval
    base = base_reward >> pre_fork_halvings

    anchor = h1_height - halving_interval
    post_fork_epoch_length = 10 * halving_interval
    phase = (height - anchor) // post_fork_epoch_length
    if phase >= 30:
        return 0
    return base >> phase
