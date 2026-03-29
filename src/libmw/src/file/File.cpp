#include <mw/file/File.h>
#include <mw/common/Logger.h>
#include <fs.h>
#include <util/system.h>

#include <cerrno>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// On Windows with MinGW (libstdc++), std::ifstream/ofstream/fstream only accept
// narrow char paths interpreted as the system ANSI code page, which mangles
// UTF-8 encoded paths containing non-ASCII characters (e.g. usernames with
// accented letters). We use _wfopen with the wide-string (UTF-16) path from
// ghc::filesystem::path::wstring() combined with __gnu_cxx::stdio_filebuf to
// properly open files at Unicode paths. This mirrors the approach used by
// fsbridge in src/fs.h for the rest of the codebase.
//
// On MSVC, std::fstream accepts wchar_t* directly, so we use that.
// On POSIX, narrow UTF-8 paths work natively.

#if defined(_WIN32) && defined(__GLIBCXX__)
#include <ext/stdio_filebuf.h>

// Convert std::ios_base::openmode to fopen mode string
static std::string openmode_to_str(std::ios_base::openmode mode)
{
    switch (mode & ~std::ios_base::ate) {
    case std::ios_base::in | std::ios_base::binary:
        return "rb";
    case std::ios_base::out | std::ios_base::binary | std::ios_base::trunc:
        return "wb";
    case std::ios_base::out | std::ios_base::binary | std::ios_base::app:
    case std::ios_base::out | std::ios_base::binary:
        return "ab";
    case std::ios_base::in | std::ios_base::out | std::ios_base::binary:
        return "r+b";
    case std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc:
        return "w+b";
    case std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::app:
        return "a+b";
    case std::ios_base::out | std::ios_base::trunc:
        return "w";
    case std::ios_base::out | std::ios_base::app:
    case std::ios_base::out:
        return "a";
    case std::ios_base::in:
        return "r";
    default:
        return std::string();
    }
}

// Wide-string-aware input stream for MinGW
class mw_ifstream : public std::istream
{
public:
    mw_ifstream() = default;
    mw_ifstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in)
    {
        open(p, mode);
    }
    ~mw_ifstream() { close(); }

    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in)
    {
        close();
        mode |= std::ios_base::in;
        std::wstring wmode = std::wstring(openmode_to_str(mode).begin(), openmode_to_str(mode).end());
        m_file = _wfopen(p.wstring().c_str(), wmode.c_str());
        if (m_file == nullptr) {
            return;
        }
        m_filebuf = __gnu_cxx::stdio_filebuf<char>(m_file, mode);
        rdbuf(&m_filebuf);
        if (mode & std::ios_base::ate) {
            seekg(0, std::ios_base::end);
        }
    }

    bool is_open() const { return m_file != nullptr; }

    void close()
    {
        if (m_file != nullptr) {
            m_filebuf.close();
            fclose(m_file);
        }
        m_file = nullptr;
    }

private:
    __gnu_cxx::stdio_filebuf<char> m_filebuf;
    FILE* m_file = nullptr;
};

// Wide-string-aware output stream for MinGW
class mw_ofstream : public std::ostream
{
public:
    mw_ofstream() = default;
    mw_ofstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::out)
    {
        open(p, mode);
    }
    ~mw_ofstream() { close(); }

    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::out)
    {
        close();
        mode |= std::ios_base::out;
        std::wstring wmode = std::wstring(openmode_to_str(mode).begin(), openmode_to_str(mode).end());
        m_file = _wfopen(p.wstring().c_str(), wmode.c_str());
        if (m_file == nullptr) {
            return;
        }
        m_filebuf = __gnu_cxx::stdio_filebuf<char>(m_file, mode);
        rdbuf(&m_filebuf);
        if (mode & std::ios_base::ate) {
            seekp(0, std::ios_base::end);
        }
    }

    bool is_open() const { return m_file != nullptr; }

    void close()
    {
        if (m_file != nullptr) {
            m_filebuf.close();
            fclose(m_file);
        }
        m_file = nullptr;
    }

