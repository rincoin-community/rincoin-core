#!/bin/bash
# Build script for Rincoin release binaries.
# Supports Linux x86_64 (Ubuntu 20/24), Linux aarch64 (Ubuntu 20/24), and Windows x64.

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CLEAN_BUILD=false
CLEAN_IMAGES=false
LOCAL_BUILD=false
BUILD_LINUX_X86=true
BUILD_LINUX_AARCH64=true
BUILD_WINDOWS=true
VERSION_OVERRIDE=""

# Every target this script can produce. --only narrows the list to one or more
# of these, which is what lets CI run each target as its own parallel job
# instead of building all five back to back in a single one.
ALL_TARGETS="linux-x86_64-ubuntu20 linux-x86_64-ubuntu24 linux-aarch64-ubuntu20 linux-aarch64-ubuntu24 windows"
SELECTED_TARGETS=""

if [ -z "$1" ]; then
    echo -e "${RED}Error: Version tag or --local is required${NC}"
    echo "Usage: $0 <git-tag|--local> [git-url] [options]"
    echo ""
    echo "Options:"
    echo "  --clean                 rebuild from scratch (keeps docker images)"
    echo "  --clean-all             also drop the builder docker images"
    echo "  --version <tag>         label a --local build with this version"
    echo "                          (use in CI: build the checked-out tag, do not re-clone)"
    echo "  --only <target>         build just this target; repeatable"
    echo "  --no-linux-x86          skip both linux x86_64 targets"
    echo "  --no-aarch64            skip both linux aarch64 targets"
    echo "  --no-windows            skip the windows target"
    echo ""
    echo "Targets for --only:"
    for t in ${ALL_TARGETS}; do echo "  ${t}"; done
    echo ""
    echo "Example: $0 v1.0.1"
    echo "Example: $0 --local --clean-all"
    echo "Example: $0 --local --version v1.1.0 --only windows"
    exit 1
fi

if [ "$1" == "--local" ]; then
    LOCAL_BUILD=true
    GIT_TAG="local"
    shift
else
    GIT_TAG="$1"
    shift
fi

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --clean-all)
            CLEAN_BUILD=true
            CLEAN_IMAGES=true
            shift
            ;;
        --local)
            LOCAL_BUILD=true
            GIT_TAG="local"
            shift
            ;;
        --only)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: --only needs a target name${NC}" >&2
                exit 1
            fi
            case " ${ALL_TARGETS} " in
                *" $2 "*) SELECTED_TARGETS="${SELECTED_TARGETS} $2" ;;
                *)
                    echo -e "${RED}Error: unknown target '$2'${NC}" >&2
                    echo "Valid targets: ${ALL_TARGETS}" >&2
                    exit 1
                    ;;
            esac
            shift 2
            ;;
        --version)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: --version needs a value${NC}" >&2
                exit 1
            fi
            VERSION_OVERRIDE="$2"
            shift 2
            ;;
        --no-linux-x86)
            BUILD_LINUX_X86=false
            shift
            ;;
        --no-aarch64)
            BUILD_LINUX_AARCH64=false
            shift
            ;;
        --no-windows)
            BUILD_WINDOWS=false
            shift
            ;;
        *)
            if [ -z "$GIT_URL" ]; then
                GIT_URL="$1"
            fi
            shift
            ;;
    esac
done

GIT_URL="${GIT_URL:-https://github.com/rincoin-community/rincoin-core.git}"

# Resolve the target set. --only is explicit and wins outright; otherwise start
# from everything and subtract the --no-* groups.
if [ -n "$SELECTED_TARGETS" ]; then
    TARGETS="$(echo ${SELECTED_TARGETS})"
else
    TARGETS=""
    for t in ${ALL_TARGETS}; do
        case "$t" in
            linux-x86_64-*)  [ "$BUILD_LINUX_X86" = "true" ]     || continue ;;
            linux-aarch64-*) [ "$BUILD_LINUX_AARCH64" = "true" ] || continue ;;
            windows)         [ "$BUILD_WINDOWS" = "true" ]       || continue ;;
        esac
        TARGETS="${TARGETS} ${t}"
    done
    TARGETS="$(echo ${TARGETS})"
fi

target_enabled() {
    case " ${TARGETS} " in *" $1 "*) return 0 ;; *) return 1 ;; esac
}

if [ "$LOCAL_BUILD" = "true" ]; then
    # --version lets CI build the already-checked-out tag while still labelling
    # the artifacts with the real version. Without it a local build is "local".
    VERSION="${VERSION_OVERRIDE#v}"
    VERSION="${VERSION:-local}"
    [ -n "$VERSION_OVERRIDE" ] && GIT_TAG="$VERSION_OVERRIDE"
