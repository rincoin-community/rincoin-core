// Copyright (c) 2026 The Rincoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Height-840,000 transition: every block index entry on disk records the client version
// of the software that wrote it, and loading the block index reports the entries from the
// transition height onward that software without the transition rules wrote
// (CChainState::RevalidateS6bBlocks() judges those blocks again).

#include <chain.h>
#include <chainparams.h>
#include <clientversion.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <txdb.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <map>
#include <memory>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(s6b_blockindex_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(entry_records_client_version_of_writer)
{
    // 1.0.5, 1.1.0, 1.2.0 and this release
    BOOST_CHECK_EQUAL(S6B_RULES_CLIENT_VERSION, 1020000);
    BOOST_CHECK(CLIENT_VERSION >= S6B_RULES_CLIENT_VERSION);

    CBlockIndex index;
    index.nHeight = 840000;
    index.nStatus = BLOCK_VALID_SCRIPTS;
    for (const int version : {1000500, 1010000, S6B_RULES_CLIENT_VERSION, CLIENT_VERSION}) {
        CDataStream stream(SER_DISK, version);
        stream << CDiskBlockIndex(&index);
        CDiskBlockIndex read;
        stream >> read;
        BOOST_CHECK_EQUAL(read.nClientVersion, version);
        BOOST_CHECK_EQUAL(read.nHeight, 840000);
        BOOST_CHECK_EQUAL(read.nStatus, uint32_t{BLOCK_VALID_SCRIPTS});
    }
}

BOOST_AUTO_TEST_CASE(load_reports_entries_written_without_s6b_rules)
{
    const Consensus::Params& consensus = Params().GetConsensus();
    const int h = consensus.nS6bHeight;
    CBlockTreeDB db(1 << 20, /* fMemory */ true, /* fWipe */ true);

    // Entries at h - 1 and h, written as 1.1.0 does; h + 1 by this release; h + 2 as 1.2.0 does.
    std::vector<uint256> hashes;
    hashes.reserve(4); // entries point into it
    std::vector<std::unique_ptr<CBlockIndex>> entries;
    for (int i = 0; i < 4; ++i) {
        hashes.push_back(InsecureRand256());
        auto entry = std::make_unique<CBlockIndex>();
        entry->phashBlock = &hashes.back();
        entry->nHeight = h - 1 + i;
        entry->nNonce = i;
        entry->nStatus = BLOCK_VALID_TREE;
        entry->pprev = entries.empty() ? nullptr : entries.back().get();
        entries.push_back(std::move(entry));
    }
    const auto write = [&](int client_version, std::vector<const CBlockIndex*> blocks) {
        db.SetEntryClientVersion(client_version);
        BOOST_CHECK(db.WriteBatchSync({}, 0, blocks));
    };
    write(1010000, {entries[0].get(), entries[1].get()});
    write(CLIENT_VERSION, {entries[2].get()});
    write(S6B_RULES_CLIENT_VERSION, {entries[3].get()});

    std::map<uint256, std::unique_ptr<CBlockIndex>> loaded;
    std::vector<CBlockIndex*> before_s6b_rules;
    BOOST_CHECK(db.LoadBlockIndexGuts(consensus, [&](const uint256& hash) -> CBlockIndex* {
        if (hash.IsNull()) return nullptr;
        auto& slot = loaded[hash];
        if (!slot) slot = std::make_unique<CBlockIndex>();
        return slot.get();
    }, before_s6b_rules));

    int with_height = 0;
    for (const auto& item : loaded) with_height += item.second->nHeight > 0;
    BOOST_CHECK_EQUAL(with_height, 4);
    BOOST_REQUIRE_EQUAL(before_s6b_rules.size(), 1U);
    BOOST_CHECK_EQUAL(before_s6b_rules[0]->nHeight, h);
}

BOOST_AUTO_TEST_SUITE_END()