private:
    __gnu_cxx::stdio_filebuf<char> m_filebuf;
    FILE* m_file = nullptr;
};

// Wide-string-aware bidirectional stream for MinGW
class mw_fstream : public std::iostream
{
public:
    mw_fstream() = default;
    mw_fstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        open(p, mode);
    }
    ~mw_fstream() { close(); }

    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    {
        close();
        std::wstring wmode = std::wstring(openmode_to_str(mode).begin(), openmode_to_str(mode).end());
        m_file = _wfopen(p.wstring().c_str(), wmode.c_str());
        if (m_file == nullptr) {
            return;
        }
        m_filebuf = __gnu_cxx::stdio_filebuf<char>(m_file, mode);
        rdbuf(&m_filebuf);
    }

    bool is_open() const { return m_file != nullptr; }

    void close()
    {
        if (m_file != nullptr) {
            m_filebuf.close();
            fclose(m_file);
        }
        m_file = nullptr;
    }

private:
    __gnu_cxx::stdio_filebuf<char> m_filebuf;
    FILE* m_file = nullptr;
};

#elif defined(_WIN32) && defined(_MSC_VER)
// MSVC: std::fstream accepts wchar_t* directly.
// Thin wrappers that convert ghc::filesystem::path to wstring for the standard streams.

class mw_ifstream : public std::ifstream
{
public:
    mw_ifstream() = default;
    mw_ifstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in)
        : std::ifstream(p.wstring().c_str(), mode) {}
    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in)
    { std::ifstream::open(p.wstring().c_str(), mode); }
};

class mw_ofstream : public std::ofstream
{
public:
    mw_ofstream() = default;
    mw_ofstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::out)
        : std::ofstream(p.wstring().c_str(), mode) {}
    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::out)
    { std::ofstream::open(p.wstring().c_str(), mode); }
};

