# Building Rincoin Release Binaries

This guide explains how to build official release binaries for Rincoin using the automated build script.

## Overview

The `contrib/build_release.sh` script automates the process of building release binaries for multiple platforms:
- **Linux (Ubuntu 20.04)** - Maximum compatibility: Ubuntu 18.04+, Debian 10+, RHEL 8+
- **Linux (Ubuntu 24.04)** - Modern performance: Ubuntu 24.04+, Debian 12+
- **Linux ARM64 (Ubuntu 20.04)** - Maximum compatibility for ARM64 systems
- **Linux ARM64 (Ubuntu 24.04)** - Modern/performance variant for ARM64 systems
- **Windows (x64)** - Windows 10 and Windows 11

The script uses Docker to create reproducible builds in isolated environments and supports build caching for faster subsequent builds.

## Build Variants

### Linux Ubuntu 20.04 (Maximum Compatibility)
- Built on Ubuntu 20.04 with glibc 2.31
- Compatible with Ubuntu 18.04+, Debian 10+, RHEL 8+
- Static linking of all dependencies except glibc
- Broadest compatibility for deployment on older systems
- **Package**: `rincoin-VERSION-x86_64-linux-gnu.tar.gz`

### Linux Ubuntu 24.04 (Modern Performance)  
- Built on Ubuntu 24.04 with glibc 2.39 and GCC 13
- Compatible with Ubuntu 24.04+, Debian 12+
- 5-15% performance improvement for compute-intensive operations
- Modern compiler optimizations and security features
- **Package**: `rincoin-VERSION-x86_64-linux-gnu-ubuntu24.tar.gz`

### Linux ARM64 Ubuntu 20.04 (Maximum Compatibility)
- Cross-compiled on Ubuntu 20.04 using `aarch64-linux-gnu`
- Compatible with modern ARM64 Linux distros using glibc 2.31+
- Includes full binary set: daemon, CLI, TX, wallet, and Qt GUI
- **Package**: `rincoin-VERSION-aarch64-linux-gnu.tar.gz`

### Linux ARM64 Ubuntu 24.04 (Modern Performance)
- Cross-compiled on Ubuntu 24.04 using `aarch64-linux-gnu`
- Optimized for modern ARM64 systems with newer runtime stack
- Includes full binary set: daemon, CLI, TX, wallet, and Qt GUI
- **Package**: `rincoin-VERSION-aarch64-linux-gnu-ubuntu24.tar.gz`

### Windows x64
- Cross-compiled on Ubuntu 24.04 using MinGW
- Compatible with Windows 10 and Windows 11
- Fully static binaries with no external DLL dependencies
- **Package**: `rincoin-VERSION-win64.zip`

## Choosing the Right Build

### For Linux Users

**Use Ubuntu 20.04 build if:**
- Deploying on Ubuntu 18.04, 20.04, 22.04, or older Debian/RHEL systems
- Maximum compatibility is required
- Running on VPS providers with older OS versions
- You're unsure which to choose (safest option)

**Use Ubuntu 24.04 build if:**
- Running Ubuntu 24.04+ or Debian 12+
- Want 5-15% better performance for Argon2 hashing
- Running on modern hardware with latest OS
- Need modern security features and compiler optimizations

**Not sure?** Use the Ubuntu 20.04 build - it works everywhere the Ubuntu 24.04 build does, plus older systems.

### For Windows Users

Use the provided `win64.zip` - compatible with both Windows 10 and Windows 11.

## Features

- ✅ Git-based builds from version tags
- ✅ Automatic source code packaging (tar.gz and zip)
- ✅ Multi-variant Linux builds (Ubuntu 20.04 and 24.04)
- ✅ ARM64 Linux builds (Ubuntu 20.04 and 24.04)
- ✅ Cross-platform compilation (Linux and Windows)
- ✅ Build artifact caching for faster rebuilds
- ✅ SHA256 checksum generation
- ✅ Comprehensive release documentation
- ✅ Static library linking for maximum portability

## Prerequisites

### System Requirements

- Linux host system (Debian 11+, Ubuntu 20.04+)
- At least 20GB free disk space
- 8GB+ RAM recommended
- Multi-core CPU (4+ cores recommended)

