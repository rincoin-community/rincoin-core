// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdexcept>

#include <flatfile.h>
#include <logging.h>
#include <tinyformat.h>
#include <util/system.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

/* Buffer size for sequential reads (1 MB) */
static constexpr size_t SEQUENTIAL_READ_BUFFER_SIZE = 1024 * 1024;

FlatFileSeq::FlatFileSeq(fs::path dir, const char* prefix, size_t chunk_size) :
    m_dir(std::move(dir)),
    m_prefix(prefix),
    m_chunk_size(chunk_size)
{
    if (chunk_size == 0) {
        throw std::invalid_argument("chunk_size must be positive");
    }
}

std::string FlatFilePos::ToString() const
{
    return strprintf("FlatFilePos(nFile=%i, nPos=%i)", nFile, nPos);
}

fs::path FlatFileSeq::FileName(const FlatFilePos& pos) const
{
    return m_dir / strprintf("%s%05u.dat", m_prefix, pos.nFile);
}

FILE* FlatFileSeq::Open(const FlatFilePos& pos, bool read_only)
{
    if (pos.IsNull()) {
        return nullptr;
    }
    fs::path path = FileName(pos);
    fs::create_directories(path.parent_path());
    FILE* file = fsbridge::fopen(path, read_only ? "rb": "rb+");
    if (!file && !read_only)
        file = fsbridge::fopen(path, "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return nullptr;
    }
    if (pos.nPos && fseek(file, pos.nPos, SEEK_SET)) {
        LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
        fclose(file);
        return nullptr;
    }
    return file;
}

FILE* FlatFileSeq::OpenSequential(const FlatFilePos& pos)
{
    if (pos.IsNull()) {
        return nullptr;
    }
    fs::path path = FileName(pos);
    FILE* file = fsbridge::fopen(path, "rb");
    if (!file) {
        LogPrintf("Unable to open file %s for sequential read\n", path.string());
        return nullptr;
    }

    /* Set a larger buffer for better sequential read performance */
    static thread_local char* seq_buffer = nullptr;
    if (!seq_buffer) {
        seq_buffer = new char[SEQUENTIAL_READ_BUFFER_SIZE];
    }
    setvbuf(file, seq_buffer, _IOFBF, SEQUENTIAL_READ_BUFFER_SIZE);

#ifndef _WIN32
    /* Advise the kernel about sequential access pattern */
    int fd = fileno(file);
    if (fd >= 0) {
        /* Get file size for fadvise */
        struct stat st;
        if (fstat(fd, &st) == 0 && st.st_size > 0) {
            /* Tell kernel we'll read sequentially - enables aggressive read-ahead */
            posix_fadvise(fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
            /* Tell kernel we'll need this data soon - starts prefetching */
            posix_fadvise(fd, 0, st.st_size, POSIX_FADV_WILLNEED);
            LogPrint(BCLog::REINDEX, "Enabled sequential read optimization for %s (%ld MB)\n",
                     path.filename().string(), st.st_size / (1024 * 1024));
        }
    }
#endif

    if (pos.nPos && fseek(file, pos.nPos, SEEK_SET)) {
        LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
        fclose(file);
        return nullptr;
    }
    return file;
}

size_t FlatFileSeq::Allocate(const FlatFilePos& pos, size_t add_size, bool& out_of_space)
{
    out_of_space = false;

    unsigned int n_old_chunks = (pos.nPos + m_chunk_size - 1) / m_chunk_size;
    unsigned int n_new_chunks = (pos.nPos + add_size + m_chunk_size - 1) / m_chunk_size;
    if (n_new_chunks > n_old_chunks) {
        size_t old_size = pos.nPos;
        size_t new_size = n_new_chunks * m_chunk_size;
        size_t inc_size = new_size - old_size;

        if (CheckDiskSpace(m_dir, inc_size)) {
            FILE *file = Open(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in %s%05u.dat\n", new_size, m_prefix, pos.nFile);
                AllocateFileRange(file, pos.nPos, inc_size);
                fclose(file);
                return inc_size;
            }
        } else {
            out_of_space = true;
        }
    }
    return 0;
}

bool FlatFileSeq::Flush(const FlatFilePos& pos, bool finalize)
{
    FILE* file = Open(FlatFilePos(pos.nFile, 0)); // Avoid fseek to nPos
    if (!file) {
        return error("%s: failed to open file %d", __func__, pos.nFile);
    }
    if (finalize && !TruncateFile(file, pos.nPos)) {
        fclose(file);
        return error("%s: failed to truncate file %d", __func__, pos.nFile);
    }
    if (!FileCommit(file)) {
        fclose(file);
        return error("%s: failed to commit file %d", __func__, pos.nFile);
    }

    fclose(file);
    return true;
}