class mw_fstream : public std::fstream
{
public:
    mw_fstream() = default;
    mw_fstream(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        : std::fstream(p.wstring().c_str(), mode) {}
    void open(const ghc::filesystem::path& p, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
    { std::fstream::open(p.wstring().c_str(), mode); }
};

#else
// POSIX: narrow UTF-8 paths work natively. Use standard streams directly.
using mw_ifstream = std::ifstream;
using mw_ofstream = std::ofstream;
using mw_fstream = std::fstream;
#endif

namespace
{
void CommitParentDirectory(const FilePath& path)
{
#if defined(_WIN32)
    HANDLE hDirectory = CreateFileW(
        path.ToBoost().wstring().c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    if (hDirectory == INVALID_HANDLE_VALUE) {
        ThrowFile_F("Failed to open directory for syncing: {}", path);
    }

    if (FlushFileBuffers(hDirectory) == 0) {
        const DWORD error = GetLastError();
        CloseHandle(hDirectory);
        if (error != ERROR_ACCESS_DENIED && error != ERROR_INVALID_FUNCTION) {
            ThrowFile_F("Failed to sync directory: {}", path);
        }
        return;
    }

    CloseHandle(hDirectory);
#else
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif

    const int fd = open(path.ToString().c_str(), flags);
    if (fd < 0) {
        ThrowFile_F("Failed to open directory for syncing: {}", path);
    }

    if (fsync(fd) != 0 && errno != EINVAL) {
        close(fd);
        ThrowFile_F("Failed to sync directory: {}", path);
    }

    close(fd);
#endif
}
}

void File::Create()
{
    m_path.GetParent().CreateDir();

    mw_ifstream inFile(m_path.m_path, std::ios::in | std::ifstream::ate | std::ifstream::binary);
    if (inFile.is_open()) {
        inFile.close();
    } else {
        LOG_INFO_F("File {} does not exist. Creating it now.", m_path);
        mw_ofstream outFile(m_path.m_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            ThrowFile_F("Failed to create file: {}", m_path);
        }
        outFile.close();
    }
}

bool File::Exists() const
{
    return m_path.Exists() && !m_path.IsDirectory();
}

void File::Truncate(const uint64_t size)
{
    bool success = false;

#if defined(WIN32)
    HANDLE hFile = CreateFileW(
        m_path.m_path.wstring().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    LARGE_INTEGER li;
    li.QuadPart = size;
    success = SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) && SetEndOfFile(hFile);

    CloseHandle(hFile);
#else
    success = (truncate(m_path.ToString().c_str(), size) == 0);
#endif

    if (!success) {
        ThrowFile_F("Failed to truncate {}", m_path);
    }
}

std::vector<uint8_t> File::ReadBytes() const
{
    std::error_code ec;
    if (!ghc::filesystem::exists(m_path.m_path, ec) || ec) {
        ThrowFile_F("{} not found", *this);
    }

    size_t size = (size_t)ghc::filesystem::file_size(m_path.m_path, ec);

    return ReadBytes(0, size);
}

std::vector<uint8_t> File::ReadBytes(const size_t startIndex, const size_t numBytes) const
{
    std::error_code ec;
    if (!ghc::filesystem::exists(m_path.m_path, ec) || ec) {
        ThrowFile_F("{} not found", *this);
    }

    mw_ifstream file(m_path.m_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        ThrowFile_F("Failed to open {} for reading", *this);
    }

    const size_t size = (size_t)ghc::filesystem::file_size(m_path.m_path, ec);
    if (size < (startIndex + numBytes)) {
        ThrowFile_F("Failed to read {} bytes from {}.", numBytes, *this);
    }

    std::vector<uint8_t> bytes(numBytes);
    file.seekg(startIndex, std::ios::beg);
    file.read((char*)bytes.data(), numBytes);
    file.close();

    return bytes;
}

void File::Write(const std::vector<uint8_t>& bytes)
{
    if (!Exists()) {
        Create();
    }

    mw_ofstream file(m_path.m_path, std::ios::out | std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        ThrowFile_F("Failed to write to file: {}", m_path);
    }

    file.write((const char*)bytes.data(), bytes.size());
    file.close();
}

void File::Write(const size_t startIndex, const std::vector<uint8_t>& bytes, const bool truncate)
{
    if (!Exists()) {
        Create();
    }
    
    if (!bytes.empty()) {
        mw_fstream file(m_path.m_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            ThrowFile_F("Failed to write to file: {}", m_path);
        }

        file.seekp(startIndex, std::ios::beg);
        file.write((const char*)bytes.data(), bytes.size());
        file.close();
    }

    if (truncate) {
        Truncate(startIndex + bytes.size());
    }
}

void File::WriteBytes(const std::unordered_map<uint64_t, uint8_t>& bytes)
{
    mw_fstream file(m_path.m_path, std::ios_base::binary | std::ios_base::out | std::ios_base::in);

    for (auto iter : bytes) {
        file.seekp(iter.first);
        file.write((const char*)&iter.second, 1);
    }

    file.close();
}

void File::Commit() const
{
    FILE* file = fsbridge::fopen(m_path.ToBoost(), "rb+");
    if (file == nullptr) {
        ThrowFile_F("Failed to open {} for syncing", *this);
    }

    if (!FileCommit(file)) {
        fclose(file);
        ThrowFile_F("Failed to sync {}", *this);
    }

    fclose(file);

    if (m_path.m_path.has_parent_path()) {
        CommitParentDirectory(m_path.GetParent());
    }
}

size_t File::GetSize() const
{
    if (!m_path.Exists()) {
        return 0;
    }

    std::error_code ec;
    const size_t size = (size_t)ghc::filesystem::file_size(m_path.m_path, ec);
    if (ec) {
        ThrowFile_F("Failed to determine size of {}", *this);
    }

    return size;
}

void File::CopyTo(const FilePath& new_path) const
{
    if (new_path.Exists()) {
        new_path.Remove();
    }

    std::error_code ec;
    ghc::filesystem::copy(m_path.m_path, new_path.m_path, ec);
    if (ec) {
        ThrowFile_F("Failed to copy {} to {}", m_path, new_path);
    }
}
