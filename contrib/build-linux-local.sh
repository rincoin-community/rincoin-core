#!/usr/bin/env bash
#
# build-linux-local.sh
#
#   LOCAL, TESTING-ONLY native Linux build of Rincoin Core.
#
#   >>> These binaries are for LOCAL TESTING / VERIFICATION ONLY.           <<<
#   >>> They are UNSIGNED and NOT reproducible (they link against your      <<<
#   >>> system libraries). Do NOT publish them or use them for official     <<<
#   >>> releases — use the project's reproducible release process for that  <<<
#   >>> (contrib/build_release.sh).                                         <<<
#
# Optimised for the fastest possible edit-build-test loop:
#   * links against system libraries (no slow depends/ build);
#   * uses ccache, so recompiles after code changes are near-instant;
#   * builds in-tree and incrementally — object files are reused between runs
#     and are already git-ignored, so nothing is added to the repo.
#
# Usage:
#   contrib/build-linux-local.sh            # build (incremental, cached)
#   contrib/build-linux-local.sh --deps     # apt-install prerequisites first
#   contrib/build-linux-local.sh --gui      # also build the Qt GUI (rincoin-qt)
#   contrib/build-linux-local.sh --clean    # wipe the build, then rebuild
#
# Binaries are also copied to release-builds-local/linux/ (git-ignored).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$REPO_ROOT/release-builds-local/linux"
WITH_GUI=0
DO_DEPS=0
DO_CLEAN=0

for arg in "$@"; do
  case "$arg" in
    --gui)   WITH_GUI=1 ;;
    --deps)  DO_DEPS=1 ;;
    --clean) DO_CLEAN=1 ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; exit 1 ;;
  esac
done

info() { printf '\033[36m%s\033[0m\n' "$*"; }
fail() { printf '\033[31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

cd "$REPO_ROOT"

# --- 1. Optional: install prerequisites -----------------------------------
if [ "$DO_DEPS" -eq 1 ]; then
  info '== Installing build prerequisites (sudo apt) =='
  command -v apt-get >/dev/null 2>&1 || fail 'apt-get not found; install the dependencies manually for your distro.'
  PKGS=(build-essential libtool autotools-dev automake pkg-config bsdmainutils
        python3 ccache
        libevent-dev libboost-dev
        libdb-dev libdb++-dev
        libminiupnpc-dev libnatpmp-dev libzmq3-dev)
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS+=(qtbase5-dev qttools5-dev qttools5-dev-tools libqrencode-dev libprotobuf-dev protobuf-compiler)
  fi
  sudo apt-get update
  sudo apt-get install -y "${PKGS[@]}"
fi

# --- 2. Prerequisite checks ------------------------------------------------
info '== Checking prerequisites =='
command -v gcc  >/dev/null 2>&1 || fail 'gcc not found. Re-run with --deps to install prerequisites.'
command -v make >/dev/null 2>&1 || fail 'make not found. Re-run with --deps to install prerequisites.'
if ! command -v ccache >/dev/null 2>&1; then
  info 'ccache not found — building without it (slower recompiles). Re-run with --deps to enable it.'
  CC_LAUNCH=''
else
  CC_LAUNCH='ccache '
fi

# --- 3. Clean if requested -------------------------------------------------
if [ "$DO_CLEAN" -eq 1 ]; then
  info '== Cleaning previous build =='
  [ -f Makefile ] && make distclean || true
  rm -rf "$OUT_DIR"
fi
mkdir -p "$OUT_DIR"

# --- 4. Configure once, then build incrementally ---------------------------
GUI_FLAG='--without-gui'
[ "$WITH_GUI" -eq 1 ] && GUI_FLAG='--with-gui=qt5'

if [ ! -x configure ]; then
  info '== Running autogen.sh =='
  ./autogen.sh
fi
if [ ! -f config.status ]; then
  info '== Configuring (system libs, testing-only) =='
  ./configure \
      "$GUI_FLAG" \
      --disable-tests --disable-bench \
      --with-incompatible-bdb \
      --enable-reduce-exports \
      CC="${CC_LAUNCH}gcc" CXX="${CC_LAUNCH}g++"
fi

info '== Building (incremental; ccache accelerates recompiles) =='
make -j"$(nproc)"

# --- 5. Collect binaries ---------------------------------------------------
for b in src/rincoind src/rincoin-cli src/rincoin-tx src/rincoin-wallet src/qt/rincoin-qt; do
  [ -f "$b" ] && cp "$b" "$OUT_DIR/" || true
done
( cd "$OUT_DIR" && sha256sum ./* > SHA256SUMS.txt 2>/dev/null || true )

info ''
info "== Done. Binaries in: $OUT_DIR =="
ls -1 "$OUT_DIR"
info ''
info 'What now:'
info '  * These are UNSIGNED, testing-only binaries — run/verify them locally.'
info '  * Edit code, then re-run this script: only changed files recompile.'
info '  * Full clean rebuild:  contrib/build-linux-local.sh --clean'
info '  * For an official build, use the reproducible release process instead.'