### Required Software

The build script requires the following tools:

- **Docker** - For isolated build environments
- **Git** - For repository management
- **tar** - For creating archives
- **gzip** - For compression
- **zip** - For Windows archives
- **Berkeley DB 4.8** - Must be pre-installed locally

### Installing Dependencies

#### Debian 13 (Trixie)

```bash
# Install Docker
sudo apt update
sudo apt install docker.io docker-compose
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
# Log out and back in for group changes to take effect

# Install build tools
sudo apt install git tar gzip zip

# Install Berkeley DB 4.8 (required for wallet support)
cd /path/to/rincoin
./contrib/install_db4.sh $(pwd)
```

#### Ubuntu 24.04 (Noble Numbat)

```bash
# Install Docker
sudo apt update
sudo apt install docker.io docker-compose-v2
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
# Log out and back in for group changes to take effect

# Install build tools
sudo apt install git tar gzip zip

# Install Berkeley DB 4.8 (required for wallet support)
cd /path/to/rincoin
./contrib/install_db4.sh $(pwd)
```

#### Verify Docker Installation

After logging out and back in, verify Docker works without sudo:

```bash
docker --version
docker ps
```

If you get permission errors, ensure you're in the docker group:

```bash
groups | grep docker
```

## Usage

### Basic Usage

Build release binaries from a git tag:

```bash
cd /path/to/rincoin
./contrib/build_release.sh <git-tag>
```

**Example:**
```bash
./contrib/build_release.sh v1.0.1
```

### Advanced Usage

#### With Custom Git Repository

```bash
./contrib/build_release.sh v1.0.1 https://github.com/YourFork/rincoin.git
```

#### Clean Build (Clear Dependency Caches)

Clear dependency download and build caches, but keep Docker images:

```bash
./contrib/build_release.sh v1.0.1 --clean
```

This will:
- Clear dependency download caches (~5-10 min savings on next build)
- Clear built dependency caches (~20-40 min savings on next build)
- **Keep Docker images** (preserves ~5-10 min of apt updates)

**Use when:** You want a fresh dependency build but don't want to rebuild Docker images.

#### Full Clean Build (Rebuild Everything)

Remove all cached artifacts including Docker images:

```bash
./contrib/build_release.sh v1.0.1 --clean-all
```

This will:
- Remove all Docker images (forces complete rebuild)
- Clear all dependency caches
- Rebuild from absolute scratch

**Use when:** You suspect Docker image corruption or want to test a completely fresh build.

#### Combined Options

```bash
./contrib/build_release.sh v1.0.2 https://github.com/Rin-coin/rincoin.git --clean
```

#### Platform Switches

By default, all platform groups are built. You can disable specific targets:

```bash
./contrib/build_release.sh v1.0.1 --no-linux-x86
./contrib/build_release.sh v1.0.1 --no-aarch64
./contrib/build_release.sh v1.0.1 --no-windows
```

These switches can be combined.

#### Building a Single Target

`--no-*` works in groups of two; `--only` selects exactly one target and can be
repeated. This is what lets CI run each target as its own parallel job instead
of building all five back to back:

```bash
./contrib/build_release.sh v1.0.1 --only windows
./contrib/build_release.sh v1.0.1 --only linux-aarch64-ubuntu24 --only windows
```

Valid targets: `linux-x86_64-ubuntu20`, `linux-x86_64-ubuntu24`,
`linux-aarch64-ubuntu20`, `linux-aarch64-ubuntu24`, `windows`.

Only the x86_64 Linux targets need Berkeley DB 4.8 on the host; the others get
theirs from `depends`, so `--only windows` runs without `install_db4.sh`.

#### Versioning a Local Build

`--local` builds the working tree and labels the output `local`. To build the
working tree but label it with a real version -- what CI does for a tag, so the
built source is exactly the commit that triggered the run rather than a re-clone
of the remote:

```bash
./contrib/build_release.sh --local --version v1.1.0
```

### Glibc Baselines

The two Linux images exist to give two glibc floors. A binary built against an
older glibc runs on newer ones, not the other way round:

| Build | glibc | Runs on |
|-------|-------|---------|
| `ubuntu20` | 2.31 | Ubuntu 20.04+, Debian 11+ (so also Ubuntu 22.04 / Debian 12) |
| `ubuntu24` | 2.39 | Ubuntu 24.04+, Debian 13+ |

The `ubuntu20` image is the compatibility build; keep it unless Ubuntu 20.04
base images stop being pullable, at which point 22.04 (glibc 2.35) is the next
floor and drops Ubuntu 20.04 and Debian 11 users.

### Clean Flags Quick Reference

| Flag | Clears Caches | Rebuilds Docker Images | Use Case |
|------|---------------|------------------------|----------|
| (none) | No | No | Fast iterative builds |
| `--clean` | Yes | **No** | Fresh dependency build, keep images |
| `--clean-all` | Yes | Yes | Complete rebuild from scratch |

### Platform Selection Flags

| Flag | Effect |
|------|--------|
| `--no-linux-x86` | Skip both x86_64 Linux builds |
| `--no-aarch64` | Skip both ARM64 Linux builds |
| `--no-windows` | Skip Windows build |
| `--only <target>` | Build only this target; repeatable, wins over `--no-*` |
| `--version <tag>` | Label a `--local` build with a real version |

## Build Process

The script performs the following steps:

### 1. Preparation
- Validates prerequisites (Docker, git, db4)
- Clones repository at specified tag
- Creates source packages (tar.gz and zip)

### 2. Linux Ubuntu 20.04 Build (Maximum Compatibility)
- Creates/reuses Docker image based on Ubuntu 20.04
- Builds dependencies via depends system (Boost, Qt5, etc.)
- Compiles binaries with static linking (except glibc 2.31)
- Generates: `rincoind`, `rincoin-cli`, `rincoin-tx`, `rincoin-wallet`, `rincoin-qt`
- Creates distribution tarball: `rincoin-VERSION-x86_64-linux-gnu.tar.gz`
- Compatible with: Ubuntu 18.04+, Debian 10+, RHEL 8+

### 3. Linux Ubuntu 24.04 Build (Modern Performance)
- Creates/reuses Docker image based on Ubuntu 24.04
- Builds dependencies via depends system with GCC 13
- Compiles binaries with modern optimizations
- Generates: `rincoind`, `rincoin-cli`, `rincoin-tx`, `rincoin-wallet`, `rincoin-qt`
- Creates distribution tarball: `rincoin-VERSION-x86_64-linux-gnu-ubuntu24.tar.gz`
- Compatible with: Ubuntu 24.04+, Debian 12+
- ~5-15% performance improvement over Ubuntu 20 build

### 4. Windows Build (MinGW cross-compilation)
- Creates/reuses Docker image based on Ubuntu 24.04 with MinGW toolchain
- Builds Windows dependencies via depends system
- Compiles Windows binaries with full static linking
- Generates: `*.exe` files
- Creates distribution zip: `rincoin-VERSION-win64.zip`
- Compatible with: Windows 10 and Windows 11

### 5. Linux ARM64 Builds
- Creates/reuses ARM64-capable Docker images for Ubuntu 20.04 and 24.04
- Builds dependencies via depends with `HOST=aarch64-linux-gnu`
- Compiles ARM64 binaries with full feature parity including Qt GUI
- Creates distribution tarballs:
  - `rincoin-VERSION-aarch64-linux-gnu.tar.gz`
  - `rincoin-VERSION-aarch64-linux-gnu-ubuntu24.tar.gz`

### 6. Finalization
- Generates SHA256 checksums for all artifacts
- Creates release documentation (README.txt)
- Organizes output in release-builds directory

## Checksum Structure

The release process uses two checksum layers:

1. **Inside each binary archive** (tar.gz/zip):
  - `SHA256SUMS.txt` is generated **before packaging**
  - Contains checksums for the binary files included in that archive
2. **Next to archives** (`tarballs/SHA256SUMS.txt`):
  - Generated after archives are built
  - Contains checksums of archive files themselves

## Build Caching

The script implements intelligent caching to speed up subsequent builds:

