#!/usr/bin/env bash
#
# test-asan-local.sh
#
# Run the CI ASan/UBSan build + tests LOCALLY in Docker, reproducing the
# GitHub Actions "asan+ubsan" leg so you can iterate without commit/push/wait.
#
# Thin wrapper: it only builds the shared image (contrib/ci-local.Dockerfile),
# manages the cache volumes, and runs the shared container-side logic
# (contrib/ci-local-runner.sh) with LEG=asan. All build/config/test logic lives
# in that one script, so this wrapper and its .ps1 sibling can never drift.
# Green here => should be green on the asan+ubsan CI leg.
#
# Requires Docker. The build tree, objects and ccache live in Docker named
# volumes: first run is slow (~15-25 min), later runs are incremental/fast.
# Nothing is written into the repo or git.
#
# Usage:
#   contrib/test-asan-local.sh                              # full 'make check'
#   contrib/test-asan-local.sh --suite scriptpubkeyman_tests   # one suite, fast
#   contrib/test-asan-local.sh --functional feature_min_peer_proto_floor.py
#   contrib/test-asan-local.sh --jobs 8                     # set parallelism
#   contrib/test-asan-local.sh --load-hogs 4               # CPU-saturate tests
#   contrib/test-asan-local.sh --clean                     # wipe caches, rebuild

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE='rincoin-local:asan'
BUILD_VOL='rincoin_local_build_asan'
CCACHE_VOL='rincoin_local_ccache_asan'
SUITE=''
FUNCTIONAL=''
CHECK=0
JOBS=''
LOAD_HOGS=0
DO_CLEAN=0

while [ $# -gt 0 ]; do
  case "$1" in
    --suite)      SUITE="$2"; shift 2 ;;
    --functional) FUNCTIONAL="$2"; shift 2 ;;
    --check)      CHECK=1; shift ;;
    --jobs)       JOBS="-j$2"; shift 2 ;;
    --load-hogs)  LOAD_HOGS="$2"; shift 2 ;;
    --clean)      DO_CLEAN=1; shift ;;
    -h|--help)    grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
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

info '== Preparing local image (single source of truth: contrib/ci-local.Dockerfile) =='
docker build -t "$IMAGE" -f "$REPO_ROOT/contrib/ci-local.Dockerfile" "$REPO_ROOT"

if [ -n "$FUNCTIONAL" ]; then
  MODE="func:$FUNCTIONAL"
elif [ -n "$SUITE" ] && [ "$CHECK" -eq 0 ]; then
  MODE="suite:$SUITE"
else
  MODE='check'
fi
info "== Building + testing (LEG=asan, mode: $MODE) =="

# ASan needs a relaxed seccomp profile + SYS_PTRACE (see ci-local-runner.sh).
# Pipe the runner through `tr -d '\r'` so a CRLF checkout cannot break it.
docker run --rm \
    --security-opt seccomp=unconfined --cap-add SYS_PTRACE \
    -e "LEG=asan" -e "MODE=$MODE" -e "JOBS_ARG=$JOBS" -e "LOAD_HOGS=$LOAD_HOGS" \
    -v "${REPO_ROOT}:/src:ro" \
    -v "${BUILD_VOL}:/build" \
    -v "${CCACHE_VOL}:/ccache" \
    "$IMAGE" bash -c "tr -d '\r' < /src/contrib/ci-local-runner.sh | bash"

info ''
info '== PASSED (matches the asan+ubsan CI leg config) =='
info '  Iterate: edit code, re-run (incremental + ccache = fast).'
info '  Fast single suite:  contrib/test-asan-local.sh --suite <suite_name>'
