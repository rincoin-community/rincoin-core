#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Generate src/test/data/s6b_sighash.json.

Independent re-implementation (hashlib only, no project code) of the signature
hashes around the height-840,000 transition. The C++ unit test
s6b_sighash_tests.cpp checks SignatureHash() against this file, so the expected
digests do not come from the code under test.

Specification (consensus-840k, technology/consensus-transition.md section 5):
from the transition height every ECDSA signature sets SIGHASH_FORKID (0x40) and is
hashed with the BIP143 algorithm, for pre-SegWit inputs as well, with the fork ID
840 in the upper three bytes of the four-byte hash type that ends the preimage.
This is the scheme of Bitcoin Gold (fork ID 79) and, with fork ID 0, Bitcoin Cash.

The file has two kinds of entries:
 - "vector": digests for a fixed transaction, every hash type, both script
   versions, both sides of the transition;
 - "bitcoin_gold": signatures of real Bitcoin Gold main-chain transactions (a
   pre-SegWit input and a SegWit v0 one). They are verified here (pure-Python ECDSA) with fork ID 79 before they are written,
   and the unit test verifies them again through SignatureHash() and the script
   interpreter. They show that the construction is the deployed one, not a
   look-alike.

Usage: test/util/gen_s6b_sighash_vectors.py > src/test/data/s6b_sighash.json
"""
import hashlib
import json
import struct

FORK_ID = 840
assert 0 < FORK_ID < (1 << 23)
BITCOIN_GOLD_FORK_ID = 79

SIGHASH_ALL, SIGHASH_NONE, SIGHASH_SINGLE, SIGHASH_FORKID, SIGHASH_ANYONECANPAY = 1, 2, 3, 0x40, 0x80


def sha256d(b):
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()


def ser_varint(n):
    if n < 0xfd:
        return bytes([n])
    assert n <= 0xffff
    return b"\xfd" + struct.pack("<H", n)


def ser_txin(prevout_hash, prevout_n, script_sig, sequence):
    return prevout_hash + struct.pack("<I", prevout_n) + ser_varint(len(script_sig)) + script_sig + struct.pack("<I", sequence)


def ser_txout(value, script):
    return struct.pack("<q", value) + ser_varint(len(script)) + script


def ser_tx(version, vin, vout, locktime):
    out = struct.pack("<i", version) + ser_varint(len(vin))
    for i in vin:
        out += ser_txin(*i)
    out += ser_varint(len(vout))
    for o in vout:
        out += ser_txout(*o)
    return out + struct.pack("<I", locktime)


def legacy_sighash(tx, script_code, idx, hash_type):
    """The historical pre-SegWit signature hash."""
    version, vin, vout, locktime = tx
    base = hash_type & 0x1f
    if base == SIGHASH_SINGLE and idx >= len(vout):
        return (1).to_bytes(32, "little")  # the historical constant digest
    new_vin = []
    for n, (h, i, _sig, seq) in enumerate(vin):
        if n == idx:
            new_vin.append((h, i, script_code, seq))
        else:
            new_vin.append((h, i, b"", 0 if base in (SIGHASH_NONE, SIGHASH_SINGLE) else seq))
    if base == SIGHASH_NONE:
        new_vout = []
    elif base == SIGHASH_SINGLE:
        new_vout = [(-1, b"")] * idx + [vout[idx]]
    else:
        new_vout = list(vout)
    if hash_type & SIGHASH_ANYONECANPAY:
        new_vin = [new_vin[idx]]
    return sha256d(ser_tx(version, new_vin, new_vout, locktime) + struct.pack("<I", hash_type))


def bip143_sighash(tx, script_code, idx, hash_type, amount, hash_type_field):
    """BIP143. hash_type selects the mode, hash_type_field is what is serialized."""
    version, vin, vout, locktime = tx
    base = hash_type & 0x1f
    zero = b"\x00" * 32
    hash_prevouts = hash_sequence = hash_outputs = zero
    if not hash_type & SIGHASH_ANYONECANPAY:
        hash_prevouts = sha256d(b"".join(h + struct.pack("<I", i) for (h, i, _s, _q) in vin))
    if not hash_type & SIGHASH_ANYONECANPAY and base not in (SIGHASH_SINGLE, SIGHASH_NONE):
        hash_sequence = sha256d(b"".join(struct.pack("<I", q) for (_h, _i, _s, q) in vin))
    if base not in (SIGHASH_SINGLE, SIGHASH_NONE):
        hash_outputs = sha256d(b"".join(ser_txout(*o) for o in vout))
    elif base == SIGHASH_SINGLE and idx < len(vout):
        hash_outputs = sha256d(ser_txout(*vout[idx]))
    h, i, _s, seq = vin[idx]
    pre = (struct.pack("<i", version) + hash_prevouts + hash_sequence + h + struct.pack("<I", i)
           + ser_varint(len(script_code)) + script_code + struct.pack("<q", amount) + struct.pack("<I", seq)
           + hash_outputs + struct.pack("<I", locktime) + struct.pack("<I", hash_type_field))
    return sha256d(pre), pre


def signature_hash(tx, script_code, idx, hash_type, amount, sigversion, fork_active, fork_id=FORK_ID):
    """The rule of the transition: which algorithm, and what ends the preimage."""
    use_forkid = fork_active and bool(hash_type & SIGHASH_FORKID)
    if sigversion == "witness_v0" or use_forkid:
        field = hash_type | (fork_id << 8) if use_forkid else hash_type
        return bip143_sighash(tx, script_code, idx, hash_type, amount, field)[0]
    return legacy_sighash(tx, script_code, idx, hash_type)


# --- pure-Python secp256k1 ECDSA verification, used only for the reference signatures ---

P = 2**256 - 2**32 - 977
N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
G = (0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798,
     0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8)


def ec_add(a, b):
    if a is None:
        return b
    if b is None:
        return a
    if a[0] == b[0] and (a[1] + b[1]) % P == 0:
        return None
    if a == b:
        lam = 3 * a[0] * a[0] * pow(2 * a[1], -1, P) % P
    else:
        lam = (b[1] - a[1]) * pow(b[0] - a[0], -1, P) % P
    x = (lam * lam - a[0] - b[0]) % P
    return x, (lam * (a[0] - x) - a[1]) % P


def ec_mul(k, point):
    result = None
    while k:
        if k & 1:
            result = ec_add(result, point)
        point = ec_add(point, point)
        k >>= 1
    return result


def decode_pubkey(b):
    if b[0] == 4:
        return int.from_bytes(b[1:33], "big"), int.from_bytes(b[33:65], "big")
    x = int.from_bytes(b[1:33], "big")
    y = pow((x**3 + 7) % P, (P + 1) // 4, P)
    return (x, y) if (y & 1) == (b[0] & 1) else (x, P - y)


def ecdsa_verify(pubkey, digest, der):
    assert der[0] == 0x30 and der[2] == 0x02
    r_len = der[3]
    r = int.from_bytes(der[4:4 + r_len], "big")
    assert der[4 + r_len] == 0x02
    s_len = der[5 + r_len]
    s = int.from_bytes(der[6 + r_len:6 + r_len + s_len], "big")
    w = pow(s, -1, N)
    point = ec_add(ec_mul(int.from_bytes(digest, "big") * w % N, G), ec_mul(r * w % N, decode_pubkey(pubkey)))
    return point is not None and point[0] % N == r


def parse_tx(raw):
    def varint(i):
        n = raw[i]
        if n < 0xfd:
            return n, i + 1
        assert n == 0xfd
        return struct.unpack_from("<H", raw, i + 1)[0], i + 3
    i = 4
    version = struct.unpack_from("<i", raw, 0)[0]
    segwit = raw[i] == 0 and raw[i + 1] == 1
    if segwit:
        i += 2
    count, i = varint(i)
    vin = []
    for _ in range(count):
        h, n = raw[i:i + 32], struct.unpack_from("<I", raw, i + 32)[0]
        length, i = varint(i + 36)
        script_sig = raw[i:i + length]
        i += length
        vin.append((h, n, script_sig, struct.unpack_from("<I", raw, i)[0]))
        i += 4
    count, i = varint(i)
    vout = []
    for _ in range(count):
        value = struct.unpack_from("<q", raw, i)[0]
        length, i = varint(i + 8)
        vout.append((value, raw[i:i + length]))
        i += length
    witness = []
    if segwit:
        for _ in vin:
            items, i = varint(i)
            stack = []
            for _ in range(items):
                length, i = varint(i)
                stack.append(raw[i:i + length])
                i += length
            witness.append(stack)
    return (version, vin, vout, struct.unpack_from("<I", raw, i)[0]), witness


def script_pushes(script):
    out, i = [], 0
    while i < len(script):
        op = script[i]
        assert 1 <= op <= 75
        out.append(script[i + 1:i + 1 + op])
        i += 1 + op
    return out


# Real Bitcoin Gold main-chain transactions (raw transaction, and for every input the
# output it spends). Source: a public Bitcoin Gold block explorer, 2026-09-21.
BITCOIN_GOLD_TRANSACTIONS = [
    {
        "txid": "4a8740a5698c41e49bddcb9dba2e7da5abf7bed30cd0154f44f844fd36a13ebe",
        "height": 965449,
        "hex": "0100000001ce642221de97bf4ed10b96958a1e83d5bebd15639c692a7f34ca6631c431c1e0010000006a473044022036cc1b93191f3a82b8b49bb6ab5b028342b111ad0823e39c7ef786cde61487c7022042025ef431c176c670f2f1292717c1508a16ffcef2df74b3bbd07e58390466ff41210311855946b15a1049a3fbc30b7b0bf93de75aefeeaa4141d4cdfce2b9b79ef4caffffffff0200e1f505000000001976a9147b0f6df0b901a4ba55d76df7761555959f593e8988ac5dc36528000000001976a9149953b8b150a904d28943f5f0826bd73b024605e388ac00000000",
        "spent": [("76a9149953b8b150a904d28943f5f0826bd73b024605e388ac", 777760272)],
    },
    {
        "txid": "b7991cdbfe5b368d4f0fe5d1060d56fb6d36b2c384aa3a4f1b0d6a4445658d27",
        "height": 965232,
        "hex": "020000000001019ef04c2d3af8d2af151e0599374a184b13a7f1b680cfc8297366199391eab1c00000000017160014d4674d03dfe8b58418e7a4bdd1b076850db1b4a3feffffff024ee33d77050000001976a914a328212acb6ac57e08353a261ad4a717951a63ce88ac8966b4190b00000017a9141503951b51d74114fd0b4aa66556c7a0254998ce870247304402204ac71ea89804ee2c0331bf1e9b5ec32409969426549ad27b307e1657de33de8a02200ee620ac0bb0b1c028ea2d0e960d2d00d657a13e83b6a85213d5f0eb3ead306e4121032d9fc2e0e9cb96ddf1a989b30e08ba0a31ef8b1612066fb461e0d3fa1f5f0c4d6fba0e00",
        "spent": [("a9144678aac550dff1c438b7ab3dcd2780922691e4d087", 71151276082)],
    },
]


def bitcoin_gold_cases():
    cases = []
    for ref in BITCOIN_GOLD_TRANSACTIONS:
        raw = bytes.fromhex(ref["hex"])
        tx, witness = parse_tx(raw)
        # the transaction ID is the hash of the serialization without witness data
        assert sha256d(ser_tx(*tx))[::-1].hex() == ref["txid"]
        for idx, (spk_hex, amount) in enumerate(ref["spent"]):
            spk = bytes.fromhex(spk_hex)
            if spk[:3] == bytes.fromhex("76a914") and len(spk) == 25:
                sigversion, script_code = "base", spk
                sig, pubkey = script_pushes(tx[1][idx][2])
            elif spk[:2] == bytes.fromhex("0014") and len(spk) == 22:
                sigversion, script_code = "witness_v0", bytes.fromhex("76a914") + spk[2:] + bytes.fromhex("88ac")
                sig, pubkey = witness[idx]
            elif spk[:2] == bytes.fromhex("a914") and len(spk) == 23:
                # P2SH-P2WPKH: the scriptSig pushes the witness program
                (program,) = script_pushes(tx[1][idx][2])
                assert program[:2] == bytes.fromhex("0014") and len(program) == 22
                sigversion, script_code = "witness_v0", bytes.fromhex("76a914") + program[2:] + bytes.fromhex("88ac")
                sig, pubkey = witness[idx]
            else:
                raise AssertionError("unsupported reference input")
            hash_type = sig[-1]
            assert hash_type & SIGHASH_FORKID
            digest = signature_hash(tx, script_code, idx, hash_type, amount, sigversion, True, BITCOIN_GOLD_FORK_ID)
            assert ecdsa_verify(pubkey, digest, sig[:-1]), "reference signature does not verify"
            # ... and only with Bitcoin Gold's fork ID
            for other in (0, FORK_ID):
                other_digest = signature_hash(tx, script_code, idx, hash_type, amount, sigversion, True, other)
                assert not ecdsa_verify(pubkey, other_digest, sig[:-1])
            cases.append({
                "kind": "bitcoin_gold",
                "txid": ref["txid"],
                "height": ref["height"],
                "tx": ref["hex"],
                "input_index": idx,
                "script_pubkey": spk_hex,
                "script_code": script_code.hex(),
                "amount": amount,
                "sigversion": sigversion,
                "fork_id": BITCOIN_GOLD_FORK_ID,
                "hash_type": hash_type,
                "pubkey": pubkey.hex(),
                "signature": sig.hex(),
                "sighash": digest.hex(),
            })
    return cases


def main():
    # Fixed transaction: three inputs, two outputs, so input 2 has no matching output.
    vin = [
        (hashlib.sha256(b"rincoin s6b vector input 0").digest(), 0, b"", 0xfffffffe),
        (hashlib.sha256(b"rincoin s6b vector input 1").digest(), 7, b"", 0xffffffff),
        (hashlib.sha256(b"rincoin s6b vector input 2").digest(), 1, b"", 0x00000005),
    ]
    p2pkh = bytes.fromhex("76a914") + hashlib.sha256(b"rincoin s6b vector key").digest()[:20] + bytes.fromhex("88ac")
    vout = [
        (123456789, p2pkh),
        (4 * 100000000, bytes.fromhex("0014") + hashlib.sha256(b"rincoin s6b vector wpkh").digest()[:20]),
    ]
    tx = (2, vin, vout, 839999)
    raw = ser_tx(*tx)
    script_code = p2pkh
    amount = 987654321

    # The preimage of a replay-protected SIGHASH_ALL signature ends in 41 48 03 00.
    assert bip143_sighash(tx, script_code, 0, 0x41, amount, 0x41 | (FORK_ID << 8))[1][-4:] == bytes.fromhex("41480300")

    entries = []
    base_types = (1, 2, 3, 0x81, 0x82, 0x83)
    for sigversion in ("base", "witness_v0"):
        for hash_type in base_types + tuple(t | SIGHASH_FORKID for t in base_types):
            for idx in range(len(vin)):
                for fork_active in (False, True):
                    digest = signature_hash(tx, script_code, idx, hash_type, amount, sigversion, fork_active)
                    entries.append({
                        "kind": "vector",
                        "tx": raw.hex(),
                        "script_code": script_code.hex(),
                        "input_index": idx,
                        "hash_type": hash_type,
                        "amount": amount,
                        "sigversion": sigversion,
                        "fork_active": fork_active,
                        "fork_id": FORK_ID,
                        "sighash": digest.hex(),
                    })
    entries += bitcoin_gold_cases()
    print(json.dumps(entries, indent=1))


if __name__ == "__main__":
    main()
