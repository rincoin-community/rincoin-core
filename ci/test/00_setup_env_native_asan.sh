#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_asan
export PACKAGES="clang llvm python3-zmq qtbase5-dev qttools5-dev-tools libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libzmq3-dev libqrencode-dev libsqlite3-dev libssl-dev libfmt-dev"
export DOCKER_NAME_TAG=ubuntu:20.04
export NO_DEPENDS=1
export GOAL="install"
export BITCOIN_CONFIG="--enable-zmq --with-incompatible-bdb --without-gui CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER' --with-sanitizers=address,integer,undefined CC=clang CXX=clang++ --with-boost-process"
# The functional suite is gated on the plain native_ci leg (see
# test/functional/ci_passing_tests.txt). The asan leg keeps to unit tests plus
# the sanitizers here; enabling the functional allowlist under ASan in CI is a
# follow-up once we confirm the clang ASan runtime starts cleanly on the CI
# runner (locally it needed ASLR disabled via setarch -R).
export RUN_FUNCTIONAL_TESTS=false
