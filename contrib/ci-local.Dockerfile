# Shared image for the LOCAL CI-parity runners (contrib/test-asan-local.*, and
# any plain-leg local runs). It carries BOTH toolchains so a single image serves
# every leg driven by contrib/ci-local-runner.sh:
#   * clang/llvm    -> LEG=asan  (mirrors ci/test/00_setup_env_native_asan.sh)
#   * build-essential (gcc/g++) -> LEG=plain (fast functional-test iteration)
#
# The Python deps (blake3, argon2-cffi) are what the functional test framework
# needs to compute the RinHash block id/PoW in pure Python
# (test/functional/test_framework/rinhash.py), matching ci/test/04_install.sh.
#
# This is the single source of truth for the local image; the thin .ps1/.sh
# wrappers only `docker build -f` this file. Do not duplicate the package set.
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive TZ=UTC
RUN apt-get update && apt-get install -y \
      build-essential libtool autotools-dev automake pkg-config bsdmainutils \
      python3 python3-pip python3-zmq ccache rsync git ca-certificates dos2unix \
      clang llvm \
      qtbase5-dev qttools5-dev-tools libevent-dev \
      libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev \
      libdb5.3++-dev libminiupnpc-dev libzmq3-dev libqrencode-dev libsqlite3-dev \
      libssl-dev libfmt-dev \
 && rm -rf /var/lib/apt/lists/* \
 && pip3 install --no-cache-dir blake3 argon2-cffi
WORKDIR /build