else
    VERSION="${GIT_TAG#v}"
fi

TEMP_DIR="/tmp/rincoin-build-$$"
SOURCE_DIR="${TEMP_DIR}/rincoin"
BUILD_DIR="${PROJECT_ROOT}/release-builds/${VERSION}"
BDB_PREFIX="${PROJECT_ROOT}/db4"
CACHE_DIR="${PROJECT_ROOT}/.build-cache"
DEPENDS_SOURCES_CACHE="${CACHE_DIR}/depends-sources"
DEPENDS_BUILT_CACHE="${CACHE_DIR}/depends-built"

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Rincoin Release Build Script${NC}"
echo -e "${GREEN}============================================${NC}"
if [ "$LOCAL_BUILD" = "true" ]; then
    echo -e "${BLUE}Build Mode:${NC} Local (current directory)"
else
    echo -e "${BLUE}Git Tag:${NC} ${GIT_TAG}"
    echo -e "${BLUE}Git URL:${NC} ${GIT_URL}"
fi
echo -e "${BLUE}Version:${NC} ${VERSION}"
echo -e "${BLUE}Clean Caches:${NC} ${CLEAN_BUILD}"
echo -e "${BLUE}Clean Images:${NC} ${CLEAN_IMAGES}"
echo -e "${BLUE}Targets:${NC} ${TARGETS}"
echo ""

cleanup() {
    local exit_code=$?
    if [ -d "$TEMP_DIR" ]; then
        if [ $exit_code -eq 0 ]; then
            print_info "Cleaning up temporary directory..."
            rm -rf "$TEMP_DIR"
        else
            print_error "Build failed. Temporary directory preserved for debugging: $TEMP_DIR"
        fi
    fi
}
trap cleanup EXIT

clean_cache() {
    if [ "$CLEAN_BUILD" = "true" ]; then
        print_info "Cleaning build caches..."
        rm -rf "$CACHE_DIR" 2>/dev/null || true
    fi

    if [ "$CLEAN_IMAGES" = "true" ]; then
        print_info "Cleaning Docker images..."
        docker rmi rincoin-builder:linux-x86_64-ubuntu20 2>/dev/null || true
        docker rmi rincoin-builder:linux-x86_64-ubuntu24 2>/dev/null || true
        docker rmi rincoin-builder:linux-aarch64-ubuntu20 2>/dev/null || true
        docker rmi rincoin-builder:linux-aarch64-ubuntu24 2>/dev/null || true
        docker rmi rincoin-builder:windows 2>/dev/null || true
    fi
}

check_prerequisites() {
    print_info "Checking prerequisites..."

    local required_commands=("docker" "git" "tar" "gzip" "zip")
    for cmd in "${required_commands[@]}"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            print_error "Required command '$cmd' not found"
            exit 1
        fi
    done

    if ! docker info >/dev/null 2>&1; then
        print_error "Docker is not running or permissions are missing"
        exit 1
    fi

    # Only the x86_64 linux targets mount the host Berkeley DB; aarch64 and
    # windows get theirs from depends, so do not demand it for those.
    if { target_enabled linux-x86_64-ubuntu20 || target_enabled linux-x86_64-ubuntu24; } && [ ! -d "$BDB_PREFIX" ]; then
        print_error "Berkeley DB 4.8 not found at $BDB_PREFIX"
        print_info "Please run: ./contrib/install_db4.sh \$(pwd)"
        exit 1
    fi

    if [ -z "$TARGETS" ]; then
        print_error "No build targets enabled."
        print_info "Valid targets: ${ALL_TARGETS}"
        exit 1
    fi
}

clone_and_checkout() {
    print_info "Preparing source..."
    rm -rf "$TEMP_DIR"
    mkdir -p "$TEMP_DIR"

    if [ "$LOCAL_BUILD" = "true" ]; then
        cd "$PROJECT_ROOT"
        mkdir -p "$SOURCE_DIR"
        if [ -d .git ] && command -v rsync >/dev/null 2>&1; then
            git ls-files -z | rsync -a --ignore-missing-args --files-from=- --from0 "${PROJECT_ROOT}/" "${SOURCE_DIR}/"
            git ls-files --others --exclude-standard -z | rsync -a --ignore-missing-args --files-from=- --from0 "${PROJECT_ROOT}/" "${SOURCE_DIR}/" 2>/dev/null || true
            cp -r "${PROJECT_ROOT}/.git" "${SOURCE_DIR}/.git" 2>/dev/null || true
        else
            cp -r "$PROJECT_ROOT" "$SOURCE_DIR"
            rm -rf "${SOURCE_DIR}/release-builds" "${SOURCE_DIR}/.build-cache" "${SOURCE_DIR}/depends/built" "${SOURCE_DIR}/depends/work"
        fi
    else
        git clone --depth 1 --branch "$GIT_TAG" "$GIT_URL" "$SOURCE_DIR"
    fi
}

