#!/usr/bin/env bash
#
# test-asan-local.sh
#
# Run the CI ASan/UBSan build + tests LOCALLY in Docker, reproducing the
# GitHub Actions "asan+ubsan" leg so you can iterate without commit/push/wait.
#
# Mirrors ci/test/00_setup_env_native_asan.sh exactly: ubuntu:20.04, clang,
# system libraries (NO_DEPENDS), CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER',
# --with-sanitizers=address,integer,undefined, and the same ASAN/UBSAN/LSAN
# runtime options + in-tree suppression files. Green here => should be green
# on the asan+ubsan CI leg.
#
# Requires Docker. The build tree, objects and ccache live in Docker named
# volumes: first run is slow (~15-25 min), later runs are incremental/fast.
# Nothing is written into the repo or git.
#
# Usage:
#   contrib/test-asan-local.sh                         # full 'make check'
#   contrib/test-asan-local.sh --suite scriptpubkeyman_tests   # one suite, fast
#   contrib/test-asan-local.sh --jobs 8                # set parallelism
#   contrib/test-asan-local.sh --clean                 # wipe caches, rebuild

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE='rincoin-local:asan'
BUILD_VOL='rincoin_local_build_asan'
CCACHE_VOL='rincoin_local_ccache_asan'
SUITE=''
JOBS=''
DO_CLEAN=0

while [ $# -gt 0 ]; do
  case "$1" in
    --suite) SUITE="$2"; shift 2 ;;
    --jobs)  JOBS="-j$2"; shift 2 ;;
    --clean) DO_CLEAN=1; shift ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

info() { printf '\033[36m%s\033[0m\n' "$*"; }
fail() { printf '\033[31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail 'Docker not found. Install Docker and start it.'
docker info >/dev/null 2>&1 || fail 'Docker daemon not responding.'

if [ "$DO_CLEAN" -eq 1 ]; then
  info '== Removing cached build/ccache volumes =='
  docker volume rm "$BUILD_VOL" "$CCACHE_VOL" >/dev/null 2>&1 || true
fi

info '== Preparing ASan image (cached after first run) =='
docker build -t "$IMAGE" -f - "$REPO_ROOT" <<'DOCKERFILE'
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive TZ=UTC
RUN apt-get update && apt-get install -y \
      build-essential libtool autotools-dev automake pkg-config bsdmainutils \
      python3 python3-zmq ccache rsync git ca-certificates \
      clang llvm \
      qtbase5-dev qttools5-dev-tools libevent-dev \
      libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev \
      libdb5.3++-dev libminiupnpc-dev libzmq3-dev libqrencode-dev libsqlite3-dev \
      libssl-dev libfmt-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /build
DOCKERFILE

MODE='check'
[ -n "$SUITE" ] && MODE="suite:$SUITE"
info "== Building + testing (mode: $MODE) =="

docker run --rm \
    -e "JOBS_ARG=$JOBS" -e "MODE=$MODE" \
    -v "${REPO_ROOT}:/src:ro" \
    -v "${BUILD_VOL}:/build" \
    -v "${CCACHE_VOL}:/ccache" \
    "$IMAGE" bash -c '
set -euo pipefail
export CCACHE_DIR=/ccache
[ -n "${JOBS_ARG}" ] || JOBS_ARG="-j$(nproc)"

mkdir -p /build/rincoin
rsync -a --exclude=.git /src/ /build/rincoin/
cd /build/rincoin

# Exact CI sanitizer runtime options (ci/test/04_install.sh).
export ASAN_OPTIONS="detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1"
export LSAN_OPTIONS="suppressions=$PWD/test/sanitizer_suppressions/lsan"
export UBSAN_OPTIONS="suppressions=$PWD/test/sanitizer_suppressions/ubsan:print_stacktrace=1:halt_on_error=1:report_error_type=1"

ccache --zero-stats --max-size=2G >/dev/null 2>&1 || true

[ -x configure ] || ./autogen.sh
if [ ! -f config.status ]; then
  ./configure --disable-dependency-tracking \
      --enable-zmq --with-incompatible-bdb --without-gui \
      CPPFLAGS="-DARENA_DEBUG -DDEBUG_LOCKORDER" \
      --with-sanitizers=address,integer,undefined \
      --with-boost-process \
      CC="ccache clang" CXX="ccache clang++"
fi

make $JOBS_ARG

if [ "$MODE" = "check" ]; then
  make $JOBS_ARG check VERBOSE=1
else
  SUITE="${MODE#suite:}"
  BIN="$(find src/test -maxdepth 1 -type f -executable -name "test_*" | head -n1)"
  [ -n "$BIN" ] || { echo "unit test binary not found"; exit 1; }
  echo ">> Running suite \"$SUITE\" from $BIN"
  "$BIN" --catch_system_errors=no -l test_suite -t "$SUITE"
fi
ccache --show-stats | tail -n 5 || true
'

info ''
info '== PASSED (matches the asan+ubsan CI leg config) =='
info '  Iterate: edit code, re-run (incremental + ccache = fast).'
info '  Fast single suite:  contrib/test-asan-local.sh --suite <suite_name>'
