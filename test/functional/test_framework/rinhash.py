# Copyright (c) 2024-2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""RinHash proof-of-work hash for the functional test framework.

Mirrors src/crypto/rinhash.cpp:
    1. BLAKE3 of the 80-byte serialized block header
    2. Argon2d with salt="RinCoinSalt", t=2, m=64 KiB, p=1, output_len=32
    3. SHA3-256 of the Argon2d output

Exposes getPoWHash() so it is a drop-in replacement for the Litecoin
`litecoin_scrypt` module the framework previously imported.

Requires:  pip install blake3 argon2-cffi
"""

import hashlib

import blake3 as _blake3_mod
from argon2.low_level import hash_secret_raw as _argon2d_raw, Type as _Argon2Type

_ARGON2_SALT = b"RinCoinSalt"
_ARGON2_T_COST = 2
_ARGON2_M_COST = 64   # kibibytes
_ARGON2_PARALLELISM = 1
_ARGON2_HASH_LEN = 32


def getPoWHash(header_bytes: bytes) -> bytes:
    """Return the 32-byte RinHash of *header_bytes* (an 80-byte block header).

    The result is in internal (little-endian) byte order, matching what
    uint256_from_str() expects, exactly like the previous scrypt module.
    """
    b3 = _blake3_mod.blake3(header_bytes).digest()
    a2 = _argon2d_raw(
        secret=b3,
        salt=_ARGON2_SALT,
        time_cost=_ARGON2_T_COST,
        memory_cost=_ARGON2_M_COST,
        parallelism=_ARGON2_PARALLELISM,
        hash_len=_ARGON2_HASH_LEN,
        type=_Argon2Type.D,
    )
    return hashlib.sha3_256(a2).digest()


# Alias matching the electrin implementation's function name.
rinhash = getPoWHash
