#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test SIGHASH_SINGLE without a matching output across the height-840,000 transition.

In pre-SegWit scripts, a SIGHASH_SINGLE signature on an input that has no output at the
same index historically signs the constant digest 1. Such a signature does not commit
to the transaction at all: it fits any transaction, on any chain. From the transition
height on (840 on regtest) the quirk is out of reach. Every signature has to set
SIGHASH_FORKID, and a signature that does is hashed with the BIP143 algorithm, which has
no constant digest.

All of this is checked with real block validation (submitblock), and against the
mempool:
- below the transition height the historical behavior is unchanged, including a spend
  signed over the constant digest, also with the (then meaningless) bit 0x40 in its hash
  type, and an OP_NOT script satisfied by a failing signature;
- from the transition height on, a signature without SIGHASH_FORKID is a script error in
  OP_CHECKSIG, OP_CHECKSIGVERIFY, OP_CHECKMULTISIG and OP_CHECKMULTISIGVERIFY, bare and
  inside P2SH and P2WSH, whether it is the historically valid one or garbage, and
  whatever follows the opcode: a hard failure, not a check that returns false, which a
  script could invert with OP_NOT;
- the signature over the constant digest with SIGHASH_FORKID added to its hash type byte
  is just a signature that does not verify;
- a replay-protected SIGHASH_SINGLE signature for an input without a matching output is
  an ordinary signature that commits to no output, as it always was for SegWit v0;
- what stays as it was: empty signatures, failing signatures under OP_NOT, the other
  hash types; Taproot keeps rejecting SIGHASH_SINGLE without a matching output;
- the mempool and the signing RPC agree with the block rule;
- the boundary: a spend confirmed in the last block below the transition height stays
  valid through restart and -reindex, while the same kind of spend that is still in
  the mempool is removed and cannot be mined any more.
