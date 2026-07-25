<#
.SYNOPSIS
    Run the CI ASan/UBSan build + tests LOCALLY in Docker, reproducing the
    GitHub Actions "asan+ubsan" leg so you can iterate without commit/push/wait.

.DESCRIPTION
    Mirrors ci/test/00_setup_env_native_asan.sh exactly: ubuntu:20.04, clang,
    system libraries (NO_DEPENDS), CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER',
    --with-sanitizers=address,integer,undefined, and the same ASAN/UBSAN/LSAN
    runtime options + in-tree suppression files. If it passes here it should
    pass the asan+ubsan CI leg.

    Requires only Docker Desktop. The build tree, object files and ccache live
    in Docker named volumes, so the first run is slow (~15-25 min) and later
    runs are incremental and fast. Nothing is written into the repo or git.

.PARAMETER Suite
    Run only one Boost unit-test suite from the main test binary (fast path),
    e.g. -Suite scriptpubkeyman_tests. Builds incrementally, then runs just
    that suite. Omit to run the full 'make check'.

.PARAMETER Check
    Force a full 'make check' (all unit tests, incl. libmw) even if -Suite is
    given. This is the default when -Suite is not specified.

.PARAMETER Jobs
    Parallel build/test jobs. Defaults to the container's CPU count.

.PARAMETER Clean
    Remove the cached build + ccache volumes, then rebuild from scratch.

.EXAMPLE
    contrib\test-asan-local.ps1 -Suite scriptpubkeyman_tests   # fast iterate
    contrib\test-asan-local.ps1                                 # full make check
    contrib\test-asan-local.ps1 -Clean                          # scratch rebuild
#>
[CmdletBinding()]
param(
    [string]$Suite,
    [switch]$Check,
    [int]$Jobs = 0,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$RepoRoot  = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Image     = 'rincoin-local:asan'
$BuildVol  = 'rincoin_local_build_asan'   # source copy + objects + configure
$CcacheVol = 'rincoin_local_ccache_asan'  # ccache

function Info($m) { Write-Host $m -ForegroundColor Cyan }
function Fail($m) { Write-Host "ERROR: $m" -ForegroundColor Red; exit 1 }

# --- 1. Prerequisites ------------------------------------------------------
Info '== Checking Docker =='
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Fail 'Docker not found. Install Docker Desktop and start it.'
}
try { docker info *> $null } catch { Fail 'Docker daemon not responding. Start Docker Desktop and try again.' }

# --- 2. Clean --------------------------------------------------------------
if ($Clean) {
    Info '== Removing cached build/ccache volumes =='
    docker volume rm $BuildVol $CcacheVol *> $null
}

# --- 3. Build image (matches CI package set) -------------------------------
Info '== Preparing ASan image (cached after first run) =='
$dockerfile = @'
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
'@
$dockerfile | docker build -t $Image -f - $RepoRoot
if ($LASTEXITCODE -ne 0) { Fail 'Docker image build failed.' }

# --- 4. Run build + tests --------------------------------------------------
$runFull = (-not $Suite) -or $Check
$mode = if ($runFull) { 'check' } else { "suite:$Suite" }
Info "== Building + testing (mode: $mode) =="

$inner = @'
set -euo pipefail
JOBS_ARG="__JOBS__"
MODE="__MODE__"
export CCACHE_DIR=/ccache
[ -n "$JOBS_ARG" ] || JOBS_ARG="-j$(nproc)"

# Sync source into the persistent build volume (keep objects for incremental).
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
  # Exactly mirror ci/test/00_setup_env_native_asan.sh's BITCOIN_CONFIG.
  ./configure --disable-dependency-tracking \
      --enable-zmq --with-incompatible-bdb --without-gui \
      CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER' \
      --with-sanitizers=address,integer,undefined \
      --with-boost-process \
      CC="ccache clang" CXX="ccache clang++"
fi

make $JOBS_ARG

if [ "$MODE" = "check" ]; then
  make $JOBS_ARG check VERBOSE=1
else
  SUITE="${MODE#suite:}"
  BIN="$(find src/test -maxdepth 1 -type f -executable -name 'test_*' | head -n1)"
  [ -n "$BIN" ] || { echo "unit test binary not found"; exit 1; }
  echo ">> Running suite '$SUITE' from $BIN"
  "$BIN" --catch_system_errors=no -l test_suite -t "$SUITE"
fi
ccache --show-stats | tail -n 5 || true
'@
$jobsArg = if ($Jobs -gt 0) { "-j$Jobs" } else { '' }
$inner = $inner.Replace('__JOBS__', $jobsArg).Replace('__MODE__', $mode)

docker run --rm `
    -v "${RepoRoot}:/src:ro" `
    -v "${BuildVol}:/build" `
    -v "${CcacheVol}:/ccache" `
    $Image bash -c $inner
$code = $LASTEXITCODE

Info ''
if ($code -eq 0) {
    Info '== PASSED (matches the asan+ubsan CI leg config) =='
    Info '  Iterate: edit code, re-run this script (incremental + ccache = fast).'
    Info '  Fast single suite:  contrib\test-asan-local.ps1 -Suite <suite_name>'
} else {
    Fail "Tests failed (exit $code). Fix and re-run; the build stays cached."
}
