#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Helpers for the height-840,000 transition tests (feature_s6b_*.py).

Regtest activates the transition at height 840 (4 halving intervals of 210). The
signature helpers wrap the test framework's own digests, so tests can produce "historical"
signatures and replay-protected ones (SIGHASH_FORKID) on demand, independently of the
node's wallet. Everywhere below `hashtype` is the hash type without SIGHASH_FORKID, and
`fork_active` says for which side of the transition height the signature is made.
"""

from decimal import Decimal

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE, key_to_p2wpkh
from test_framework.blocktools import (
    REGTEST_S6B_HEIGHT,
    add_witness_commitment,
    create_block,
    create_coinbase,
    regtest_block_subsidy,
)
from test_framework.key import (
    ECKey,
    compute_xonly_pubkey,
    generate_privkey,
    sign_schnorr,
    tweak_add_privkey,
)
from test_framework.messages import (
    COIN,
    CBlock,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxInWitness,
    CTxOut,
    FromHex,
)
from test_framework.script import (
    OP_0,
    OP_2,
    OP_3,
    OP_CHECKMULTISIG,
    OP_CHECKSIG,
    OP_TRUE,
    SIG_FORK_ID_840K,
    SIGHASH_ALL,
    SIGHASH_ANYONECANPAY,
    SIGHASH_DEFAULT,
    SIGHASH_FORKID,
    SIGHASH_NONE,
    SIGHASH_SINGLE,
    CScript,
    ForkIdSignatureHash,
    LegacySignatureHash,
    SegwitV0SignatureHash,
    TaprootSignatureHash,
    taproot_construct,
)
from test_framework.script_util import (
    key_to_p2pkh_script,
    key_to_p2wpkh_script,
    script_to_p2sh_script,
    script_to_p2wsh_script,
)
from test_framework.util import assert_equal

S6B_HEIGHT = REGTEST_S6B_HEIGHT
SIG_FORK_ID = SIG_FORK_ID_840K

# The digest that a legacy SIGHASH_SINGLE signature covers when its input has no matching output.
SIGHASH_SINGLE_BUG_DIGEST = (1).to_bytes(32, "little")

ALL_HASH_TYPES = [
    SIGHASH_ALL,
    SIGHASH_NONE,
    SIGHASH_SINGLE,
    SIGHASH_ALL | SIGHASH_ANYONECANPAY,
    SIGHASH_NONE | SIGHASH_ANYONECANPAY,
    SIGHASH_SINGLE | SIGHASH_ANYONECANPAY,
]


def sig_hashtype(hashtype, fork_active):
    """The hash type byte of a signature: SIGHASH_FORKID is part of it from the transition height."""
    return hashtype | SIGHASH_FORKID if fork_active else hashtype


def legacy_sighash(script, tx_to, in_idx, hashtype, amount, fork_active):
    """Signature hash of a pre-SegWit input.

    Historical: the legacy digest (for SIGHASH_SINGLE without a matching output that is
    the constant SIGHASH_SINGLE_BUG_DIGEST). From the transition height: the BIP143 digest
    with the fork ID in the hash type, which also commits to the amount of the input.
    """
    if fork_active:
        return ForkIdSignatureHash(script, tx_to, in_idx, sig_hashtype(hashtype, True), amount, SIG_FORK_ID)
    return LegacySignatureHash(script, tx_to, in_idx, hashtype)[0]


def segwit_v0_sighash(script, tx_to, in_idx, hashtype, amount, fork_active):
    """Signature hash of a SegWit v0 input: BIP143, with the fork ID in the hash type from the transition height."""
    if fork_active:
        return ForkIdSignatureHash(script, tx_to, in_idx, sig_hashtype(hashtype, True), amount, SIG_FORK_ID)
    return SegwitV0SignatureHash(script, tx_to, in_idx, hashtype, amount)


def der_sig(key, digest, hashtype=SIGHASH_ALL):
    """An ECDSA signature over digest with the given hash type byte (see sig_hashtype())."""
    return key.sign_ecdsa(digest, low_s=True) + bytes([hashtype])


def mine_to_height(node, height, addr=None, batch=500):
    """Generate blocks with the node's own miner until the tip reaches `height`.

    addr defaults to an address outside of every test wallet. Blocks are generated in
    batches so that a single RPC call stays well within the RPC timeout."""
    addr = addr or ADDRESS_BCRT1_UNSPENDABLE
    current = node.getblockcount()
    assert current <= height, "tip %d is already above %d" % (current, height)
    while current < height:
        n = min(batch, height - current)
        node.generatetoaddress(n, addr)
        current += n
    assert_equal(node.getblockcount(), height)


def build_block(node, txs=None, fees=0, coinbase_value=None, coinbase_values=None, coinbase_script_sig_extra=None,
                extra_coinbase_outputs=None, prev_hash=None, prev_height=None, prev_time=None):
    """Hand-build a block on the node's tip (or on the given parent).

    fees: total fees of `txs` (the caller knows them); the coinbase claims
          regtest subsidy + fees unless coinbase_value/coinbase_values say otherwise.
    coinbase_values: list of output values replacing the single paying output.
    coinbase_script_sig_extra: bytes appended to the coinbase scriptSig.
    extra_coinbase_outputs: list of CTxOut appended to the coinbase.
    """
    txs = txs or []
    if prev_hash is None:
        tip = node.getblock(node.getbestblockhash())
        prev_hash = tip["hash"]
        prev_height = tip["height"]
        prev_time = tip["time"]
    height = prev_height + 1
    coinbase = create_coinbase(height, fees=fees)
    if coinbase_value is not None:
        coinbase.vout[0].nValue = coinbase_value
    if coinbase_values is not None:
        script = coinbase.vout[0].scriptPubKey
        coinbase.vout = [CTxOut(v, script) for v in coinbase_values]
    if extra_coinbase_outputs:
        coinbase.vout.extend(extra_coinbase_outputs)
    if coinbase_script_sig_extra:
        coinbase.vin[0].scriptSig = CScript(bytes(coinbase.vin[0].scriptSig) + coinbase_script_sig_extra)
    coinbase.rehash()
    block = create_block(int(prev_hash, 16), coinbase, prev_time + 1, version=0x20000000)
    for tx in txs:
        tx.rehash()
        block.vtx.append(tx)
    block.hashMerkleRoot = block.calc_merkle_root()
    if any(not tx.wit.is_null() for tx in txs):
        add_witness_commitment(block)
    block.solve()
    return block


def remine_with(node, mutate):
    """Let the node mine the next block, take it off the chain again, and return
    (changed_copy, original_hash): the copy has mutate(block) applied and is re-solved.

    Works at every height, including those where blocks carry MWEB data that a test
    cannot build by hand. node.reconsiderblock(original_hash) restores the original."""
    # A fresh address each time: with the same address, the same parent and a timestamp that fast
    # mining has pushed to median-time-past + 1, the node would re-create a block it has marked invalid.
    original_hash = node.generatetoaddress(1, key_to_p2wpkh(new_key().get_pubkey().get_bytes()))[0]
    block = FromHex(CBlock(), node.getblock(original_hash, 0))
    node.invalidateblock(original_hash)
    mutate(block)
    block.vtx[0].rehash()
    block.hashMerkleRoot = block.calc_merkle_root()
    block.solve()
    return block, original_hash


def submit(node, block):
    """submitblock; returns None on acceptance or the reject reason."""
    return node.submitblock(block.serialize().hex())


def assert_block_accepted(node, block):
    assert_equal(submit(node, block), None)
    assert_equal(node.getbestblockhash(), block.hash)


def assert_block_rejected(node, block, reason=None, exact=True):
    """submitblock must fail and leave the tip alone. With exact=False, `reason` only
    has to be contained in the reject reason (script errors carry details)."""
    tip = node.getbestblockhash()
    result = submit(node, block)
    assert result is not None, "block unexpectedly accepted"
    if reason is not None:
        if exact:
            assert_equal(result, reason)
        else:
            assert reason in result, "expected %r in reject reason %r" % (reason, result)
    assert_equal(node.getbestblockhash(), tip)
    return result


def subsidy(height):
    return regtest_block_subsidy(height)


# ---------------------------------------------------------------------------------------------
# Outputs of every common script type, spendable under either signature-hash regime
# ---------------------------------------------------------------------------------------------

# Pays to nobody in particular: spendable with an empty scriptSig (non-standard, for hand-built blocks).
ANYONE_CAN_SPEND = CScript([OP_TRUE])
# A standard output script for transactions that have to pass the mempool's standardness checks:
# the P2WPKH script of ADDRESS_BCRT1_UNSPENDABLE.
STANDARD_OUTPUT_SCRIPT = CScript([OP_0, bytes(20)])

LEGACY_KINDS = ("p2pk", "p2pkh", "multisig", "p2sh_multisig")
WITNESS_V0_KINDS = ("p2wpkh", "p2wsh_multisig", "p2sh_p2wpkh", "p2sh_p2wsh_multisig")
ECDSA_KINDS = LEGACY_KINDS + WITNESS_V0_KINDS
TAPROOT_KIND = "p2tr"

TAPROOT_HASH_TYPES = [SIGHASH_DEFAULT] + ALL_HASH_TYPES


def new_key():
    key = ECKey()
    key.generate()
    return key


class TestOutput:
    """A scriptPubKey of the given kind with fresh keys, and the recipe to spend it.

    After fund_outputs() it also knows its outpoint and amount. sign() fills in the
    scriptSig/witness of one transaction input, with historical signatures
    (fork_active=False) or replay-protected ones (fork_active=True). Taproot
    signatures do not depend on fork_active.
    """

    def __init__(self, kind, amount=COIN):
        self.kind = kind
        self.amount = amount
        self.outpoint = None
        self.redeem_script = None   # P2SH redeem script (pushed in the scriptSig)
        self.witness_script = None  # P2WSH witness script (last witness element)
        if kind == TAPROOT_KIND:
            self.sec = generate_privkey()
            self.xonly, _ = compute_xonly_pubkey(self.sec)
            self.tap = taproot_construct(self.xonly)
            self.script_pubkey = self.tap.scriptPubKey
            return
        if "multisig" in kind:
            self.keys = [new_key() for _ in range(3)]
            pubkeys = [k.get_pubkey().get_bytes() for k in self.keys]
            multisig = CScript([OP_2] + pubkeys + [OP_3, OP_CHECKMULTISIG])
        else:
            self.keys = [new_key()]
            pubkey = self.keys[0].get_pubkey().get_bytes()
        if kind == "p2pk":
            self.script_pubkey = CScript([pubkey, OP_CHECKSIG])
            self.script_code = self.script_pubkey
        elif kind == "p2pkh":
            self.script_pubkey = key_to_p2pkh_script(pubkey)
            self.script_code = self.script_pubkey
        elif kind == "multisig":
            self.script_pubkey = multisig
            self.script_code = multisig
        elif kind == "p2sh_multisig":
            self.redeem_script = multisig
            self.script_pubkey = script_to_p2sh_script(multisig)
            self.script_code = multisig
        elif kind == "p2wpkh":
            self.script_pubkey = key_to_p2wpkh_script(pubkey)
            self.script_code = key_to_p2pkh_script(pubkey)
        elif kind == "p2wsh_multisig":
            self.witness_script = multisig
            self.script_pubkey = script_to_p2wsh_script(multisig)
            self.script_code = multisig
        elif kind == "p2sh_p2wpkh":
            self.redeem_script = key_to_p2wpkh_script(pubkey)
            self.script_pubkey = script_to_p2sh_script(self.redeem_script)
            self.script_code = key_to_p2pkh_script(pubkey)
        elif kind == "p2sh_p2wsh_multisig":
            self.witness_script = multisig
            self.redeem_script = script_to_p2wsh_script(multisig)
            self.script_pubkey = script_to_p2sh_script(self.redeem_script)
            self.script_code = multisig
        else:
            raise ValueError(kind)

    @property
    def is_witness(self):
        return self.kind in WITNESS_V0_KINDS or self.kind == TAPROOT_KIND

    @property
    def txout(self):
        return CTxOut(self.amount, self.script_pubkey)

    def digest(self, tx, in_idx, hashtype, fork_active):
        if self.kind in WITNESS_V0_KINDS:
            return segwit_v0_sighash(self.script_code, tx, in_idx, hashtype, self.amount, fork_active)
        return legacy_sighash(self.script_code, tx, in_idx, hashtype, self.amount, fork_active)

    def sign(self, tx, in_idx, hashtype, fork_active, spent_outputs=None):
        """Sign input in_idx of tx. spent_outputs (list of CTxOut, one per input) is
        only needed for Taproot inputs of multi-input transactions."""
        while len(tx.wit.vtxinwit) < len(tx.vin):
            tx.wit.vtxinwit.append(CTxInWitness())
        if self.kind == TAPROOT_KIND:
            spent = spent_outputs if spent_outputs is not None else [self.txout]
            sighash = TaprootSignatureHash(tx, spent, hashtype, in_idx)
            sig = sign_schnorr(tweak_add_privkey(self.sec, self.tap.tweak), sighash)
            if hashtype != SIGHASH_DEFAULT:
                sig += bytes([hashtype])
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = [sig]
            tx.vin[in_idx].scriptSig = CScript()
            tx.rehash()
            return
        digest = self.digest(tx, in_idx, hashtype, fork_active)
        sigs = [der_sig(k, digest, sig_hashtype(hashtype, fork_active)) for k in self.keys[:2]]
        pubkey = self.keys[0].get_pubkey().get_bytes()
        if self.kind == "p2pk":
            tx.vin[in_idx].scriptSig = CScript([sigs[0]])
        elif self.kind == "p2pkh":
            tx.vin[in_idx].scriptSig = CScript([sigs[0], pubkey])
        elif self.kind == "multisig":
            tx.vin[in_idx].scriptSig = CScript([OP_0] + sigs)
        elif self.kind == "p2sh_multisig":
            tx.vin[in_idx].scriptSig = CScript([OP_0] + sigs + [bytes(self.redeem_script)])
        elif self.kind == "p2wpkh":
            tx.vin[in_idx].scriptSig = CScript()
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = [sigs[0], pubkey]
        elif self.kind == "p2wsh_multisig":
            tx.vin[in_idx].scriptSig = CScript()
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = [b""] + sigs + [bytes(self.witness_script)]
        elif self.kind == "p2sh_p2wpkh":
            tx.vin[in_idx].scriptSig = CScript([bytes(self.redeem_script)])
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = [sigs[0], pubkey]
        elif self.kind == "p2sh_p2wsh_multisig":
            tx.vin[in_idx].scriptSig = CScript([bytes(self.redeem_script)])
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = [b""] + sigs + [bytes(self.witness_script)]
        tx.rehash()

    def spend(self, hashtype, fork_active, fee=10000, script_pubkey=ANYONE_CAN_SPEND, n_outputs=1):
        """A signed transaction spending this output to n_outputs equal outputs."""
        assert self.outpoint is not None, "output is not funded"
        tx = CTransaction()
        tx.vin = [CTxIn(self.outpoint, nSequence=0xfffffffe)]
        value = (self.amount - fee) // n_outputs
        tx.vout = [CTxOut(value, script_pubkey) for _ in range(n_outputs)]
        self.sign(tx, 0, hashtype, fork_active)
        return tx


class NoSigOutput:
    """P2SH(OP_TRUE): an output whose (standard) spend carries no signature at all."""

    def __init__(self, amount=COIN):
        self.amount = amount
        self.redeem_script = CScript([OP_TRUE])
        self.script_pubkey = script_to_p2sh_script(self.redeem_script)
        self.outpoint = None

    @property
    def script_sig(self):
        return CScript([bytes(self.redeem_script)])

    def spend(self, fee=10000, script_pubkey=STANDARD_OUTPUT_SCRIPT):
        tx = CTransaction()
        tx.vin = [CTxIn(self.outpoint, self.script_sig, 0xfffffffe)]
        tx.vout = [CTxOut(self.amount - fee, script_pubkey)]
        tx.rehash()
        return tx


def fund_outputs(node, outputs, confirm=True, fee=50000):
    """Pay every output from the node's wallet in one transaction. An output is either a
    (scriptPubKey, amount) pair or an object with script_pubkey and amount attributes (such
    as TestOutput), whose outpoint attribute is then set. Returns the funding transaction."""
    targets = [out if isinstance(out, tuple) else (out.script_pubkey, out.amount) for out in outputs]
    need = sum(amount for _, amount in targets) + fee
    tx = CTransaction()
    total = 0
    for utxo in sorted(node.listunspent(1), key=lambda u: u["amount"], reverse=True):
        if not utxo["spendable"]:
            continue
        tx.vin.append(CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"])))
        total += int(utxo["amount"] * COIN)
        if total >= need:
            break
    assert total >= need, "wallet cannot fund %d base units" % need
    tx.vout = [CTxOut(amount, script) for script, amount in targets]
    if total - need > 100000:
        change_script = bytes.fromhex(node.getaddressinfo(node.getnewaddress())["scriptPubKey"])
        tx.vout.append(CTxOut(total - need, change_script))
    signed = node.signrawtransactionwithwallet(tx.serialize().hex())
    assert signed["complete"], signed
    funding = FromHex(CTransaction(), signed["hex"])
    funding.rehash()
    node.sendrawtransaction(signed["hex"], 0)
    for i, out in enumerate(outputs):
        if not isinstance(out, tuple):
            out.outpoint = COutPoint(funding.sha256, i)
    if confirm:
        node.generatetoaddress(1, ADDRESS_BCRT1_UNSPENDABLE)
        assert funding.hash not in node.getrawmempool()
    return funding


def mempool_accepts(node, tx):
    """testmempoolaccept for one transaction: (allowed, reject_reason)."""
    res = node.testmempoolaccept([tx.serialize().hex()], 0)[0]
    return res["allowed"], res.get("reject-reason")


def amount_to_decimal(value):
    return Decimal(value) / COIN