### Cached Artifacts

1. **Docker Images** (`~2-5 minutes saved`)
   - `rincoin-builder:linux-ubuntu20`
   - `rincoin-builder:linux-ubuntu24`
   - `rincoin-builder:windows`

2. **Dependency Downloads** (`~5-10 minutes saved`)
   - Location: `.build-cache/depends-sources/`
   - Cached: boost, qt, libevent, openssl, etc.

3. **Built Dependencies** (`~20-40 minutes saved`)
   - Location: `.build-cache/depends-built/`
   - Cached: Compiled dependencies for all platforms

### Build Times

| Build Type | First Build | Cached Build | Savings |
|------------|-------------|--------------|---------|
| Linux Ubuntu 20 only | ~25 min | ~10 min | ~60% |
| Linux Ubuntu 24 only | ~25 min | ~10 min | ~60% |
| Linux ARM64 Ubuntu 20 only | ~25 min | ~10 min | ~60% |
| Linux ARM64 Ubuntu 24 only | ~25 min | ~10 min | ~60% |
| Windows only | ~60 min | ~15 min | ~75% |
| All five platform variants | ~160 min | ~50 min | ~68% |

### Managing Cache

**View cache size:**
```bash
du -sh .build-cache/
```

**Clear dependency caches only (keeps Docker images):**
```bash
./contrib/build_release.sh v1.0.1 --clean
```

**Clear everything including Docker images:**
```bash
./contrib/build_release.sh v1.0.1 --clean-all
```

**Clear cache manually:**
```bash
rm -rf .build-cache/
```

## Output Structure

After a successful build, you'll find the following structure:

```
release-builds/
└── 1.0.1/
    ├── source/
    │   ├── rincoin-1.0.1.tar.gz              # Source code (tar.gz)
    │   ├── rincoin-1.0.1.zip                  # Source code (zip)
    │   └── SHA256SUMS.txt                      # Source checksums
    ├── binaries/
    │   ├── linux-ubuntu20/                     # Ubuntu 20.04 build (max compatibility)
    │   │   ├── rincoind                        # Linux daemon
    │   │   ├── rincoin-cli                     # Linux RPC client
    │   │   ├── rincoin-tx                      # Linux transaction tool
    │   │   ├── rincoin-wallet                  # Linux wallet tool
    │   │   ├── rincoin-qt                      # Linux GUI
    │   │   └── SHA256SUMS.txt                  # Binary checksums
    │   ├── linux-ubuntu24/                     # Ubuntu 24.04 build (modern performance)
    │   │   ├── rincoind                        # Linux daemon
    │   │   ├── rincoin-cli                     # Linux RPC client
    │   │   ├── rincoin-tx                      # Linux transaction tool
    │   │   ├── rincoin-wallet                  # Linux wallet tool
    │   │   ├── rincoin-qt                      # Linux GUI
    │   │   └── SHA256SUMS.txt                  # Binary checksums
    │   ├── linux-aarch64-ubuntu20/             # ARM64 Ubuntu 20.04 build
    │   │   ├── rincoind                        # ARM64 Linux daemon
    │   │   ├── rincoin-cli                     # ARM64 Linux RPC client
    │   │   ├── rincoin-tx                      # ARM64 Linux transaction tool
    │   │   ├── rincoin-wallet                  # ARM64 Linux wallet tool
    │   │   ├── rincoin-qt                      # ARM64 Linux GUI
    │   │   └── SHA256SUMS.txt                  # Binary checksums (also inside tarball)
    │   ├── linux-aarch64-ubuntu24/             # ARM64 Ubuntu 24.04 build
    │   │   ├── rincoind                        # ARM64 Linux daemon
    │   │   ├── rincoin-cli                     # ARM64 Linux RPC client
    │   │   ├── rincoin-tx                      # ARM64 Linux transaction tool
    │   │   ├── rincoin-wallet                  # ARM64 Linux wallet tool
    │   │   ├── rincoin-qt                      # ARM64 Linux GUI
    │   │   └── SHA256SUMS.txt                  # Binary checksums (also inside tarball)
    │   └── windows/
    │       ├── rincoind.exe                    # Windows daemon
    │       ├── rincoin-cli.exe                 # Windows RPC client
    │       ├── rincoin-tx.exe                  # Windows transaction tool
    │       ├── rincoin-wallet.exe              # Windows wallet tool
    │       ├── rincoin-qt.exe                  # Windows GUI
    │       └── SHA256SUMS.txt                  # Binary checksums
    ├── tarballs/
    │   ├── rincoin-1.0.1-x86_64-linux-gnu.tar.gz          # Linux Ubuntu 20.04 distro
    │   ├── rincoin-1.0.1-x86_64-linux-gnu-ubuntu24.tar.gz # Linux Ubuntu 24.04 distro
    │   ├── rincoin-1.0.1-aarch64-linux-gnu.tar.gz         # Linux ARM64 Ubuntu 20.04 distro
    │   ├── rincoin-1.0.1-aarch64-linux-gnu-ubuntu24.tar.gz# Linux ARM64 Ubuntu 24.04 distro
    │   ├── rincoin-1.0.1-win64.zip                         # Windows distribution
    │   └── SHA256SUMS.txt                                   # Archive checksums
    └── README.txt                                           # Release documentation
```

