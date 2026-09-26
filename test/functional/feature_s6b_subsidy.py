#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the block subsidy schedule around and after the height-840,000 transition.

Regtest scales the mainnet schedule by 1/1000: the halving interval is 210, the
transition height is 840, and the later phases start at 2,100, 4,200 and 6,300.

Covered here:
- the halving schedule before the transition and the 4 / 2 / 1 / 0.6 RIN phases after it;
- the transition block itself, whose coinbase has to claim exactly subsidy plus fees:
  every fee level of interest (none, ordinary, exactly the 0.875 RIN by which the new
  subsidy exceeds the one of the previous schedule, just above it, high), each with an
  exact claim, one base unit less, one more, the claim of the previous schedule, and
  a flat 4 RIN;
- that under-claiming stays valid on both sides of the transition block;
- the node's own miner and getblocktemplate at the transition height;
- restart, -reindex-chainstate, -reindex, and initial block download by a new node.
"""

from decimal import Decimal

from test_framework.messages import COIN, COutPoint, CTransaction, CTxIn, CTxOut
from test_framework.s6b_util import (
    ANYONE_CAN_SPEND,
    S6B_HEIGHT,
    assert_block_accepted,
    assert_block_rejected,
    build_block,
    fund_outputs,
    mine_to_height,
    remine_with,
    subsidy,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

# Subsidy that the schedule in force before the transition would have paid at the transition height.
PREVIOUS_SCHEDULE_SUBSIDY = 312500000
NEW_SUBSIDY = 4 * COIN
SUBSIDY_DIFFERENCE = NEW_SUBSIDY - PREVIOUS_SCHEDULE_SUBSIDY  # 0.875 RIN

FEE_LEVELS = [
    ("no fees", 0),
    ("ordinary fees", 12345),
    ("fees of exactly 0.875 RIN", SUBSIDY_DIFFERENCE),
    ("fees just above 0.875 RIN", SUBSIDY_DIFFERENCE + 1),
    ("fees of 1.5 RIN", 150000000),
    ("fees of 25 RIN", 25 * COIN),
]


class S6bSubsidyTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # The second node stays isolated until the initial-block-download step. While the first node takes
        # blocks off its chain again to try several blocks at one height, it does not serve them to peers.
        self.setup_nodes()

    def coinbase_value(self, node, height):
        block = node.getblock(node.getblockhash(height), 2)
        return int(sum(out["value"] for out in block["tx"][0]["vout"]) * COIN)

    def fee_tx(self, outpoint, amount, fee):
        """Spend an anyone-can-spend output, leaving `fee` to the miner."""
        tx = CTransaction()
        tx.vin = [CTxIn(outpoint)]
        tx.vout = [CTxOut(amount - fee, ANYONE_CAN_SPEND)]
        tx.rehash()
        return tx

    def run_test(self):
        node = self.nodes[0]
        addr = node.getnewaddress()

        self.log.info("Halving schedule before the transition")
        mine_to_height(node, S6B_HEIGHT - 6, addr)
        for height, expected in [(1, 50 * COIN), (209, 50 * COIN), (210, 25 * COIN), (419, 25 * COIN),
                                 (420, 1250000000), (629, 1250000000), (630, 625000000), (S6B_HEIGHT - 6, 625000000)]:
            assert_equal(subsidy(height), expected)
            assert_equal(self.coinbase_value(node, height), expected)

        self.log.info("Fund one anyone-can-spend output per fee level")
        funded = []  # (COutPoint, amount)
        targets = [(ANYONE_CAN_SPEND, fee + COIN) for _, fee in FEE_LEVELS if fee > 0]
        funding = fund_outputs(node, targets)  # confirms at S6B_HEIGHT - 5
        for i, (_, amount) in enumerate(targets):
            funded.append((COutPoint(funding.sha256, i), amount))

        self.log.info("Under-claiming is valid below the transition height, over-claiming is not")
        mine_to_height(node, S6B_HEIGHT - 2, addr)
        assert_block_rejected(node, build_block(node, coinbase_value=subsidy(S6B_HEIGHT - 1) + 1), "bad-cb-amount")
        assert_block_accepted(node, build_block(node, coinbase_value=subsidy(S6B_HEIGHT - 1) - 1))
        assert_equal(node.getblockcount(), S6B_HEIGHT - 1)
        unclaimed = 1

        self.log.info("getblocktemplate for the transition block")
        tmpl = node.getblocktemplate({"rules": ["mweb", "segwit"]})
        assert_equal(tmpl["height"], S6B_HEIGHT)
        assert_equal(tmpl["coinbasevalue"], NEW_SUBSIDY)

        self.log.info("The transition block has to claim exactly subsidy plus fees")
        assert_equal(subsidy(S6B_HEIGHT), NEW_SUBSIDY)
        funded_iter = iter(funded)
        for label, fee in FEE_LEVELS:
            self.log.info("- %s", label)
            txs = []
            if fee > 0:
                outpoint, amount = next(funded_iter)
                txs = [self.fee_tx(outpoint, amount, fee)]
            exact = NEW_SUBSIDY + fee

            rejected = {
                "one base unit less": (exact - 1, "bad-cb-amount-transition"),
                "one base unit more": (exact + 1, "bad-cb-amount"),
                "claim of the previous schedule": (PREVIOUS_SCHEDULE_SUBSIDY + fee, "bad-cb-amount-transition"),
                "nothing": (0, "bad-cb-amount-transition"),
                "twice the amount": (2 * exact, "bad-cb-amount"),
            }
            if fee > 0:
                rejected["a flat 4 RIN"] = (NEW_SUBSIDY, "bad-cb-amount-transition")
                rejected["fees only"] = (fee, "bad-cb-amount-transition")
            tried = set()
            for what, (value, reason) in rejected.items():
                if value in tried:
                    # With fees of exactly 0.875 RIN the claim of the previous schedule is a flat 4 RIN:
                    # the same block, which the node would now report as a known invalid one.
                    continue
                tried.add(value)
                self.log.debug("  rejected: %s", what)
                assert_block_rejected(node, build_block(node, txs=txs, coinbase_value=value), reason)

            # The exact claim, also when it is split over several outputs
            split = build_block(node, txs=txs, coinbase_values=[exact - 3 * COIN, 2 * COIN, COIN])
            assert_block_accepted(node, split)
            assert_equal(self.coinbase_value(node, S6B_HEIGHT), exact)
            node.invalidateblock(split.hash)
            assert_equal(node.getblockcount(), S6B_HEIGHT - 1)

            block = build_block(node, txs=txs, coinbase_value=exact)
            assert_block_accepted(node, block)
            assert_equal(self.coinbase_value(node, S6B_HEIGHT), exact)
            stats = node.getblockstats(S6B_HEIGHT)
            assert_equal(stats["subsidy"], NEW_SUBSIDY)
            assert_equal(stats["totalfee"], fee)
            # Back to the block below the transition for the next fee level
            node.invalidateblock(block.hash)
            assert_equal(node.getblockcount(), S6B_HEIGHT - 1)

        self.log.info("The node's own miner claims exactly subsidy plus fees at the transition height")
        # The fee transactions of the disconnected blocks returned to the mempool; start with an empty one
        self.restart_node(0, extra_args=["-persistmempool=0"])
        assert_equal(node.getrawmempool(), [])
        txids = [node.sendtoaddress(node.getnewaddress(), 1) for _ in range(3)]
        fees = sum(int(-node.gettransaction(txid)["fee"] * COIN) for txid in txids)
        assert fees > 0
        tmpl = node.getblocktemplate({"rules": ["mweb", "segwit"]})
        assert_equal(tmpl["height"], S6B_HEIGHT)
        assert_equal(tmpl["coinbasevalue"], NEW_SUBSIDY + fees)
        transition_hash = node.generatetoaddress(1, addr)[0]
        assert_equal(node.getblockcount(), S6B_HEIGHT)
        assert_equal(self.coinbase_value(node, S6B_HEIGHT), NEW_SUBSIDY + fees)
        assert_equal(sorted(node.getblock(transition_hash)["tx"][1:]), sorted(txids))

        self.log.info("Under-claiming is valid again above the transition height, over-claiming is not")
        assert_block_rejected(node, build_block(node, coinbase_value=NEW_SUBSIDY + 1), "bad-cb-amount")
        assert_block_rejected(node, build_block(node, coinbase_value=subsidy(S6B_HEIGHT - 1)), "bad-cb-amount")
        assert_block_accepted(node, build_block(node, coinbase_value=NEW_SUBSIDY - 1000))
        unclaimed += 1000
        assert_equal(node.getblockcount(), S6B_HEIGHT + 1)

        self.log.info("Later phases: 2 RIN from 2,100, 1 RIN from 4,200 and 0.6 RIN from 6,300")
        assert_equal(self.coinbase_value(node, S6B_HEIGHT + 1), NEW_SUBSIDY - 1000)
        for start, previous, new in [(2100, 4 * COIN, 2 * COIN), (4200, 2 * COIN, COIN), (6300, COIN, 60000000)]:
            self.log.info("- phase starting at %d", start)
            # to an address outside the wallet, to keep the wallet small
            mine_to_height(node, start - 1)
            assert_equal(subsidy(start - 1), previous)
            assert_equal(subsidy(start), new)
            assert_equal(self.coinbase_value(node, start - 1), previous)
            assert_equal(node.getblocktemplate({"rules": ["mweb", "segwit"]})["coinbasevalue"], new)

            def claim(value):
                def mutate(block):
                    block.vtx[0].vout[0].nValue = value
                return mutate

            for value in (new + 1, previous):
                block, _ = remine_with(node, claim(value))
                assert_block_rejected(node, block, "bad-cb-amount")
                assert_equal(node.getblockcount(), start - 1)
            # below the maximum is fine
            block, _ = remine_with(node, claim(new - 1))
            assert_block_accepted(node, block)
            unclaimed += 1
            assert_equal(node.getblockcount(), start)
            assert_equal(self.coinbase_value(node, start), new - 1)
            node.generatetoaddress(1, addr)
            assert_equal(self.coinbase_value(node, start + 1), new)
        mine_to_height(node, 6310)

        self.log.info("The coin supply matches the schedule")
        tip = node.getblockcount()
        expected_supply = sum(subsidy(h) for h in range(1, tip + 1)) - unclaimed
        assert_equal(node.gettxoutsetinfo()["total_amount"], Decimal(expected_supply) / COIN)

        best = node.getbestblockhash()

        self.log.info("Restart, -reindex-chainstate and -reindex arrive at the same chain")
        for extra in ([], ["-reindex-chainstate"], ["-reindex"]):
            self.restart_node(0, extra_args=extra)
            self.wait_until(lambda: node.getblockcount() == tip, timeout=600)
            assert_equal(node.getbestblockhash(), best)
            assert_equal(node.gettxoutsetinfo()["total_amount"], Decimal(expected_supply) / COIN)

        self.log.info("A new node downloads and validates the whole chain")
        assert_equal(self.nodes[1].getblockcount(), 0)
        self.connect_nodes(0, 1)
        self.sync_blocks(timeout=600)
        assert_equal(self.nodes[1].getbestblockhash(), best)
        assert_equal(self.coinbase_value(self.nodes[1], S6B_HEIGHT), NEW_SUBSIDY + fees)
        assert_equal(self.nodes[1].gettxoutsetinfo()["total_amount"], Decimal(expected_supply) / COIN)


if __name__ == '__main__':
    S6bSubsidyTest().main()
