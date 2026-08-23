#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test-side mirrors of LegacySignatureHash/SegwitV0SignatureHash
(test_framework/script.py) with sig_fork_id mixed in, matching the
consensus/s1-testing branch's SignatureHash() changes in
src/script/interpreter.cpp (B.4): the 8-byte sig_fork_id is appended as the
last field of the preimage, after nHashType, in both the legacy and BIP143
sighash constructions -- and nowhere else (tx nVersion is never touched).

These exist purely so functional tests can construct "old-style" (pre-fork
digest) and "new-style" (post-fork digest) signatures on demand to exercise
both sides of the replay-protection rule, without needing a wallet that
already knows about sig_fork_id.
"""

import struct

from test_framework.messages import CTransaction, CTxOut, hash256, ser_string, ser_uint256, uint256_from_str
from test_framework.script import (
    SIGHASH_ANYONECANPAY,
    SIGHASH_NONE,
    SIGHASH_SINGLE,
    FindAndDelete,
    OP_CODESEPARATOR,
    CScript,
)


def LegacySignatureHashForkId(script, txTo, inIdx, hashtype, sig_fork_id):
    """LegacySignatureHash() with sig_fork_id appended, or the unmodified
    upstream digest if sig_fork_id is None (pre-H1 / old-style signing)."""
    txtmp = CTransaction(txTo)

    for txin in txtmp.vin:
        txin.scriptSig = b''
    txtmp.vin[inIdx].scriptSig = FindAndDelete(script, CScript([OP_CODESEPARATOR]))

    if (hashtype & 0x1f) == SIGHASH_NONE:
        txtmp.vout = []
        for i in range(len(txtmp.vin)):
            if i != inIdx:
                txtmp.vin[i].nSequence = 0
    elif (hashtype & 0x1f) == SIGHASH_SINGLE:
        outIdx = inIdx
        tmp = txtmp.vout[outIdx]
        txtmp.vout = []
        for _ in range(outIdx):
            txtmp.vout.append(CTxOut(-1))
        txtmp.vout.append(tmp)
        for i in range(len(txtmp.vin)):
            if i != inIdx:
                txtmp.vin[i].nSequence = 0

    if hashtype & SIGHASH_ANYONECANPAY:
        tmp = txtmp.vin[inIdx]
        txtmp.vin = []
        txtmp.vin.append(tmp)

    s = txtmp.serialize_without_witness()
    s += struct.pack(b"<I", hashtype)
    if sig_fork_id is not None:
        assert len(sig_fork_id) == 8
        s += sig_fork_id

    return hash256(s)


def SegwitV0SignatureHashForkId(script, txTo, inIdx, hashtype, amount, sig_fork_id):
    """SegwitV0SignatureHash() (BIP143) with sig_fork_id appended, or the
    unmodified upstream digest if sig_fork_id is None."""
    hashPrevouts = 0
    hashSequence = 0
    hashOutputs = 0

    if not (hashtype & SIGHASH_ANYONECANPAY):
        serialize_prevouts = bytes()
        for i in txTo.vin:
            serialize_prevouts += i.prevout.serialize()
        hashPrevouts = uint256_from_str(hash256(serialize_prevouts))

    if (not (hashtype & SIGHASH_ANYONECANPAY) and (hashtype & 0x1f) != SIGHASH_SINGLE and (hashtype & 0x1f) != SIGHASH_NONE):
        serialize_sequence = bytes()
        for i in txTo.vin:
            serialize_sequence += struct.pack("<I", i.nSequence)
        hashSequence = uint256_from_str(hash256(serialize_sequence))

    if ((hashtype & 0x1f) != SIGHASH_SINGLE and (hashtype & 0x1f) != SIGHASH_NONE):
        serialize_outputs = bytes()
        for o in txTo.vout:
            serialize_outputs += o.serialize()
        hashOutputs = uint256_from_str(hash256(serialize_outputs))
    elif ((hashtype & 0x1f) == SIGHASH_SINGLE and inIdx < len(txTo.vout)):
        serialize_outputs = txTo.vout[inIdx].serialize()
        hashOutputs = uint256_from_str(hash256(serialize_outputs))

    ss = bytes()
    ss += struct.pack("<i", txTo.nVersion)
    ss += ser_uint256(hashPrevouts)
    ss += ser_uint256(hashSequence)
    ss += txTo.vin[inIdx].prevout.serialize()
    ss += ser_string(script)
    ss += struct.pack("<q", amount)
    ss += struct.pack("<I", txTo.vin[inIdx].nSequence)
    ss += ser_uint256(hashOutputs)
    ss += struct.pack("<i", txTo.nLockTime)
    ss += struct.pack("<I", hashtype)
    if sig_fork_id is not None:
        assert len(sig_fork_id) == 8
        ss += sig_fork_id

    return hash256(ss)
