#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the peer-protocol-version floor.

Consensus::Params exposes a per-network peer-protocol-version floor schedule
(vMinPeerProtoVersionFloors: a sorted list of {height, min_version} pairs).
The floor in effect at the tip height is the min_version of the highest entry
whose height the chain has reached; peers advertising a lower version are
disconnected during the version handshake.

This release introduces no protocol bump, so every network carries a *flat*
schedule of a single {0, 70017} entry: the same floor from genesis to any
height, with no step. Height 600 used to carry a 70018 step on regtest, so this
test deliberately mines across it and asserts that a 70017 peer stays welcome
on both sides -- it fails loudly if a height-gated floor is reintroduced.

Note that MIN_PEER_PROTO_VERSION is also 70017, so the obsolete-version cutoff
and the floor coincide and a sub-70017 peer is rejected by the earlier of the
two checks. The floor's own rejection path is therefore unreachable while the
schedule stays flat at the hard minimum; that is expected, and the mechanism is
retained for a future release that raises one of them.
"""

from test_framework.messages import msg_version
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, p2p_port


FORMER_STEP_HEIGHT = 600  # where regtest used to raise the floor to 70018
FLAT_FLOOR = 70017        # the single schedule entry on every network
OBSOLETE_VERSION = 70016  # one below MIN_PEER_PROTO_VERSION (70017)


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

    def assert_flat_floor_peer_accepted(self, node, where):
        """A peer at the flat floor connects and stays connected."""
        node.add_p2p_connection(FixedVersionPeer(FLAT_FLOOR))
        peers = node.getpeerinfo()
        assert_equal(len(peers), 1)
        assert_equal(peers[0]["version"], FLAT_FLOOR)
        self.log.info("  accepted at %s (height %d)", where, node.getblockcount())
        node.disconnect_p2ps()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("A peer below the hard minimum is always rejected")
        self.connect_expect_reject(node, OBSOLETE_VERSION,
                                   "using obsolete version %d" % OBSOLETE_VERSION)

        self.log.info("A peer at the flat floor is accepted at genesis height")
        self.assert_flat_floor_peer_accepted(node, "genesis")

        addr = node.getnewaddress()

        self.log.info("Mine to one block below the former step height")
        node.generatetoaddress(FORMER_STEP_HEIGHT - 1, addr)
        assert_equal(node.getblockcount(), FORMER_STEP_HEIGHT - 1)
        self.assert_flat_floor_peer_accepted(node, "just below the former step")

        self.log.info("Mine past the former step height; the floor must not rise")
        # Mine a few blocks past it so the tip is unambiguously above the former
        # step when the next peer connects. Testing exactly at the boundary is
        # timing-sensitive: the version handshake reads the active chain height,
        # and a peer connecting the instant that block is connected could
        # otherwise race the tip update.
        node.generatetoaddress(5, addr)
        assert_equal(node.getblockcount(), FORMER_STEP_HEIGHT + 4)
        self.assert_flat_floor_peer_accepted(node, "above the former step")

        self.log.info("A sub-minimum peer is still rejected above the former step")
        self.connect_expect_reject(node, OBSOLETE_VERSION,
                                   "using obsolete version %d" % OBSOLETE_VERSION)


if __name__ == '__main__':
    MinPeerProtoFloorTest().main()
