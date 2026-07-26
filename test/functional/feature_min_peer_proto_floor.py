#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the peer-protocol-version floor.

Consensus::Params exposes a per-network peer-protocol-version floor
(nMinPeerProtoVersionFloorHeight / nMinPeerProtoVersionFloor). Before the
active chain reaches the floor height, the floor is dormant and any peer that
satisfies MIN_PEER_PROTO_VERSION (31800) is accepted. From the floor height
onward, peers advertising a version below the configured floor must be
disconnected during the version handshake.

On regtest the floor activates at height 600 with floor 70018.
"""

from test_framework.messages import msg_version
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


REGTEST_FLOOR_HEIGHT = 600
REGTEST_FLOOR = 70018
LOW_VERSION = 70017   # one below the floor
HIGH_VERSION = 70018  # at the floor
OBSOLETE_VERSION = 31799  # one below MIN_PEER_PROTO_VERSION (31800)


class FixedVersionPeer(P2PInterface):
    """P2P peer that advertises a specific nVersion in its version message."""

    def __init__(self, version):
        super().__init__()
        self._wanted_version = version

    def peer_connect_send_version(self, services):
        vt = msg_version()
        vt.nVersion = self._wanted_version
        vt.nServices = services
        vt.addrTo.ip = self.dstaddr
        vt.addrTo.port = self.dstport
        vt.addrFrom.ip = "0.0.0.0"
        vt.addrFrom.port = 0
        self.on_connection_send_msg = vt


class MinPeerProtoFloorTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Obsolete version (< MIN_PEER_PROTO_VERSION) is always rejected")
        # This rule is independent of the floor; check it while the floor is
        # still dormant (height 0). The node drops the connection during the
        # handshake, so do not wait for verack.
        peer_obsolete = node.add_p2p_connection(FixedVersionPeer(OBSOLETE_VERSION),
                                                wait_for_verack=False)
        peer_obsolete.wait_for_disconnect()

        self.log.info("Below floor height: a low-version peer is accepted")
        node.add_p2p_connection(FixedVersionPeer(LOW_VERSION))
        peers = node.getpeerinfo()
        assert_equal(len(peers), 1)
        assert_equal(peers[0]["version"], LOW_VERSION)
        node.disconnect_p2ps()

        self.log.info("Mine to one block before the floor height; still dormant")
        addr = node.getnewaddress()
        node.generatetoaddress(REGTEST_FLOOR_HEIGHT - 1, addr)
        assert_equal(node.getblockcount(), REGTEST_FLOOR_HEIGHT - 1)

        node.add_p2p_connection(FixedVersionPeer(LOW_VERSION))
        assert_equal(node.getpeerinfo()[0]["version"], LOW_VERSION)
        node.disconnect_p2ps()

        self.log.info("Mine past the floor block; floor now %d", REGTEST_FLOOR)
        # Mine a few blocks past the floor height so the tip is unambiguously
        # at/above the floor when the next peer connects. Testing exactly at the
        # boundary height is timing-sensitive: the version handshake reads the
        # active chain height, and a peer connecting the instant the floor block
        # is connected could otherwise race the tip update.
        node.generatetoaddress(5, addr)
        assert_equal(node.getblockcount(), REGTEST_FLOOR_HEIGHT + 4)

        self.log.info("At/above floor height: low-version peer is disconnected")
        # The node drops the connection during the handshake; disable the
        # verack wait so add_p2p_connection does not deadlock.
        peer_low = node.add_p2p_connection(FixedVersionPeer(LOW_VERSION),
                                           wait_for_verack=False)
        peer_low.wait_for_disconnect()

        self.log.info("At/above floor height: high-version peer is accepted")
        node.add_p2p_connection(FixedVersionPeer(HIGH_VERSION))
        peers = node.getpeerinfo()
        assert_equal(len(peers), 1)
        assert_equal(peers[0]["version"], HIGH_VERSION)


if __name__ == '__main__':
    MinPeerProtoFloorTest().main()
