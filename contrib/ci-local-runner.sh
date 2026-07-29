#!/usr/bin/env bash
#
# ci-local-runner.sh — container-side build+test logic for the LOCAL CI-parity
# runners. This is the SINGLE SOURCE OF TRUTH: the thin host wrappers
# (contrib/test-asan-local.ps1 / .sh) only build the image, manage the cache
# volumes and `docker run` this script. Never re-implement build/config logic in
# a wrapper — add it here so every host stays in lockstep.
#
# It runs INSIDE the contrib/ci-local.Dockerfile image, driven entirely by env:
#
#   LEG       asan | plain            (default: asan)
#               asan  -> clang + --with-sanitizers=address,integer,undefined,
#                        mirroring ci/test/00_setup_env_native_asan.sh.
#               plain -> gcc, no sanitizers; fast functional-test iteration.
#   MODE      check | suite:<name> | func:<spec>   (default: check)
#               check       -> make check (all unit tests)
#               suite:NAME  -> one Boost unit suite from the test binary
#               func:SPEC   -> test/functional/test_runner.py SPEC. SPEC is
#                              space-separated bare test names, OR ';'-separated
#                              entries when an entry needs its own flags, e.g.
#                              "wallet_dump.py --legacy-wallet;p2p_addr.py".
#   JOBS_ARG  make -j flag, e.g. -j4   (default: -j<nproc>)
#   LOAD_HOGS integer N                (default: 0) — spawn N `yes` CPU hogs for
#               the duration of the test phase, to reproduce scheduler-starvation
#               races (e.g. feature_min_peer_proto_floor) under load.
#
# The source is mounted read-only at /src and rsync'd into the persistent build
# volume at /build/rincoin so objects survive between runs (ccache in /ccache).

set -euo pipefail

LEG="${LEG:-asan}"
MODE="${MODE:-check}"
JOBS_ARG="${JOBS_ARG:-}"
[ -n "$JOBS_ARG" ] || JOBS_ARG="-j$(nproc)"
export CCACHE_DIR=/ccache

# --- Sync source into the persistent build volume --------------------------
mkdir -p /build/rincoin
# No --delete on the main sync: preserve the incremental build objects.
rsync -a --exclude=.git /src/ /build/rincoin/
# The test tree holds no build artifacts, so mirror it with --delete to drop any
# stale scripts left behind by renames/removals in the source. Keep the
# configure-generated test/config.ini and any __pycache__.
rsync -a --delete --exclude=config.ini --exclude='__pycache__' /src/test/ /build/rincoin/test/
cd /build/rincoin

# A Windows checkout may store scripts with CRLF; normalize build/autotools and
# the extensionless sanitizer-suppression files to LF (-k keeps mtimes so
# autotools does not needlessly re-run configure).
find . -type f \( -name '*.sh' -o -name '*.ac' -o -name '*.am' -o -name '*.m4' \
    -o -name '*.mk' -o -name '*.include' -o -name '*.py' -o -name 'configure' \) \
    -print0 | xargs -0 -r dos2unix -k -q 2>/dev/null || true
dos2unix -k -q test/sanitizer_suppressions/* 2>/dev/null || true

ccache --zero-stats --max-size=2G >/dev/null 2>&1 || true

[ -x configure ] || ./autogen.sh

# --- Per-leg configuration -------------------------------------------------
# NB: do NOT add --disable-dependency-tracking here. This runner reuses object
# files across edits, so make must track header dependencies; otherwise editing
# a widely-included header (e.g. consensus/params.h) recompiles only the .cpp
# you touched, leaving the rest with a stale struct layout -> ABI mismatch ->
# the node SIGSEGVs at startup ("Error: no RPC connection"). CI does a clean
# build and omits the flag too, so keeping it off also matches CI exactly.
SETARCH=''
if [ "$LEG" = "asan" ]; then
  # Exact CI sanitizer runtime options (ci/test/04_install.sh).
  export ASAN_OPTIONS="detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1"
  export LSAN_OPTIONS="suppressions=$PWD/test/sanitizer_suppressions/lsan"
  export UBSAN_OPTIONS="suppressions=$PWD/test/sanitizer_suppressions/ubsan:print_stacktrace=1:halt_on_error=1:report_error_type=1"
  CONFIGURE_ARGS=(--enable-zmq --with-incompatible-bdb --with-sqlite=yes --without-gui
      CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER'
      --with-sanitizers=address,integer,undefined
      --with-boost-process
      CC=clang CXX=clang++)
  # Disable ASLR for the sanitized binaries: the clang-10 ASan runtime on Ubuntu
  # 20.04 segfaults at startup under Docker Desktop's high mmap_rnd_bits entropy.
  # Needs the relaxed seccomp profile + SYS_PTRACE set on `docker run`.
  SETARCH="setarch $(uname -m) -R"
elif [ "$LEG" = "plain" ]; then
  CONFIGURE_ARGS=(--enable-zmq --with-incompatible-bdb --with-sqlite=yes --without-gui
      --with-boost-process
      CC=gcc CXX=g++)
else
  echo "ci-local-runner: unknown LEG '$LEG' (expected asan|plain)" >&2
  exit 2
fi

if [ ! -f config.status ]; then
  ./configure "${CONFIGURE_ARGS[@]}"
fi

make $JOBS_ARG

# --- Optional CPU saturation (race reproduction) ---------------------------
if [ "${LOAD_HOGS:-0}" -gt 0 ]; then
  echo ">> Spawning ${LOAD_HOGS} CPU hog(s) for the test phase"
  for _ in $(seq 1 "${LOAD_HOGS}"); do yes >/dev/null & done
  # shellcheck disable=SC2064
  trap "kill $(jobs -p) 2>/dev/null || true" EXIT
fi

# --- Run the requested tests -----------------------------------------------
case "$MODE" in
  check)
    $SETARCH make $JOBS_ARG check VERBOSE=1
    ;;
  suite:*)
    SUITE="${MODE#suite:}"
    BIN="$(find src/test -maxdepth 1 -type f -executable -name 'test_*' | head -n1)"
    [ -n "$BIN" ] || { echo "unit test binary not found (was it built with --disable-tests?)" >&2; exit 1; }
    echo ">> Running suite '$SUITE' from $BIN"
    $SETARCH "$BIN" --catch_system_errors=no -l test_suite -t "$SUITE"
    ;;
  func:*)
    SPEC="${MODE#func:}"
    echo ">> Running functional test(s): $SPEC"
    if [ "${SPEC#*;}" != "$SPEC" ]; then
      # ';'-separated: each entry is one argument (may carry its own flags).
      IFS=';' read -ra _tests <<< "$SPEC"
      $SETARCH python3 test/functional/test_runner.py "${_tests[@]}"
    else
      # space-separated bare test names.
      $SETARCH python3 test/functional/test_runner.py $SPEC
    fi
    ;;
  *)
    echo "ci-local-runner: unknown MODE '$MODE' (expected check|suite:<name>|func:<spec>)" >&2
    exit 2
    ;;
esac

ccache --show-stats | tail -n 5 || true
