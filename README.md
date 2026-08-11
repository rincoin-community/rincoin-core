
# RinCoin Core

> ### ⚠ This release stops at block height 840,000
>
> This is a **terminal release** for the current lineage. It does not implement
> the height-840,000 consensus rules — the community review at
> [consensus-840k](https://github.com/rincoin-community/consensus-840k) is still
> open — so it validates normally up to block 839,999 and then shuts down
> instead of connecting a block at 840,000. It warns persistently for the ~30
> days beforehand.
>
> It changes no consensus rule, no protocol version and no block or transaction
> format, so it cannot cause a chain split, and the chain it leaves behind is
> directly resumable by a successor release. **Install a release that implements
> the selected rules before block 840,000**, or your node will stop. See the
> [release notes](doc/release-notes-rincoin.md) and
> [`doc/rincoin-parameters.md`](doc/rincoin-parameters.md) §6.

RinCoin is a decentralized digital currency, based on Bitcoin Core, that introduces a new Proof-of-Work hashing algorithm called **RinHash**. RinHash is a hybrid PoW algorithm designed for both security and ASIC-resistance, combining BLAKE3, Argon2d, and SHA3-256. This README provides an overview of RinCoin’s specifications, the RinHash algorithm, and network parameters.

## Key Specifications

- **Coin Name / Ticker:** RinCoin (**RIN**)  
- **Consensus Mechanism:** Proof-of-Work (PoW) – **RinHash** algorithm (BLAKE3 → Argon2d → SHA3-256)  
- **Block Target Time:** 1 minute (60 seconds per block)  
- **Block Reward:** 50 RIN (initial coinbase reward per block)  
- **Halving Schedule:** Reward halves every 210,000 blocks (~145 days at 1 min blocks)  
- **Difficulty Adjustment:** Every 2016 blocks (~33.6 hours)  
- **Proof-of-Work Hash:** 256-bit output  
- **Address Format:** Base58 addresses start with **R**  
- **Network Ports:** P2P: 9555, RPC: 9556  
- **Network Magic:** 0x52 0x49 0x4E 0x43 ("RINC")  

## Proof-of-Work Algorithm: RinHash

RinHash is a custom proof-of-work algorithm using:

1. **BLAKE3**: Fast initial hashing  
2. **Argon2d**: Memory-hard step to resist ASICs  
3. **SHA3-256**: Final standard cryptographic hash

A valid block satisfies:  
`SHA3-256( Argon2d( BLAKE3(block_header) )) < Target`

This design provides:
- Fast verification
- Memory-hardness to deter ASICs
- Compatibility with existing 256-bit PoW frameworks

## Network and Usage

- **Magic bytes:** `0x52 0x49 0x4E 0x43`  
- **Ports:** 9555 (P2P), 9556 (RPC)  
- **Mining:** CPU/GPU mining supported  
- **Wallet:** Full-node wallet with RIN units

## Building Rincoin

For detailed instructions on building release binaries for Linux and Windows, see [doc/build-release.md](doc/build-release.md).

Quick start for building from source:
- [Linux/Unix Build Notes](doc/build-unix.md)
- [Windows Build Notes](doc/build-windows.md)
- [Release Build Guide](doc/build-release.md)

## Release Notes

- **Release history (canonical):** [doc/release-notes-rincoin.md](doc/release-notes-rincoin.md). A longer v1.1.0 narrative at [release-notes-rincoin.md](release-notes-rincoin.md) is being reconciled with it before the next release.
- **Historical upstream notes (reference only, not Rincoin):** [doc/bitcoin-release-notes/](doc/bitcoin-release-notes/) and [doc/litecoin-release-notes/](doc/litecoin-release-notes/).

## Developer Notes

See `chainparams.cpp` for network configuration.  
See `GetPoWHash()` for RinHash implementation.

## Rincoin Community
[![Discord Banner 2](https://discord.com/api/guilds/1354664874176680017/widget.png?style=banner2)](https://discord.gg/Ap7TUXYRBf)