#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Fixed, reusable block-building helpers for the fork scenario test suite
(test/functional/feature_fork_*.py). Not scenario-specific -- see
fork_scenario.py for the per-branch override surface these helpers consume.
"""

from test_framework.blocktools import NORMAL_GBT_REQUEST_PARAMS, create_coinbase
from test_framework.messages import CBlock, COIN, CTxOut
from test_framework.script import CScript, OP_RETURN
from test_framework.util import assert_equal

from test_framework.fork_scenario import FORK_H1_HEIGHT, build_fork_commitment_payload, expected_subsidy

REGTEST_HALVING_INTERVAL = 150  # matches CRegTestParams::nSubsidyHalvingInterval


def mine_to_height(node, height, addr=None):
    """Generate blocks (via the node's own miner) until the tip reaches `height`."""
    addr = addr or node.get_deterministic_priv_key().address
    current = node.getblockcount()
    if height > current:
        node.generatetoaddress(height - current, addr)
    assert_equal(node.getblockcount(), height)


def get_tip_template(node):
    return node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)


def build_block_from_template(tmpl, coinbase_tx):
    block = CBlock()
    block.nVersion = tmpl["version"]
    block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
    block.nTime = max(tmpl["curtime"], tmpl.get("mintime", 0))
    block.nBits = int(tmpl["bits"], 16)
    block.nNonce = 0
    block.vtx = [coinbase_tx]
    block.hashMerkleRoot = block.calc_merkle_root()
    return block


def build_fork_test_block(node, *, omit_commitment=False, duplicate_commitment=False,
                           extra_commitment_kwargs=None, second_commitment_kwargs=None,
                           raw_commitment_payload=None, commitment_value=0,
                           commitment_index="last"):
    """Build a single block on top of the current tip whose coinbase carries a
    fork-commitment output under caller control, ready for node.submitblock().

    - omit_commitment: don't add any RINF-namespace output at all.
    - duplicate_commitment: add two commitment outputs (see
      extra_commitment_kwargs/second_commitment_kwargs to control whether
      both are correct, or one correct + one wrong).
    - raw_commitment_payload: bytes to push after OP_RETURN verbatim,
      bypassing build_fork_commitment_payload()'s field-based construction --
      used for malformed-payload cases (truncated/extended/non-minimal-push).
    - commitment_value: nValue of the commitment CTxOut (should be 0 for a
      valid commitment; nonzero exercises the "must be zero" rule).
    - commitment_index: "first", "last", or an int index for where the
      commitment output lands relative to the primary reward output.
    """
    tmpl = get_tip_template(node)
    height = int(tmpl["height"])
    coinbase_tx = create_coinbase(height=height)
    if height >= FORK_H1_HEIGHT:
        # create_coinbase()'s built-in subsidy formula is the plain
        # pre-fork regtest halving (height // 150) -- it has no notion of
        # this branch's post-fork schedule, so it overclaims from H1 onward and
        # every such block would fail on bad-cb-amount regardless of the
        # commitment under test. Override with the correct post-fork ceiling.
        coinbase_tx.vout[0].nValue = expected_subsidy(
            height, h1_height=FORK_H1_HEIGHT, halving_interval=REGTEST_HALVING_INTERVAL, base_reward=50 * COIN)
        coinbase_tx.rehash()

    commitment_outputs = []
    if not omit_commitment:
        payload = raw_commitment_payload
        if payload is None:
            payload = build_fork_commitment_payload(**(extra_commitment_kwargs or {}))
        commitment_outputs.append(CTxOut(commitment_value, CScript([OP_RETURN, payload])))
        if duplicate_commitment:
            payload2 = build_fork_commitment_payload(**(second_commitment_kwargs or extra_commitment_kwargs or {}))
            commitment_outputs.append(CTxOut(commitment_value, CScript([OP_RETURN, payload2])))

    if commitment_index == "last":
        coinbase_tx.vout.extend(commitment_outputs)
    elif commitment_index == "first":
        coinbase_tx.vout = commitment_outputs + coinbase_tx.vout
    else:
        idx = int(commitment_index)
        coinbase_tx.vout[idx:idx] = commitment_outputs

    coinbase_tx.rehash()
    block = build_block_from_template(tmpl, coinbase_tx)
    block.solve()
    return block


def submit_and_check(node, block, expect_accept, reject_reason=None):
    """Submit a hand-built block and assert it was accepted/rejected as tip.

    submitblock() returns None on acceptance, or a reject-reason string on
    rejection (also logged); we check both the RPC result and the resulting
    tip, matching the idiom used by test/functional/mining_basic.py.
    """
    result = node.submitblock(hexdata=block.serialize().hex())
    if expect_accept:
        assert_equal(result, None)
        assert_equal(node.getbestblockhash(), block.hash)
    else:
        if reject_reason is not None:
            assert_equal(result, reject_reason)
        else:
            assert result is not None
        assert node.getbestblockhash() != block.hash
    return result