## Verification

### Verify Checksums

**For source packages:**
```bash
cd release-builds/1.0.1/source/
sha256sum -c SHA256SUMS.txt
```

**For distribution archives:**
```bash
cd release-builds/1.0.1/tarballs/
sha256sum -c SHA256SUMS.txt
```

**For individual binaries:**
```bash
cd release-builds/1.0.1/binaries/linux-ubuntu20/
sha256sum -c SHA256SUMS.txt
```

**For checksums embedded in an archive:**
```bash
tar -xzf release-builds/1.0.1/tarballs/rincoin-1.0.1-aarch64-linux-gnu.tar.gz
cd rincoin-1.0.1/bin
sha256sum -c SHA256SUMS.txt
```

### Test Binaries

**Linux:**
```bash
./release-builds/1.0.1/binaries/linux/rincoind --version
./release-builds/1.0.1/binaries/linux/rincoin-cli --help
```

**Windows (on Windows system):**
```cmd
release-builds\1.0.1\binaries\windows\rincoind.exe --version
```

## Distribution

### For Linux Users

Provide the tarball:
```
rincoin-1.0.1-x86_64-linux-gnu.tar.gz
```

Installation:
```bash
tar xzf rincoin-1.0.1-x86_64-linux-gnu.tar.gz
sudo cp rincoin-1.0.1/bin/* /usr/local/bin/
```

### For Windows Users

Provide the zip archive:
```
rincoin-1.0.1-win64.zip
```

Installation:
- Extract the zip file
- Run `rincoin-qt.exe` for GUI
- Or run `rincoind.exe` for daemon mode

### For Developers

Provide the source packages:
```
rincoin-1.0.1.tar.gz
rincoin-1.0.1.zip
```

Build from source:
```bash
tar xzf rincoin-1.0.1.tar.gz
cd rincoin-1.0.1
./autogen.sh
./configure
make -j$(nproc)
sudo make install
```

## Troubleshooting

### Docker Permission Denied

**Problem:** `permission denied while trying to connect to the Docker daemon socket`

**Solution:**
```bash
sudo usermod -aG docker $USER
# Log out and back in
```

### Berkeley DB 4.8 Not Found

**Problem:** `Berkeley DB 4.8 not found at /path/to/db4`

**Solution:**
```bash
cd /path/to/rincoin
./contrib/install_db4.sh $(pwd)
```

### Build Fails with "No space left on device"

**Problem:** Not enough disk space

**Solution:**
```bash
# Check disk space
df -h

# Clear Docker cache
docker system prune -a

# Clear build cache
rm -rf .build-cache/
```

### Windows Build Takes Too Long

**Problem:** Windows build is very slow on first run

**Solution:** This is normal. The depends system builds all dependencies from source (Qt, Boost, etc.). Subsequent builds will be much faster thanks to caching. Use `--clean` flag only when necessary.

### Git Tag Not Found

**Problem:** `Failed to clone repository or checkout tag`

