#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the voluntary coinbase tag and the absence of any mandatory block marker.

Rincoin Community Core offers the tag "/RCC/" in getblocktemplate's coinbaseaux.flags
and its own miner puts it into the coinbase scriptSig. The tag identifies the software
that assembled a block; it is not a consensus rule, neither before nor after the
height-840,000 transition (840 on regtest), and no other marker is required either.

Covered here, on both sides of the transition and for the transition block itself:
- getblocktemplate's coinbaseaux.flags;
- the node's own miner: BIP34 height first, extra nonce, tag, at most 100 bytes, with a
  witness commitment when the block has witness transactions;
- the coinbase constructions of pool software that copies the flags raw after the height,
  that wraps all coinbaseaux values in one push, or that ignores them;
- blocks with other markers -- the commitment output of earlier testing builds
  (OP_RETURN "RINF"...), once, twice or with a foreign payload, and a block version of
  0x52494e33 -- which are judged by the ordinary rules only;
- the ordinary coinbase rules, which the tag does not relax: the scriptSig length limit
  and the BIP34 height.
"""

from test_framework.blocktools import script_BIP34_coinbase_height
from test_framework.messages import CTxOut
from test_framework.s6b_util import (
    S6B_HEIGHT,
    assert_block_accepted,
    assert_block_rejected,
    build_block,
    mine_to_height,
)
from test_framework.script import OP_RETURN, CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

TAG = b"/RCC/"
FLAGS_HEX = "052f5243432f"  # the tag as a script push
WITNESS_COMMITMENT_HEADER = "6a24aa21a9ed"
GBT_RULES = {"rules": ["mweb", "segwit"]}


class S6bCoinbaseFlagsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # The second node only joins at the end. While the first node takes blocks off its chain again to
        # build several blocks at the transition height, it does not serve them to peers that ask.
        self.setup_nodes()

    def coinbase(self, node, block_hash):
        return node.getblock(block_hash, 2)["tx"][0]

    def check_own_miner(self, with_witness_tx):
        node = self.nodes[0]
        height = node.getblockcount() + 1
        if with_witness_tx:
            node.sendtoaddress(node.getnewaddress(address_type="bech32"), 1)
            txid = node.sendtoaddress(node.getnewaddress(address_type="bech32"), 0.5)
        block_hash = node.generatetoaddress(1, node.getnewaddress())[0]
        coinbase = self.coinbase(node, block_hash)
        script_sig = bytes.fromhex(coinbase["vin"][0]["coinbase"])
        height_push = bytes(script_BIP34_coinbase_height(height))
        assert script_sig.startswith(height_push), "BIP34 height must come first"
        assert script_sig.endswith(bytes.fromhex(FLAGS_HEX)), "tag must close the scriptSig"
        assert 2 <= len(script_sig) <= 100
        # height, a small extra nonce, and the tag: nothing else
        assert len(script_sig) <= len(height_push) + 5 + len(FLAGS_HEX) // 2
        commitments = [o for o in coinbase["vout"] if o["scriptPubKey"]["hex"].startswith(WITNESS_COMMITMENT_HEADER)]
        if with_witness_tx:
            assert txid in node.getblock(block_hash)["tx"]
            assert_equal(len(commitments), 1)
        return block_hash

    def check_pool_coinbases(self):
        """Coinbases as pool software builds them from a block template."""
        node = self.nodes[0]
        tmpl = node.getblocktemplate(GBT_RULES)
        assert_equal(tmpl["coinbaseaux"], {"flags": FLAGS_HEX})
        assert_equal(tmpl["height"], node.getblockcount() + 1)
        flags = bytes.fromhex(tmpl["coinbaseaux"]["flags"])
        extra_nonce = bytes(CScript([b"\x11\x22\x33\x44\x55\x66\x77\x88"]))
        aux_joined = b"".join(bytes.fromhex(v) for v in tmpl["coinbaseaux"].values())
        constructions = {
            "flags spliced in raw after the height": flags + extra_nonce,
            "all coinbaseaux values wrapped in one push": bytes(CScript([aux_joined])),
            "flags ignored": extra_nonce,
            "own pool tag only": bytes(CScript([b"/examplepool/"])) + extra_nonce,
        }
        for description, extra in constructions.items():
            self.log.debug("  %s", description)
            block = build_block(node, coinbase_script_sig_extra=extra)
            assert_block_accepted(node, block)
            script_sig = self.coinbase(node, block.hash)["vin"][0]["coinbase"]
            assert_equal(TAG.hex() in script_sig, description != "flags ignored" and "own pool" not in description)
            if node.getblockcount() == S6B_HEIGHT:
                # the other constructions get the same height too
                node.invalidateblock(block.hash)
        if node.getblockcount() == S6B_HEIGHT - 1:
            node.reconsiderblock(block.hash)

    def check_other_markers(self):
        node = self.nodes[0]
        payload = b"RINF" + bytes(range(36))
        marker = CTxOut(0, CScript([OP_RETURN, payload]))
        foreign = CTxOut(0, CScript([OP_RETURN, b"RINF" + b"\xff" * 4]))
        for description, outputs in {
            "one marker output": [marker],
            "two marker outputs": [marker, marker],
            "a marker with another payload": [foreign],
        }.items():
            self.log.debug("  %s", description)
            assert_block_accepted(node, build_block(node, extra_coinbase_outputs=outputs))
        self.log.debug("  block version 0x52494e33")
        block = build_block(node)
        block.nVersion = 0x52494e33
        block.solve()
        assert_block_accepted(node, block)
        assert_equal(node.getblock(block.hash)["versionHex"], "52494e33")

    def check_ordinary_rules(self):
        node = self.nodes[0]
        height = node.getblockcount() + 1
        height_push = bytes(script_BIP34_coinbase_height(height))
        # exactly 100 bytes is fine, 101 is not, tag or no tag
        # (a coinbase scriptSig is never executed, so any bytes will do as filler)
        filler = b"\x00" * (100 - len(height_push) - len(FLAGS_HEX) // 2)
        too_long = build_block(node, coinbase_script_sig_extra=filler + b"\x51" + bytes.fromhex(FLAGS_HEX))
        assert_block_rejected(node, too_long, "bad-cb-length")
        # a tag in place of the height does not satisfy BIP34
        block = build_block(node)
        block.vtx[0].vin[0].scriptSig = CScript(bytes.fromhex(FLAGS_HEX) + height_push)
        block.vtx[0].rehash()
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        assert_block_rejected(node, block, "bad-cb-height")
        assert_block_accepted(node, build_block(node, coinbase_script_sig_extra=filler + bytes.fromhex(FLAGS_HEX)))

    def run_test(self):
        node = self.nodes[0]
        mine_to_height(node, 200, node.getnewaddress())
        mine_to_height(node, S6B_HEIGHT - 20)

        for stage, target in (("Below the transition height", None), ("The transition block", S6B_HEIGHT - 1),
                              ("Above the transition height", None)):
            self.log.info(stage)
            if target is not None:
                mine_to_height(node, target)
                self.log.info("- the node's own miner")
                own = self.check_own_miner(with_witness_tx=True)
                assert_equal(node.getblock(own)["height"], S6B_HEIGHT)
                node.invalidateblock(own)
                self.log.info("- coinbases of pool software")
                self.check_pool_coinbases()
                assert_equal(node.getblockcount(), S6B_HEIGHT)
                continue
            self.log.info("- the node's own miner")
            self.check_own_miner(with_witness_tx=False)
            self.check_own_miner(with_witness_tx=True)
            self.log.info("- coinbases of pool software")
            self.check_pool_coinbases()
            self.log.info("- other markers")
            self.check_other_markers()
            self.log.info("- the ordinary coinbase rules")
            self.check_ordinary_rules()
            assert node.getblockcount() < S6B_HEIGHT - 1 or node.getblockcount() > S6B_HEIGHT

        self.log.info("A second node accepts the whole chain")
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(self.nodes[1].getbestblockhash(), node.getbestblockhash())


if __name__ == '__main__':
    S6bCoinbaseFlagsTest().main()
