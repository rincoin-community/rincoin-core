#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

# Plain (no-sanitizer) native build that runs the unit and functional test
# suites. Kept fork-appropriate: it does not download upstream previous
# releases (those tags do not exist for Rincoin) and builds against the
# system BDB via --with-incompatible-bdb so no depends build is required.
export CONTAINER_NAME=ci_native
export PACKAGES="python3-zmq python3-pip qtbase5-dev qttools5-dev-tools libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libzmq3-dev libqrencode-dev libsqlite3-dev libssl-dev libfmt-dev"
export DOCKER_NAME_TAG=ubuntu:20.04
export NO_DEPENDS=1
export GOAL="install"
export BITCOIN_CONFIG="--enable-zmq --with-incompatible-bdb --without-gui --with-boost-process"