"""

from test_framework.key import ECKey
from test_framework.messages import COIN, CTransaction, CTxIn, CTxInWitness, CTxOut
from test_framework.s6b_util import (
    S6B_HEIGHT,
    SIGHASH_SINGLE_BUG_DIGEST,
    STANDARD_OUTPUT_SCRIPT,
    NoSigOutput,
    TestOutput,
    assert_block_accepted,
    assert_block_rejected,
    build_block,
    der_sig,
    fund_outputs,
    legacy_sighash,
    mempool_accepts,
    mine_to_height,
    segwit_v0_sighash,
    sig_hashtype,
)
from test_framework.script import (
    OP_0,
    OP_1,
    OP_2,
    OP_CHECKMULTISIG,
    OP_CHECKMULTISIGVERIFY,
    OP_CHECKSIG,
    OP_CHECKSIGVERIFY,
    OP_NOT,
    OP_TRUE,
    SIGHASH_ALL,
    SIGHASH_ANYONECANPAY,
    SIGHASH_FORKID,
    SIGHASH_NONE,
    SIGHASH_SINGLE,
    CScript,
)
from test_framework.script_util import script_to_p2sh_script, script_to_p2wsh_script
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error
from test_framework.wallet_util import bytes_to_wif

FEE = 10000
SINGLE_ACP = SIGHASH_SINGLE | SIGHASH_ANYONECANPAY

ERR_FORKID = "mandatory-script-verify-flag-failed (Signature must use SIGHASH_FORKID)"
# (for a witness program the node words a failed block check as "non-mandatory": the same spend would pass
# with the witness rules switched off; it is a consensus failure of the block all the same)
ERR_FORKID_WITNESS = "non-mandatory-script-verify-flag (Signature must use SIGHASH_FORKID)"
ERR_EVAL_FALSE = "mandatory-script-verify-flag-failed (Script evaluated without error but finished with a false/empty top stack element)"
ERR_CHECKSIGVERIFY = "mandatory-script-verify-flag-failed (Script failed an OP_CHECKSIGVERIFY operation)"
ERR_CHECKMULTISIGVERIFY = "mandatory-script-verify-flag-failed (Script failed an OP_CHECKMULTISIGVERIFY operation)"
ERR_TAPROOT_HASHTYPE = "Invalid Schnorr signature hash type"
OLD_STYLE = "old-style-sig-fork-id"
ACCEPT = None


def new_key():
    key = ECKey()
    key.generate()
    return key


class Script:
    """One of the scripts under test, with two keys, bare or wrapped in P2SH or P2WSH."""

    def __init__(self, template, wrap="bare", amount=COIN):
        self.template = template
        self.wrap = wrap
        self.amount = amount
        self.outpoint = None
        self.keys = [new_key(), new_key()]
        pk = [k.get_pubkey().get_bytes() for k in self.keys]
        self.script = {
            "checksig": CScript([pk[0], OP_CHECKSIG]),
            "checksig_not": CScript([pk[0], OP_CHECKSIG, OP_NOT]),
            "checksigverify": CScript([pk[0], OP_CHECKSIGVERIFY, OP_TRUE]),
            "multisig": CScript([OP_1, pk[0], pk[1], OP_2, OP_CHECKMULTISIG]),
            "multisig_not": CScript([OP_1, pk[0], pk[1], OP_2, OP_CHECKMULTISIG, OP_NOT]),
            "multisigverify": CScript([OP_1, pk[0], pk[1], OP_2, OP_CHECKMULTISIGVERIFY, OP_TRUE]),
            "multisig_2of2": CScript([OP_2, pk[0], pk[1], OP_2, OP_CHECKMULTISIG]),
        }[template]
        self.script_pubkey = {
            "bare": self.script,
            "p2sh": script_to_p2sh_script(self.script),
            "p2wsh": script_to_p2wsh_script(self.script),
        }[wrap]

    @property
    def is_multisig(self):
        return self.template.startswith("multisig")

    def digest(self, tx, in_idx, hashtype, fork_active):
        if self.wrap == "p2wsh":
            return segwit_v0_sighash(self.script, tx, in_idx, hashtype, self.amount, fork_active)
        return legacy_sighash(self.script, tx, in_idx, hashtype, self.amount, fork_active)

    def satisfy(self, tx, in_idx, sigs):
        """Put the signatures (bytes, possibly empty or garbage) into input in_idx."""
        items = ([b""] if self.is_multisig else []) + list(sigs)
        while len(tx.wit.vtxinwit) < len(tx.vin):
            tx.wit.vtxinwit.append(CTxInWitness())
        if self.wrap == "p2wsh":
            tx.wit.vtxinwit[in_idx].scriptWitness.stack = items + [bytes(self.script)]
        else:
            script_sig = [OP_0 if item == b"" else item for item in items]
            if self.wrap == "p2sh":
                script_sig.append(bytes(self.script))
            tx.vin[in_idx].scriptSig = CScript(script_sig)
        tx.rehash()


class S6bSighashSingleTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # -par=1: a failing script in a block is reported with its script error
        self.extra_args = [["-par=1"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def two_input_tx(self, first, second, n_outputs=1):
        """A transaction with `second` as input 1: without a matching output when n_outputs is 1."""
        tx = CTransaction()
        tx.vin = [CTxIn(first.outpoint, nSequence=0xfffffffe), CTxIn(second.outpoint, nSequence=0xfffffffe)]
        value = (first.amount + second.amount - FEE) // n_outputs
        tx.vout = [CTxOut(value, STANDARD_OUTPUT_SCRIPT) for _ in range(n_outputs)]
        tx.vin[0].scriptSig = first.script_sig
        tx.rehash()
        return tx

    def fee_of(self, tx, inputs):
        return sum(i.amount for i in inputs) - sum(o.nValue for o in tx.vout)

    def garbage_sig(self, hashtype):
        """A well-formed signature that verifies for no key of the scripts under test."""
        return der_sig(new_key(), bytes([7] * 32), hashtype)

    def run_cases(self, fork_active):
        """Every case as its own block on top of the current tip. Returns (txid, block hash) of the confirmed ones."""
        node = self.nodes[0]
        height = node.getblockcount() + 1
        assert_equal(height >= S6B_HEIGHT, fork_active)

        def one(script, hashtype=SIGHASH_SINGLE, key=0):
            # the signature that was valid historically: over the constant digest
            return lambda tx: [der_sig(script.keys[key], SIGHASH_SINGLE_BUG_DIGEST, hashtype)]

        def garbage(hashtype):
            return lambda tx: [self.garbage_sig(hashtype)]

        def empty():
            return lambda tx: [b""]

        def proper(script, hashtype, key=0):
            # a real signature for the regime of this block
            return lambda tx: [der_sig(script.keys[key], script.digest(tx, 1, hashtype, fork_active), sig_hashtype(hashtype, fork_active))]

        cases = []  # (description, script, signatures(tx), n_outputs, expected reject reason or ACCEPT)

        def add(description, script, sigs, expected_before, n_outputs=1, expected_after=ERR_FORKID):
            cases.append((description, script, sigs, n_outputs, expected_after if fork_active else expected_before))

        for wrap in ("bare", "p2sh"):
            for hashtype in (SIGHASH_SINGLE, SINGLE_ACP):
                name = "%s, hash type 0x%02x: " % (wrap, hashtype)
                flagged = hashtype | SIGHASH_FORKID
                # The historically valid signature over the constant digest
                s = Script("checksig", wrap)
                add(name + "CHECKSIG, signature over the constant digest", s, one(s, hashtype), ACCEPT)
                s = Script("checksigverify", wrap)
                add(name + "CHECKSIGVERIFY, signature over the constant digest", s, one(s, hashtype), ACCEPT)
                s = Script("multisig", wrap)
                add(name + "CHECKMULTISIG, signature over the constant digest", s, one(s, hashtype, key=1), ACCEPT)
                s = Script("multisigverify", wrap)
                add(name + "CHECKMULTISIGVERIFY, signature over the constant digest", s, one(s, hashtype), ACCEPT)
                # ... which a script ending in OP_NOT rejects as long as it verifies,
                s = Script("checksig_not", wrap)
                add(name + "CHECKSIG NOT, signature over the constant digest", s, one(s, hashtype), ERR_EVAL_FALSE)
                # ... while a failing signature satisfies it. After the transition a signature without
                # SIGHASH_FORKID must not become a way to spend: its check may not merely return false.
                s = Script("checksig_not", wrap)
                add(name + "CHECKSIG NOT, garbage signature", s, garbage(hashtype), ACCEPT)
                s = Script("multisig_not", wrap)
                add(name + "CHECKMULTISIG NOT, garbage signature", s, garbage(hashtype), ACCEPT)
                # A failing signature fails these scripts on both sides, but for a different reason
                s = Script("checksig", wrap)
                add(name + "CHECKSIG, garbage signature", s, garbage(hashtype), ERR_EVAL_FALSE)
                s = Script("checksigverify", wrap)
                add(name + "CHECKSIGVERIFY, garbage signature", s, garbage(hashtype), ERR_CHECKSIGVERIFY)
                s = Script("multisigverify", wrap)
                add(name + "CHECKMULTISIGVERIFY, garbage signature", s, garbage(hashtype), ERR_CHECKMULTISIGVERIFY)

                # The same signature over the constant digest with bit 0x40 in its hash type byte. Below the
                # transition height the bit means nothing to consensus and the digest is still the constant;
                # from it, the signature is hashed the BIP143 way and is not a signature of this transaction.
                s = Script("checksig", wrap)
                add(name + "CHECKSIG, signature over the constant digest, 0x40 set", s, one(s, flagged), ACCEPT, expected_after=ERR_EVAL_FALSE)
                s = Script("checksigverify", wrap)
                add(name + "CHECKSIGVERIFY, signature over the constant digest, 0x40 set", s, one(s, flagged), ACCEPT, expected_after=ERR_CHECKSIGVERIFY)
                s = Script("multisig", wrap)
                add(name + "CHECKMULTISIG, signature over the constant digest, 0x40 set", s, one(s, flagged, key=1), ACCEPT, expected_after=ERR_EVAL_FALSE)
                s = Script("checksig_not", wrap)
                add(name + "CHECKSIG NOT, signature over the constant digest, 0x40 set", s, one(s, flagged), ERR_EVAL_FALSE, expected_after=ACCEPT)

                # 2-of-2: a fine first signature does not excuse the second one
                s = Script("multisig_2of2", wrap)

                def two_sigs(tx, s=s, hashtype=hashtype):
                    first = der_sig(s.keys[0], s.digest(tx, 1, SIGHASH_ALL, fork_active), sig_hashtype(SIGHASH_ALL, fork_active))
                    return [first, der_sig(s.keys[1], SIGHASH_SINGLE_BUG_DIGEST, hashtype)]
                add(name + "2-of-2 CHECKMULTISIG, second signature over the constant digest", s, two_sigs, ACCEPT)

                # A real signature of this hash type for the regime of the block. Below the transition height
                # that is the signature over the constant digest again (so: accepted); from it, an ordinary
                # BIP143 signature that commits to no output.
                s = Script("checksig", wrap)
                add(name + "CHECKSIG, real signature, no matching output", s, proper(s, hashtype), ACCEPT, expected_after=ACCEPT)
                s = Script("checksig", wrap)
                add(name + "CHECKSIG with a matching output", s, proper(s, hashtype), ACCEPT, n_outputs=2, expected_after=ACCEPT)
                s = Script("multisig", wrap)
                add(name + "CHECKMULTISIG with a matching output", s, proper(s, hashtype), ACCEPT, n_outputs=2, expected_after=ACCEPT)

            name = wrap + ": "
            for hashtype in (SIGHASH_ALL, SIGHASH_NONE, SIGHASH_ALL | SIGHASH_ANYONECANPAY, SIGHASH_NONE | SIGHASH_ANYONECANPAY):
                s = Script("checksig", wrap)
                add(name + "CHECKSIG, hash type 0x%02x, no matching output" % hashtype, s, proper(s, hashtype), ACCEPT, expected_after=ACCEPT)
            s = Script("checksig_not", wrap)
            add(name + "CHECKSIG NOT, empty signature", s, empty(), ACCEPT, expected_after=ACCEPT)
            s = Script("multisig_not", wrap)
            add(name + "CHECKMULTISIG NOT, empty signature", s, empty(), ACCEPT, expected_after=ACCEPT)
            # A failing signature under OP_NOT: fine as long as it is one the regime of the block knows
            s = Script("checksig_not", wrap)
            add(name + "CHECKSIG NOT, garbage SIGHASH_ALL signature", s, garbage(SIGHASH_ALL), ACCEPT)
            s = Script("checksig_not", wrap)
            add(name + "CHECKSIG NOT, garbage SIGHASH_ALL|FORKID signature", s, garbage(SIGHASH_ALL | SIGHASH_FORKID), ACCEPT, expected_after=ACCEPT)
            s = Script("multisig_not", wrap)
            add(name + "CHECKMULTISIG NOT, garbage SIGHASH_SINGLE|FORKID signature", s, garbage(SIGHASH_SINGLE | SIGHASH_FORKID), ACCEPT, expected_after=ACCEPT)

        # SegWit v0 never had the constant digest: the signature commits to the transaction with an all-zero
        # outputs hash. The rule about SIGHASH_FORKID is the same.
        for hashtype in (SIGHASH_SINGLE, SINGLE_ACP):
            name = "p2wsh, hash type 0x%02x: " % hashtype
            s = Script("checksig", "p2wsh")
            add(name + "CHECKSIG, no matching output", s, proper(s, hashtype), ACCEPT, expected_after=ACCEPT)
            s = Script("multisig", "p2wsh")
            add(name + "CHECKMULTISIG, no matching output", s, proper(s, hashtype, key=1), ACCEPT, expected_after=ACCEPT)
            s = Script("checksig_not", "p2wsh")
            add(name + "CHECKSIG NOT, garbage signature", s, garbage(hashtype), ACCEPT, expected_after=ERR_FORKID_WITNESS)
            s = Script("checksig_not", "p2wsh")
            add(name + "CHECKSIG NOT, garbage signature with SIGHASH_FORKID", s, garbage(hashtype | SIGHASH_FORKID), ACCEPT, expected_after=ACCEPT)

        helpers = [NoSigOutput() for _ in cases]
        taproot = TestOutput("p2tr")
        taproot_helper = NoSigOutput()
        fund_outputs(node, [c[1] for c in cases] + helpers + [taproot, taproot_helper])
        height = node.getblockcount() + 1
        assert_equal(height >= S6B_HEIGHT, fork_active)

        confirmed = []
        for (description, script, sigs, n_outputs, expected), helper in zip(cases, helpers):
            self.log.debug("%s -> %s", description, expected or "accepted")
            tx = self.two_input_tx(helper, script, n_outputs)
            script.satisfy(tx, 1, sigs(tx))
            block = build_block(node, txs=[tx], fees=self.fee_of(tx, [helper, script]))
            if expected is ACCEPT:
                assert_block_accepted(node, block)
                confirmed.append((tx.hash, block.hash))
            else:
                assert_block_rejected(node, block, expected)
            assert node.getblockcount() + 1 < S6B_HEIGHT or fork_active, "ran into the transition height"

        # Taproot rejects SIGHASH_SINGLE without a matching output on both sides, by its own rule
        for hashtype in (SIGHASH_SINGLE, SINGLE_ACP):
            tx = self.two_input_tx(taproot_helper, taproot)
            # sign as SIGHASH_NONE of the same kind, then claim SIGHASH_SINGLE: there is no digest to sign
            taproot.sign(tx, 1, (hashtype & ~3) | SIGHASH_NONE, fork_active, spent_outputs=[CTxOut(taproot_helper.amount, taproot_helper.script_pubkey), taproot.txout])
            sig = tx.wit.vtxinwit[1].scriptWitness.stack[0]
            tx.wit.vtxinwit[1].scriptWitness.stack = [sig[:64] + bytes([hashtype])]
            tx.rehash()
            block = build_block(node, txs=[tx], fees=self.fee_of(tx, [taproot_helper, taproot]))
            assert_block_rejected(node, block, ERR_TAPROOT_HASHTYPE, exact=False)
        return confirmed

    def run_test(self):
        node = self.nodes[0]
        mine_to_height(node, 200, node.getnewaddress())

        self.log.info("Below the transition height: the historical behavior")
        mine_to_height(node, S6B_HEIGHT - 130)
        confirmed_before = self.run_cases(fork_active=False)
        assert node.getblockcount() < S6B_HEIGHT - 5

        self.log.info("The mempool and the signing RPC below the transition height")
        mine_to_height(node, S6B_HEIGHT - 4)
        in_block, in_mempool, late = Script("checksig"), Script("checksig"), Script("checksig")
        signer = Script("checksig")
        helpers = [NoSigOutput() for _ in range(4)]
        fund_outputs(node, [in_block, in_mempool, late, signer] + helpers)
        assert_equal(node.getblockcount(), S6B_HEIGHT - 3)

        def signed_over_constant(script, helper):
            tx = self.two_input_tx(helper, script)
            script.satisfy(tx, 1, [der_sig(script.keys[0], SIGHASH_SINGLE_BUG_DIGEST, SIGHASH_SINGLE)])
            return tx

        mempool_tx = signed_over_constant(in_mempool, helpers[1])
        assert_equal(mempool_accepts(node, mempool_tx), (True, None))

        def sign_single_with_rpc():
            unsigned = self.two_input_tx(helpers[3], signer)
            prevtxs = [{"txid": "%064x" % signer.outpoint.hash, "vout": signer.outpoint.n,
                        "scriptPubKey": signer.script_pubkey.hex(), "amount": signer.amount / COIN}]
            wif = bytes_to_wif(signer.keys[0].get_bytes())
            return node.signrawtransactionwithkey(unsigned.serialize().hex(), [wif], prevtxs, "SINGLE")
        # The signing RPC has never produced such a signature
        result = sign_single_with_rpc()
        assert_equal(result["complete"], False)
        assert_equal([e["vout"] for e in result["errors"]], [signer.outpoint.n])

        self.log.info("The boundary")
        mine_to_height(node, S6B_HEIGHT - 2)
        node.sendrawtransaction(mempool_tx.serialize().hex())
        # The last block below the transition height confirms one such spend; the one in the mempool is removed
        block_tx = signed_over_constant(in_block, helpers[0])
        last_block = build_block(node, txs=[block_tx], fees=self.fee_of(block_tx, [helpers[0], in_block]))
        with node.assert_debug_log(["signed for the other side of the height-%d transition" % S6B_HEIGHT]):
            assert_block_accepted(node, last_block)
        assert_equal(node.getblockcount(), S6B_HEIGHT - 1)
        assert_equal(node.getrawmempool(), [])
        assert_equal(mempool_accepts(node, mempool_tx), (False, OLD_STYLE))
        assert_raises_rpc_error(-26, OLD_STYLE, node.sendrawtransaction, mempool_tx.serialize().hex())
        # It cannot be mined any more, not even in a block that satisfies the transition block's coinbase rule
        fee = self.fee_of(mempool_tx, [helpers[1], in_mempool])
        assert_block_rejected(node, build_block(node, txs=[mempool_tx], fees=fee), ERR_FORKID)
        # ... nor can a signature that is garbage; the mempool names the script error for it
        late_tx = self.two_input_tx(helpers[2], late)
        late.satisfy(late_tx, 1, [self.garbage_sig(SIGHASH_SINGLE)])
        assert_equal(mempool_accepts(node, late_tx), (False, ERR_FORKID))
        result = sign_single_with_rpc()
        assert_equal(result["complete"], False)

        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), S6B_HEIGHT)

        self.log.info("Disconnecting the last historical block returns its spend to the mempool; reconnecting removes it")
        best = node.getbestblockhash()
        node.invalidateblock(last_block.hash)
        assert_equal(node.getblockcount(), S6B_HEIGHT - 2)
        assert_equal(node.getrawmempool(), [block_tx.hash])
        node.reconsiderblock(last_block.hash)
        assert_equal(node.getbestblockhash(), best)
        assert_equal(node.getrawmempool(), [])

        self.log.info("From the transition height on: out of reach")
        confirmed_after = self.run_cases(fork_active=True)

        self.log.info("Restart and -reindex keep the chain, with the historical spends below the transition height")
        best = node.getbestblockhash()
        tip = node.getblockcount()
        for extra in ([], ["-reindex"]):
            self.restart_node(0, extra_args=self.extra_args[0] + extra)
            self.wait_until(lambda: node.getblockcount() == tip, timeout=300)
            assert_equal(node.getbestblockhash(), best)
            for txid, block_hash in confirmed_before + [(block_tx.hash, last_block.hash)] + confirmed_after:
                block = node.getblock(block_hash)
                assert block["confirmations"] > 0
                assert txid in block["tx"]


if __name__ == '__main__':
    S6bSighashSingleTest().main()
