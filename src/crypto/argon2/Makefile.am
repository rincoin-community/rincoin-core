noinst_LIBRARIES = libargon2.a

# Optionally built SIMD-optimized libraries
if ENABLE_SSE41
noinst_LIBRARIES += libargon2_ssse3.a
endif

if ENABLE_AVX2
noinst_LIBRARIES += libargon2_avx2.a
endif

if ENABLE_AVX512F
noinst_LIBRARIES += libargon2_avx512.a
endif

# Core Argon2 sources (always compiled - includes reference implementation)
libargon2_a_SOURCES = \
    core.c \
    blake2/blake2b.c \
    encoding.c \
    argon2.c \
    ref.c \
    thread.c \
    argon2_dispatch.c

# Headers required to build the library. These must be listed so that
# `make dist`/`make distdir` ships them; otherwise an out-of-tree build from
# the distribution tarball fails with "core.h: No such file or directory".
noinst_HEADERS = \
    core.h \
    encoding.h \
    thread.h \
    argon2_dispatch.h \
    genkat.h \
    include/argon2.h \
    blake2/blake2.h \
    blake2/blake2-impl.h \
    blake2/blamka-round-ref.h \
    blake2/blamka-round-opt.h \
    blake2/blamka-round-ssse3.h \
    blake2/blamka-round-avx2.h \
    blake2/blamka-round-avx512.h

# Base compiler flags
AM_CFLAGS = -O2 -fPIC
AM_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/blake2

# SSSE3 optimized library (128-bit SIMD)
libargon2_ssse3_a_CFLAGS = $(AM_CFLAGS) -mssse3
libargon2_ssse3_a_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_SSSE3
libargon2_ssse3_a_SOURCES = argon2_ssse3.c

# AVX2 optimized library (256-bit SIMD)
libargon2_avx2_a_CFLAGS = $(AM_CFLAGS) -mavx2
libargon2_avx2_a_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_AVX2
libargon2_avx2_a_SOURCES = argon2_avx2.c

# AVX-512 optimized library (512-bit SIMD)
libargon2_avx512_a_CFLAGS = $(AM_CFLAGS) -mavx512f -mavx512dq
libargon2_avx512_a_CPPFLAGS = $(AM_CPPFLAGS) -DENABLE_AVX512
libargon2_avx512_a_SOURCES = argon2_avx512.c
