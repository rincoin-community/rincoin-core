#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the peer-protocol-version floor.

Consensus::Params exposes a per-network peer-protocol-version floor schedule
(vMinPeerProtoVersionFloors: a sorted list of {height, min_version} pairs).
The floor in effect at the tip height is the min_version of the highest entry
whose height the chain has reached; peers advertising a lower version are
disconnected during the version handshake (this is independent of the older
MIN_PEER_PROTO_VERSION = 31800 obsolete-version cutoff).

On regtest the schedule is {{0, 70017}, {600, 70018}}: 70017 is required from
genesis and the floor rises to 70018 at height 600. LOW_VERSION (70017) is
therefore accepted below height 600 but rejected at/after it.
"""

from test_framework.messages import msg_version
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, p2p_port


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

    def connect_expect_reject(self, node, version, expect_log):
        """Connect a fixed-version peer the node rejects during the version
        handshake, confirming the rejection via the node's durable debug-log
        line instead of the mininode's transient ``is_connected`` flag.

        A peer dropped mid-handshake is ``is_connected == True`` for only a few
        tens of milliseconds. ``add_p2p_connection`` polls for that transient on
        the main thread, which under load can be descheduled past the whole
        window and then time out on a flag that will never be True again. The
        debug-log line is written once and never disappears, so gating on it is
        race-free regardless of scheduler timing.
        """
        peer = FixedVersionPeer(version)
        with node.assert_debug_log([expect_log], timeout=30):
            peer.peer_connect(dstaddr='127.0.0.1', dstport=p2p_port(node.index),
                              net=node.chain,
                              timeout_factor=node.timeout_factor)()
            node.p2ps.append(peer)
        # is_connected may already be False (we never needed to observe True);
        # this is a harmless best-effort wait for the socket teardown.
        peer.wait_for_disconnect()
        return peer

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Obsolete version (< MIN_PEER_PROTO_VERSION) is always rejected")
        # This rule is independent of the floor; check it while the floor is
        # still dormant (height 0).
        self.connect_expect_reject(node, OBSOLETE_VERSION,
                                   "using obsolete version %d" % OBSOLETE_VERSION)

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
        self.connect_expect_reject(node, LOW_VERSION,
                                   "below RinHash floor %d" % REGTEST_FLOOR)

        self.log.info("At/above floor height: high-version peer is accepted")
        node.add_p2p_connection(FixedVersionPeer(HIGH_VERSION))
        peers = node.getpeerinfo()
        assert_equal(len(peers), 1)
        assert_equal(peers[0]["version"], HIGH_VERSION)


if __name__ == '__main__':
    MinPeerProtoFloorTest().main()
