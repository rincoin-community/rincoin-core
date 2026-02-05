#!/bin/bash
# Build script for test-dns-seeds utility
# This builds the utility as part of the main rincoin build

set -e

echo "Building DNS Seed Testing Utility..."

# Check if we're in the rincoin directory
if [ ! -f "configure.ac" ]; then
    echo "Error: This script must be run from the rincoin root directory"
    exit 1
fi

# Method 1: Build using existing build system if available
if [ -f "Makefile" ]; then
    echo "Using existing build system..."
    make src/test-dns-seeds
    echo "✓ Build complete: src/test-dns-seeds"
    echo ""
    echo "You can now run the utility:"
    echo "  ./src/test-dns-seeds -help"
    exit 0
fi

# Method 2: Simple direct compilation
echo "Building with direct compilation..."

CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -I. -Isrc -Isrc/secp256k1/include -Isrc/univalue/include"

# Check for bitcoin-config.h
if [ -f "src/config/bitcoin-config.h" ]; then
    CXXFLAGS="$CXXFLAGS -DHAVE_CONFIG_H"
elif [ -f "bitcoin-config.h" ]; then
    CXXFLAGS="$CXXFLAGS -DHAVE_CONFIG_H"
else
    echo "Warning: bitcoin-config.h not found. Run ./autogen.sh && ./configure first."
    echo "Attempting build anyway..."
fi

# Compile
$CXX $CXXFLAGS \
    src/test-dns-seeds.cpp \
    src/netbase.cpp \
    src/netaddress.cpp \
    src/chainparams.cpp \
    src/chainparamsbase.cpp \
    src/util/strencodings.cpp \
    src/util/system.cpp \
    src/util/time.cpp \
    src/util/threadnames.cpp \
    src/logging.cpp \
    src/random.cpp \
    src/randomenv.cpp \
    src/fs.cpp \
    src/sync.cpp \
    src/arith_uint256.cpp \
    src/uint256.cpp \
    src/utilstrencodings.cpp \
    src/hash.cpp \
    src/crypto/sha256.cpp \
    src/crypto/sha256_sse4.cpp \
    src/crypto/hmac_sha512.cpp \
    src/crypto/hmac_sha256.cpp \
    src/crypto/sha512.cpp \
    src/crypto/ripemd160.cpp \
    src/crypto/rinhash.cpp \
    src/support/cleanse.cpp \
    src/support/lockedpool.cpp \
    -lpthread -lboost_system -lboost_filesystem -lboost_thread \
    -o test-dns-seeds

echo ""
echo "✓ Build complete: ./test-dns-seeds"
echo ""
echo "You can now run the utility:"
echo "  ./test-dns-seeds -help"