**Solution:**
```bash
# List available tags
cd /path/to/rincoin
git tag

# Ensure tag exists
git tag | grep v1.0.1

# Try with exact tag name
./contrib/build_release.sh v1.0.1
```

## Advanced Topics

### Building for Additional Architectures

The script currently supports:

- Linux x86_64 (Ubuntu 20.04 + 24.04 build variants)
- Linux aarch64 (Ubuntu 20.04 + 24.04 build variants)
- Windows x64

Additional architectures can be added by extending target parameters in `contrib/build_release.sh`.

Example architectures from Bitcoin Core:
- `aarch64-linux-gnu` (64-bit ARM)
- `arm-linux-gnueabihf` (32-bit ARM)
- `riscv64-linux-gnu` (RISC-V 64-bit)

### Reproducible Builds

For deterministic/reproducible builds, consider using Gitian or Guix:

- **Gitian:** See `contrib/gitian-descriptors/`
- **Guix:** See `contrib/guix/` (if implemented)

These provide additional guarantees that the same source produces identical binaries.

### Continuous Integration

This repository includes `.github/workflows/release-build.yml` for automated release builds on tag pushes and manual workflow dispatch.

To trigger manually, use GitHub Actions workflow dispatch and provide a tag (for example `v1.0.1`).

High-level workflow steps:

```yaml
# Simplified release workflow flow
name: Build Release Binaries
on:
  push:
    tags:
      - 'v*'
jobs:
  build-release:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - run: ./contrib/build_release.sh ${{ github.ref_name }}
```

## Platform Compatibility

### Linux Binaries

Built on Ubuntu 20.04 (glibc 2.31), compatible with:

| Distribution | Minimum Version | Status |
|--------------|----------------|--------|
| Ubuntu | 20.04 LTS | ✅ Tested |
| Ubuntu | 22.04 LTS | ✅ Tested |
| Ubuntu | 24.04 LTS | ✅ Tested |
| Debian | 11 (Bullseye) | ✅ Compatible |
| Debian | 12 (Bookworm) | ✅ Compatible |
| Debian | 13 (Trixie) | ✅ Compatible |
| RHEL/Rocky/Alma | 8+ | ✅ Compatible |
| Fedora | 35+ | ✅ Compatible |
| Arch Linux | Current | ✅ Compatible |

**Not Supported:**
- CentOS 7 (glibc 2.17, EOL)
- Ubuntu 18.04 (glibc 2.27, EOL)
- Debian 10 (glibc 2.28, EOL)

### Windows Binaries

Built with MinGW, compatible with:

| Windows Version | Architecture | Status |
|-----------------|--------------|--------|
| Windows 10 | x64 | ✅ Supported |
| Windows 11 | x64 | ✅ Supported |
| Windows Server 2016+ | x64 | ✅ Supported |

**Not Supported:**
- Windows 7 (EOL)
- Windows 8/8.1 (EOL)
- 32-bit Windows

## Security Considerations

### Build Environment

- Builds run in isolated Docker containers
- No network access during compilation (only for dependency downloads)
- Uses official Ubuntu base images
- Source code cloned from git ensures authenticity

### Binary Verification

Always verify checksums:

```bash
# After download
sha256sum rincoin-1.0.1-x86_64-linux-gnu.tar.gz
# Compare with published SHA256SUMS.txt
```

### GPG Signing

For official releases, consider signing the binaries:

```bash
# Sign the checksums file
gpg --detach-sign --armor SHA256SUMS.txt

# Verify signature
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
```

## Additional Resources

- [Build Unix](build-unix.md) - Detailed Unix build instructions
- [Build Windows](build-windows.md) - Windows build documentation
- [Dependencies](dependencies.md) - Dependency version information
- [Gitian Building](gitian-building.md) - Deterministic build system
- [Release Process](release-process.md) - Official release procedures

## Support

For build-related issues:

- GitHub Issues: https://github.com/Rin-coin/rincoin/issues
- Discord: https://discord.gg/Ap7TUXYRBf
- Documentation: https://www.rincoin.net/

## License

This build script and documentation are released under the MIT License, same as Rincoin Core.
