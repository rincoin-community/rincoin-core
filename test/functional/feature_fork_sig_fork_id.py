#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test sig_fork_id sighash-level replay protection (Scenario 1: one node).

From FORK_H1_HEIGHT onward, every non-coinbase input's legacy and BIP143
(SegWit v0) sighash preimage must include the 8-byte sig_fork_id
(rincoin-consensus840k/technology/consensus-transition.md §6). A signature
computed without it ("old-style") is rejected once the confirming height
reaches H1; a signature computed with it ("new-style") is required from H1
onward and is rejected before H1 (since sig_fork_id is inactive pre-fork,
the preimages simply don't match).

Rejection of an old-style-signed post-H1 transaction must use the
non-punitive TX_RECENT_CONSENSUS_CHANGE classification (already defined in
src/consensus/validation.h, unused before this branch), not the punitive
TX_CONSENSUS bucket -- checked via the node's debug log.
"""

from test_framework.fork_scenario import FORK_H1_HEIGHT, FORK_H1_EXTRA_ARG, sig_fork_id
from test_framework.fork_sighash import LegacySignatureHashForkId, SegwitV0SignatureHashForkId
from test_framework.fork_util import mine_to_height
from test_framework.key import ECKey
from test_framework.messages import CTransaction, CTxIn, CTxInWitness, CTxOut, COIN
from test_framework.rin_util import make_utxo
from test_framework.script import SIGHASH_ALL, CScript
from test_framework.script_util import DUMMY_P2WPKH_SCRIPT, key_to_p2pkh_script, key_to_p2wpkh_script
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

FEE = 1000


def make_key():
    key = ECKey()
    key.generate()
    return key, key.get_pubkey().get_bytes()


def der_sig(key, sighash, hashtype=SIGHASH_ALL):
    return key.sign_ecdsa(sighash, low_s=True) + bytes([hashtype])


class ForkSigForkIdTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[FORK_H1_EXTRA_ARG]]

    def spend_p2pkh(self, node, key, pubkey, utxo, amount, *, forkid):
        script_pubkey = key_to_p2pkh_script(pubkey)
        tx = CTransaction()
        tx.vin.append(CTxIn(utxo, b""))
        tx.vout.append(CTxOut(amount - FEE, DUMMY_P2WPKH_SCRIPT))
        sighash = LegacySignatureHashForkId(script_pubkey, tx, 0, SIGHASH_ALL, forkid)
        tx.vin[0].scriptSig = CScript([der_sig(key, sighash), pubkey])
        tx.rehash()
        return tx

    def spend_p2wpkh(self, node, key, pubkey, utxo, amount, *, forkid):
        script_code = key_to_p2pkh_script(pubkey)  # BIP143 scriptCode for P2WPKH
        tx = CTransaction()
        tx.vin.append(CTxIn(utxo, b""))
        tx.vout.append(CTxOut(amount - FEE, DUMMY_P2WPKH_SCRIPT))
        sighash = SegwitV0SignatureHashForkId(script_code, tx, 0, SIGHASH_ALL, amount, forkid)
        tx.wit.vtxinwit = [CTxInWitness()]
        tx.wit.vtxinwit[0].scriptWitness.stack = [der_sig(key, sighash), pubkey]
        tx.rehash()
        return tx

    def run_test(self):
        node = self.nodes[0]
        amount = 10 * COIN

        self.log.info(f"Pre-fork (height < {FORK_H1_HEIGHT}): ordinarily-signed (forkid-inactive) spends are accepted")
        mine_to_height(node, FORK_H1_HEIGHT - 10)
        key, pubkey = make_key()
        utxo = make_utxo(node, amount, scriptPubKey=key_to_p2pkh_script(pubkey))
        tx = self.spend_p2pkh(node, key, pubkey, utxo, amount, forkid=None)
        txid = node.sendrawtransaction(tx.serialize().hex())
        assert_equal(txid, tx.hash)
        node.generate(1)  # confirm pre-fork, clears mempool for the next case

        self.log.info(f"Reaching H1 ({FORK_H1_HEIGHT}) via the node's own miner "
                       "(exercises B.2/B.3 as a side effect)")
        mine_to_height(node, FORK_H1_HEIGHT)

        forkid = sig_fork_id()
        assert_equal(len(forkid), 8)

        self.log.info("Post-H1: new-style (forkid-included) legacy P2PKH spend is accepted")
        key1, pubkey1 = make_key()
        utxo1 = make_utxo(node, amount, scriptPubKey=key_to_p2pkh_script(pubkey1))
        tx1 = self.spend_p2pkh(node, key1, pubkey1, utxo1, amount, forkid=forkid)
        txid1 = node.sendrawtransaction(tx1.serialize().hex())
        assert_equal(txid1, tx1.hash)

        self.log.info("Post-H1: old-style (forkid-omitted) legacy P2PKH spend is rejected, "
                       "classified as a recent-consensus-change (non-punitive) reject")
        key2, pubkey2 = make_key()
        utxo2 = make_utxo(node, amount, scriptPubKey=key_to_p2pkh_script(pubkey2))
        tx2 = self.spend_p2pkh(node, key2, pubkey2, utxo2, amount, forkid=None)
        # "old-style-sig-fork-id" is the machine-readable reject reason set
        # alongside TxValidationResult::TX_RECENT_CONSENSUS_CHANGE
        # (validation.cpp, PolicyScriptChecks) -- the classification itself
        # is internal state, not something separately logged, so this is
        # checked via the RPC error's reason string rather than debug.log.
        assert_raises_rpc_error(-26, "old-style-sig-fork-id", node.sendrawtransaction, tx2.serialize().hex())

        self.log.info("Post-H1: new-style (forkid-included) SegWit v0 P2WPKH spend is accepted")
        key3, pubkey3 = make_key()
        utxo3 = make_utxo(node, amount, scriptPubKey=key_to_p2wpkh_script(pubkey3))
        tx3 = self.spend_p2wpkh(node, key3, pubkey3, utxo3, amount, forkid=forkid)
        txid3 = node.sendrawtransaction(tx3.serialize().hex())
        assert_equal(txid3, tx3.hash)

        self.log.info("Post-H1: old-style (forkid-omitted) SegWit v0 P2WPKH spend is rejected")
        key4, pubkey4 = make_key()
        utxo4 = make_utxo(node, amount, scriptPubKey=key_to_p2wpkh_script(pubkey4))
        tx4 = self.spend_p2wpkh(node, key4, pubkey4, utxo4, amount, forkid=None)
        assert_raises_rpc_error(-26, None, node.sendrawtransaction, tx4.serialize().hex())

        self.log.info("Rejected old-style txs never entered the mempool, so block assembly is unaffected")
        mempool = node.getrawmempool()
        assert tx2.hash not in mempool
        assert tx4.hash not in mempool

        # make_utxo() confirms its own funding tx as it goes, so txid1/txid3
        # may already be confirmed by now (in an earlier block than this
        # final one) -- check they've cleared the mempool (accepted and
        # since confirmed, never invalidated) rather than assuming they
        # land in one specific block. Both txs pay to an anyone-can-spend
        # dummy script the wallet doesn't own, so gettransaction()
        # (wallet-only) can't be used here.
        node.generate(1)
        mempool = node.getrawmempool()
        assert txid1 not in mempool
        assert txid3 not in mempool


if __name__ == '__main__':
    ForkSigForkIdTest().main()
