#!/usr/bin/env bash
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

# Git (>= 2.35.2, and security-backported builds such as Ubuntu 20.04's git)
# refuses to operate on a repository owned by a different user. In the CI
# container the bind-mounted work tree is owned by the host user while commands
# run as root, so mark it safe before any git-invoking build step. In
# particular `make distdir` runs `git archive` to embed clientversion/build
# info and would otherwise fail with "detected dubious ownership".
DOCKER_EXEC git config --global --add safe.directory "${BASE_ROOT_DIR}"

BITCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$DEPENDS_DIR/$HOST --bindir=$BASE_OUTDIR/bin --libdir=$BASE_OUTDIR/lib"
DOCKER_EXEC "ccache --zero-stats --max-size=$CCACHE_SIZE"

BEGIN_FOLD autogen
if [ -n "$CONFIG_SHELL" ]; then
  DOCKER_EXEC "$CONFIG_SHELL" -c "./autogen.sh"
else
  DOCKER_EXEC ./autogen.sh
fi
END_FOLD

export P_CI_DIR="${BASE_ROOT_DIR}"

BEGIN_FOLD configure
DOCKER_EXEC ./configure --cache-file=config.cache $BITCOIN_CONFIG_ALL $BITCOIN_CONFIG || ( (DOCKER_EXEC cat config.log) && false)
END_FOLD

# Build in-tree (srcdir == builddir), mirroring how release binaries are built.
# We do NOT build from a `make distdir` copy (the dist tarball is incomplete for
# some vendored components: argon2, libmw and its bundled deps do not declare
# all of their headers for `make dist`), nor as a separate VPATH tree (a few
# fork-added libmw test-framework include paths are not written VPATH-relative,
# e.g. -Ilibmw/test/framework/include). An in-tree build sidesteps both issues
# and is the most representative of the actual release build.

set -o errtrace
trap 'DOCKER_EXEC "cat ${BASE_SCRATCH_DIR}/sanitizer-output/* 2> /dev/null"' ERR

if [[ ${USE_MEMORY_SANITIZER} == "true" ]]; then
  # MemorySanitizer (MSAN) does not support tracking memory initialization done by
  # using the Linux getrandom syscall. Avoid using getrandom by undefining
  # HAVE_SYS_GETRANDOM. See https://github.com/google/sanitizers/issues/852 for
  # details.
  DOCKER_EXEC 'grep -v HAVE_SYS_GETRANDOM src/config/bitcoin-config.h > src/config/bitcoin-config.h.tmp && mv src/config/bitcoin-config.h.tmp src/config/bitcoin-config.h'
fi

BEGIN_FOLD build
DOCKER_EXEC make $MAKEJOBS $GOAL || ( echo "Build failure. Verbose build follows." && DOCKER_EXEC make $GOAL V=1 ; false )
END_FOLD

BEGIN_FOLD cache_stats
DOCKER_EXEC "ccache --version | head -n 1 && ccache --show-stats"
DOCKER_EXEC du -sh "${DEPENDS_DIR}"/*/
DOCKER_EXEC du -sh "${PREVIOUS_RELEASES_DIR}"
END_FOLD