setup_build_dirs() {
    mkdir -p "$BUILD_DIR" "$BUILD_DIR/source" "$BUILD_DIR/tarballs" "$DEPENDS_SOURCES_CACHE" "$DEPENDS_BUILT_CACHE"

    for t in ${TARGETS}; do
        case "$t" in
            linux-x86_64-ubuntu20)  mkdir -p "$BUILD_DIR/binaries/linux-ubuntu20" ;;
            linux-x86_64-ubuntu24)  mkdir -p "$BUILD_DIR/binaries/linux-ubuntu24" ;;
            linux-aarch64-ubuntu20) mkdir -p "$BUILD_DIR/binaries/linux-aarch64-ubuntu20" ;;
            linux-aarch64-ubuntu24) mkdir -p "$BUILD_DIR/binaries/linux-aarch64-ubuntu24" ;;
            windows)                mkdir -p "$BUILD_DIR/binaries/windows" ;;
        esac
    done
}

create_source_packages() {
    if [ "$LOCAL_BUILD" = "true" ]; then
        print_info "Skipping source package creation for local build"
        return
    fi

    cd "$TEMP_DIR"
    local source_name="rincoin-${VERSION}"
    cp -r "$SOURCE_DIR" "$source_name"
    rm -rf "${source_name}/.git"

    tar czf "${source_name}.tar.gz" "${source_name}/"
    zip -r -q "${source_name}.zip" "${source_name}/"

    mv "${source_name}.tar.gz" "$BUILD_DIR/source/"
    mv "${source_name}.zip" "$BUILD_DIR/source/"
    rm -rf "$source_name"
}

