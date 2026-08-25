#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Per-branch override surface for the height-840,000 fork test framework.

Every ``consensus/<codename>`` branch (S1, S5/b, S6/b, ...) that implements a
candidate post-840k ruleset copies this file and edits exactly two things:

  * The four identity constants (FORK_BRANCH_ID / FORK_NO / FORK_SCENARIO_ID /
    FORK_FORMAT_VERSION) to whatever this branch's consensus code was
    compiled with.
  * The body of ``expected_subsidy()`` to match this branch's subsidy
    formula.

Everything else in this module (the commitment byte layout, sig_fork_id
derivation, script-building helpers) is fixed by the design docs
(rincoin-consensus840k/technology/consensus-transition.md §5/§6) and is not
scenario-specific -- it should not need to change between branches.

The values below belong to the ``consensus/s1-testing`` branch specifically.
They are test-only placeholders (branch_id was freshly generated for this
branch and is never a mainnet value); do not reuse them for a different
scenario or for any real release.
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
FORK_SCENARIO_ID = 1
FORK_FORMAT_VERSION = 1

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
    """S1 schedule: recursive integer-floor x19/20 per 210,000-block epoch
    from H1 onward, no floor, no tail.

    The post-fork series starts from the pre-fork subsidy value at the
    halving epoch immediately below H1 (i.e. what the ordinary halving
    schedule already produced just before H1 -- NOT one further halving,
    which is what a naive continuation of the old rule would give at H1
    itself). From that base, x19/20 (integer floor) is applied once per
    completed post-fork epoch, inclusive of the epoch containing `height`.
    """
    if height < h1_height:
        halvings = height // halving_interval
        if halvings >= 64:
            return 0
        return base_reward >> halvings

    pre_fork_halvings = (h1_height - 1) // halving_interval
    base = base_reward >> pre_fork_halvings

    epoch = (height - h1_height) // halving_interval
    subsidy = base
    for _ in range(epoch + 1):
        subsidy = (subsidy * 19) // 20
    return subsidy
