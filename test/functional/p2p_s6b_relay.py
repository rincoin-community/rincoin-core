#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Relaying a transaction signed for the other side of the height-840,000 transition.

Around the transition height honest peers hold and relay transactions signed under the
regime they last saw. Rejecting such a transaction must not cost the peer anything: the
rejection is classified as a recent consensus change, so it carries no misbehaviour score
and the connection stays up. The node still refuses the transaction every time, which this
test checks twice, because the node forgets a rejected transaction when the tip moves and
remembers what a peer announced only for as long as that connection lasts.
"""

from test_framework.messages import msg_tx
from test_framework.p2p import P2PInterface
from test_framework.s6b_util import (
    S6B_HEIGHT,
    STANDARD_OUTPUT_SCRIPT,
    TestOutput,
    fund_outputs,
    mempool_accepts,
    mine_to_height,
)
from test_framework.script import SIGHASH_ALL
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

OLD_STYLE = "old-style-sig-fork-id"
FEE = 10000


class S6bRelayTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-acceptnonstdtxn=0", "-debug=mempoolrej", "-debug=net"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def announce(self, tx, step):
        """Hand the transaction to the node over P2P and read back how it judged it."""
        node = self.nodes[0]
        peer = node.add_p2p_connection(P2PInterface())
        peer_id = node.getpeerinfo()[-1]["id"]
        line = "%s from peer=%d was not accepted: %s" % (tx.hash, peer_id, OLD_STYLE)
        with node.assert_debug_log([line], unexpected_msgs=["Misbehaving"], timeout=30):
            peer.send_and_ping(msg_tx(tx))
        self.log.info("- %s: refused as %s, peer=%d not punished", step, OLD_STYLE, peer_id)
        assert_equal(node.getrawmempool(), [])
        assert_equal(len(node.getpeerinfo()), 1)  # still connected
        return peer

    def run_test(self):
        node = self.nodes[0]

        self.log.info("A chain above the transition height, and a transaction signed below it")
        node.generatetoaddress(110, node.getnewaddress())
        mine_to_height(node, S6B_HEIGHT + 4, node.getnewaddress())
        out = TestOutput("p2pkh")
        fund_outputs(node, [out])
        assert node.getblockcount() > S6B_HEIGHT

        # Signed as a node that has not seen the transition would sign it.
        stale = out.spend(SIGHASH_ALL, fork_active=False, fee=FEE, script_pubkey=STANDARD_OUTPUT_SCRIPT)
        stale.rehash()
        assert_equal(mempool_accepts(node, stale), (False, OLD_STYLE))

        peer = self.announce(stale, "first announcement")

        self.log.info("The same transaction again, after a new block and a new connection")
        # A new tip clears what the node remembers about rejected transactions, and a new
        # connection clears what it remembers about what this peer announced, so the second
        # announcement really reaches the mempool check again.
        node.generatetoaddress(1, node.getnewaddress())
        peer.peer_disconnect()
        peer.wait_for_disconnect()
        self.announce(stale, "second announcement")

        self.log.info("The properly signed spend of the same output is accepted")
        good = out.spend(SIGHASH_ALL, fork_active=True, fee=FEE, script_pubkey=STANDARD_OUTPUT_SCRIPT)
        good.rehash()
        assert_equal(mempool_accepts(node, good), (True, None))
        assert good.hash == node.sendrawtransaction(good.serialize().hex())
        assert_equal(node.getrawmempool(), [good.hash])


if __name__ == "__main__":
    S6bRelayTest().main()
