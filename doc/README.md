# Rincoin Core Documentation

This directory contains Rincoin-specific documentation together with technical documentation inherited from Bitcoin Core and Litecoin Core.

For Rincoin consensus and network behaviour, the **source code is authoritative**. Rincoin-specific documentation should be preferred over inherited upstream documentation where the two differ.

## Quickstart Guide

### Setup 

Rincoin Core is the Rincoin client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Rincoin transactions, which requires approximately 1 gigabyte of disk space (August 2026). Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more - the whole blockchain must be downloaded and hashes of all blocks checked first.

To download Rincoin Core, visit [releases](https://github.com/rincoin-community/rincoin-core/releases).

### Running

The following are some helpful notes on how to run Rincoin Core on your native platform.

#### Unix

Unpack the files into a directory and run:

- `bin/rincoin-qt` (GUI) or
- `bin/rincoind` (headless)

#### Windows

Unpack the files into a directory, and then run `rincoin-qt.exe`.

#### macOS

Drag Rincoin Core to your applications folder, and then run Rincoin Core.


## Rincoin-specific documentation

- [`rincoin-parameters.md`](rincoin-parameters.md) — network and consensus parameters, with references to their implementation.
- [`release-notes-rincoin.md`](release-notes-rincoin.md) — consolidated Rincoin release history.
- [`build-rincoin-release.md`](build-rincoin-release.md) — Rincoin release build instructions.
- [`dns-seed-tester.md`](dns-seed-tester.md) — the DNS-seed testing utility.

Project-wide development documents are in the repository root:

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — how to contribute.
- [`../GOVERNANCE.md`](../GOVERNANCE.md) — development roles and decision-making.
- [`../SECURITY.md`](../SECURITY.md) — reporting security vulnerabilities.
- [`../BRANCHES.md`](../BRANCHES.md) — purpose and status of the main repository branches.

## Building

- [`dependencies.md`](dependencies.md) — build dependencies.
- [`build-unix.md`](build-unix.md) — Linux and other Unix-like systems.
- [`build-windows.md`](build-windows.md) — Windows.
- [`build-osx.md`](build-osx.md) — macOS.
- [`build-freebsd.md`](build-freebsd.md) — FreeBSD.
- [`build-openbsd.md`](build-openbsd.md) — OpenBSD.
- [`build-netbsd.md`](build-netbsd.md) — NetBSD.

Some build documents originate upstream. Package names, paths, optional features, or release procedures may require Rincoin-specific adjustments.

## Development and interfaces

- [`developer-notes.md`](developer-notes.md) — developer notes and coding practices.
- [`JSON-RPC-interface.md`](JSON-RPC-interface.md) — JSON-RPC interface.
- [`REST-interface.md`](REST-interface.md) — REST interface.
- [`shared-libraries.md`](shared-libraries.md) — shared libraries.
- [`descriptors.md`](descriptors.md) — output descriptors.
- [`psbt.md`](psbt.md) — PSBT support.
- [`zmq.md`](zmq.md) — ZeroMQ notifications.
- [`fuzzing.md`](fuzzing.md) — fuzz testing.
- [`benchmarking.md`](benchmarking.md) — benchmarking.
- [`files.md`](files.md) — data files and directories.
- [`tor.md`](tor.md) — Tor support.
- [`reduce-memory.md`](reduce-memory.md) — reducing memory usage.
- [`reduce-traffic.md`](reduce-traffic.md) — reducing network traffic.

## Mining and network infrastructure

Rincoin-specific network and Proof-of-Work parameters are documented in [`rincoin-parameters.md`](rincoin-parameters.md).

The implementation itself remains the source of truth, in particular:

- `src/chainparams.cpp` — network parameters and activation heights;
- `src/consensus/` — consensus definitions;
- `src/pow.cpp` — difficulty adjustment and Proof-of-Work checks;
- `src/crypto/rinhash.cpp` — RinHash implementation.

Infrastructure operators should also review repository documentation and release notes for changes affecting peer compatibility, DNS seeds, protocol versions, or deployment.

## Release history and upstream documentation

[`release-notes-rincoin.md`](release-notes-rincoin.md) is the Rincoin release history.

The directories below are retained as upstream reference material:

- [`bitcoin-release-notes/`](bitcoin-release-notes/)
- [`litecoin-release-notes/`](litecoin-release-notes/)

They document the projects from which Rincoin Core descends and are useful when tracing inherited code or evaluating upstream fixes. They are **not Rincoin release notes**.

Several other documents in this directory also originate from Bitcoin Core or Litecoin Core. Upstream terminology or defaults appearing in those documents should not be assumed to describe the Rincoin network. When in doubt, verify against the Rincoin source code and [`rincoin-parameters.md`](rincoin-parameters.md).

## Community and support

- Website: https://www.rincoin.tech
- Development: https://github.com/rincoin-community
- Discord: https://discord.gg/XFDkSqeUPQ