build_linux_binaries() {
    local ubuntu_version="$1"
    local ubuntu_label="$2"
    local host_triplet="$3"
    local arch_label="$4"
    local binary_dir="$5"
    local tarball_suffix="$6"
    local extra_packages="$7"
    local strip_cmd="$8"
    local use_host_db4="$9"

    print_info "Building Linux ${arch_label} on Ubuntu ${ubuntu_version}..."

    local dockerfile="${BUILD_DIR}/Dockerfile.linux-${arch_label}-${ubuntu_label}"
    local image_name="rincoin-builder:linux-${arch_label}-${ubuntu_label}"

    if ! docker image inspect "$image_name" >/dev/null 2>&1; then
        cat > "$dockerfile" << DOCKERFILE_END
FROM ubuntu:${ubuntu_version}
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
RUN apt-get update && apt-get install -y \
    build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 cmake \
    libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev \
    libfmt-dev libminiupnpc-dev libzmq3-dev libsqlite3-dev \
    libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libqrencode-dev \
    curl wget ca-certificates ${extra_packages} \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /build
DOCKERFILE_END
        docker build -t "$image_name" -f "$dockerfile" "$BUILD_DIR"
    fi

    local configure_cmd="CONFIG_SITE=\$PWD/depends/${host_triplet}/share/config.site ./configure --prefix=/ --disable-tests --disable-bench --enable-reduce-exports --with-incompatible-bdb=no"
    local db4_mount=""
    if [ "$use_host_db4" = "true" ]; then
        db4_mount="-v ${BDB_PREFIX}:/db4:ro"
        configure_cmd="CONFIG_SITE=\$PWD/depends/${host_triplet}/share/config.site ./configure BDB_LIBS=\"-L/db4/lib -ldb_cxx-4.8\" BDB_CFLAGS=\"-I/db4/include\" --prefix=/ --disable-tests --disable-bench --enable-reduce-exports --with-incompatible-bdb=no"
    fi

    docker run --rm \
        -v "${SOURCE_DIR}:/source:ro" \
        ${db4_mount} \
        -v "${BUILD_DIR}:/output" \
        -v "${DEPENDS_SOURCES_CACHE}:/depends_sources_cache" \
        -v "${DEPENDS_BUILT_CACHE}:/depends_built_cache" \
        -e "VERSION=${VERSION}" \
        "$image_name" \
        bash -c "
            set -e
            cp -r /source /build/rincoin
            cd /build/rincoin

            mkdir -p depends/sources depends/built
            cp -r /depends_sources_cache/* depends/sources/ 2>/dev/null || true
            cp -r /depends_built_cache/* depends/built/ 2>/dev/null || true

            cd depends
            make -j\$(nproc) HOST=${host_triplet}
            cp -r sources/* /depends_sources_cache/ 2>/dev/null || true
            cp -r built/* /depends_built_cache/ 2>/dev/null || true
            cd ..

            ./autogen.sh
            ${configure_cmd}
            make -j\$(nproc)

            ${strip_cmd} src/rincoind src/rincoin-cli src/rincoin-tx src/rincoin-wallet src/qt/rincoin-qt || true

            cp src/rincoind /output/binaries/${binary_dir}/
            cp src/rincoin-cli /output/binaries/${binary_dir}/
            cp src/rincoin-tx /output/binaries/${binary_dir}/
            cp src/rincoin-wallet /output/binaries/${binary_dir}/
            cp src/qt/rincoin-qt /output/binaries/${binary_dir}/

            rm -rf /tmp/rincoin-${VERSION}
            mkdir -p /tmp/rincoin-${VERSION}/bin
            cp src/rincoind src/rincoin-cli src/rincoin-tx src/rincoin-wallet src/qt/rincoin-qt /tmp/rincoin-${VERSION}/bin/
            cd /tmp/rincoin-${VERSION}/bin
            sha256sum rincoind rincoin-cli rincoin-tx rincoin-wallet rincoin-qt > SHA256SUMS.txt
            cp SHA256SUMS.txt /output/binaries/${binary_dir}/
            cd /tmp
            tar czf /output/tarballs/rincoin-${VERSION}-${tarball_suffix}.tar.gz rincoin-${VERSION}/
        "
}

build_windows_binaries() {
    print_info "Building Windows binaries..."

    local dockerfile="${BUILD_DIR}/Dockerfile.windows"
    local image_name="rincoin-builder:windows"

    if ! docker image inspect "$image_name" >/dev/null 2>&1; then
        cat > "$dockerfile" << 'DOCKERFILE_END'
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
RUN apt-get update && apt-get install -y \
    build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 curl git cmake \
    g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 mingw-w64-tools \
    nsis zip wget ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix && \
    update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
WORKDIR /build
DOCKERFILE_END
        docker build -t "$image_name" -f "$dockerfile" "$BUILD_DIR"
    fi

    docker run --rm \
        -v "${SOURCE_DIR}:/source:ro" \
        -v "${BUILD_DIR}:/output" \
        -v "${DEPENDS_SOURCES_CACHE}:/depends_sources_cache" \
        -v "${DEPENDS_BUILT_CACHE}:/depends_built_cache" \
        -e "VERSION=${VERSION}" \
        "$image_name" \
        bash -c '
            set -e
            cp -r /source /build/rincoin
            cd /build/rincoin

            mkdir -p depends/sources depends/built
            cp -r /depends_sources_cache/* depends/sources/ 2>/dev/null || true
            cp -r /depends_built_cache/* depends/built/ 2>/dev/null || true

            cd depends
            make -j$(nproc) HOST=x86_64-w64-mingw32
            cp -r sources/* /depends_sources_cache/ 2>/dev/null || true
            cp -r built/* /depends_built_cache/ 2>/dev/null || true
            cd ..

            ./autogen.sh
            CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure \
                --prefix=/ --disable-tests --disable-bench --enable-reduce-exports --disable-gui-tests
            make -j$(nproc)

            x86_64-w64-mingw32-strip --strip-all src/rincoind.exe src/rincoin-cli.exe src/rincoin-tx.exe src/rincoin-wallet.exe src/qt/rincoin-qt.exe || true

            cp src/rincoind.exe /output/binaries/windows/
            cp src/rincoin-cli.exe /output/binaries/windows/
            cp src/rincoin-tx.exe /output/binaries/windows/
            cp src/rincoin-wallet.exe /output/binaries/windows/
            cp src/qt/rincoin-qt.exe /output/binaries/windows/

            rm -rf /tmp/rincoin-'"${VERSION}"'
            mkdir -p /tmp/rincoin-'"${VERSION}"'
            cp src/rincoind.exe /tmp/rincoin-'"${VERSION}"'/
            cp src/rincoin-cli.exe /tmp/rincoin-'"${VERSION}"'/
            cp src/rincoin-tx.exe /tmp/rincoin-'"${VERSION}"'/
            cp src/rincoin-wallet.exe /tmp/rincoin-'"${VERSION}"'/
            cp src/qt/rincoin-qt.exe /tmp/rincoin-'"${VERSION}"'/
            cd /tmp/rincoin-'"${VERSION}"'
            sha256sum rincoind.exe rincoin-cli.exe rincoin-tx.exe rincoin-wallet.exe rincoin-qt.exe > SHA256SUMS.txt
            cp SHA256SUMS.txt /output/binaries/windows/
            cd /tmp
            zip -r /output/tarballs/rincoin-'"${VERSION}"'-win64.zip rincoin-'"${VERSION}"'/
        '
}

create_checksums() {
    print_info "Creating checksums..."
    cd "$BUILD_DIR"

    cd source
    rm -f SHA256SUMS.txt
    sha256sum * > SHA256SUMS.txt 2>/dev/null || true

    cd ../tarballs
    rm -f SHA256SUMS.txt
    sha256sum * > SHA256SUMS.txt 2>/dev/null || true
}

create_release_info() {
    print_info "Creating release documentation..."

    local git_commit
    local git_date
    git_commit=$(cd "$SOURCE_DIR" && git rev-parse HEAD 2>/dev/null || echo "N/A")
    git_date=$(cd "$SOURCE_DIR" && git log -1 --format=%cd --date=short 2>/dev/null || echo "N/A")

    cat > "${BUILD_DIR}/README.txt" << EOF
================================================================================
Rincoin ${VERSION} Release Binaries
================================================================================

Build Information:
------------------
Build Date:      $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Git Tag:         ${GIT_TAG}
Git Commit:      ${git_commit}
Git Date:        ${git_date}

Build Targets Built:
--------------------
${TARGETS}

Release Contents:
-----------------
source/
  - rincoin-${VERSION}.tar.gz
  - rincoin-${VERSION}.zip
  - SHA256SUMS.txt

tarballs/
  - rincoin-${VERSION}-x86_64-linux-gnu.tar.gz
  - rincoin-${VERSION}-x86_64-linux-gnu-ubuntu24.tar.gz
  - rincoin-${VERSION}-aarch64-linux-gnu.tar.gz
  - rincoin-${VERSION}-aarch64-linux-gnu-ubuntu24.tar.gz
  - rincoin-${VERSION}-win64.zip
  - SHA256SUMS.txt

Checksum Model:
---------------
1) Each binary archive includes an internal SHA256SUMS.txt for binaries.
2) tarballs/SHA256SUMS.txt contains hashes of the archive files.

EOF
}

main() {
    check_prerequisites
    clean_cache
    clone_and_checkout
    setup_build_dirs
    create_source_packages

    # One case per target keeps the dispatch immune to the `set -e` behaviour of
    # `guard && command` lists, and makes an unhandled target an error rather
    # than a silent no-op.
    for t in ${TARGETS}; do
        case "$t" in
            linux-x86_64-ubuntu20)
                build_linux_binaries "20.04" "ubuntu20" "x86_64-pc-linux-gnu" "x86_64" "linux-ubuntu20" "x86_64-linux-gnu" "" "strip" "true" ;;
            linux-x86_64-ubuntu24)
                build_linux_binaries "24.04" "ubuntu24" "x86_64-pc-linux-gnu" "x86_64" "linux-ubuntu24" "x86_64-linux-gnu-ubuntu24" "" "strip" "true" ;;
            linux-aarch64-ubuntu20)
                build_linux_binaries "20.04" "ubuntu20" "aarch64-linux-gnu" "aarch64" "linux-aarch64-ubuntu20" "aarch64-linux-gnu" "g++-aarch64-linux-gnu binutils-aarch64-linux-gnu" "aarch64-linux-gnu-strip" "false" ;;
            linux-aarch64-ubuntu24)
                build_linux_binaries "24.04" "ubuntu24" "aarch64-linux-gnu" "aarch64" "linux-aarch64-ubuntu24" "aarch64-linux-gnu-ubuntu24" "g++-aarch64-linux-gnu binutils-aarch64-linux-gnu" "aarch64-linux-gnu-strip" "false" ;;
            windows)
                build_windows_binaries ;;
            *)
                print_error "Unhandled target '$t'"
                exit 1 ;;
        esac
    done

    create_checksums
    create_release_info

    echo ""
    print_info "BUILD COMPLETE"
    print_info "Artifacts: ${BUILD_DIR}"
}

main