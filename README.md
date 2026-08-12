<p align="center">
  <img src="doc/assets/rincoin.png" alt="Rincoin" width="520">
</p>

<h1 align="center">Rincoin Community Core</h1>

<p align="center">
  <strong>Full-node and wallet software for the Rincoin network.</strong>
</p>

<p align="center">
  <a href="https://www.rincoin.tech"><img align="top" src="doc/assets/globe.svg" width="20" height="20">&nbsp;Website</a>
  &nbsp;&nbsp;·&nbsp;&nbsp;
  <a href="https://github.com/rincoin-community"><img align="top" src="doc/assets/github.svg" width="20" height="20"/>&nbsp;GitHub</a>
  &nbsp;&nbsp;·&nbsp;&nbsp;
  <a href="https://discord.gg/XFDkSqeUPQ"><img align="top" src="doc/assets/discord.svg" width="20" height="20"/>&nbsp;Discord</a>

</p>

---

## Rincoin Core

Rincoin Community Core is the continuing development of the Rincoin Core codebase originally published at [`Rin-Coin/rincoin`](https://github.com/Rin-Coin/rincoin).

This repository develops and maintains software for the **existing Rincoin blockchain and network**. It is not a new cryptocurrency, a chain fork, a token migration, or a relaunch.

Rincoin is developed openly through **Rincoin Community Forge**, the engineering and governance structure through which **Rincoin Community** coordinates Core development, technical review, testing, releases, infrastructure, and technical proposals.

Open participation does not mean unstructured development. Changes are expected to undergo review and testing appropriate to their impact, and consensus-sensitive changes require explicit technical scrutiny. Rincoin Community Forge develops and publishes software and proposals; adoption of consensus changes remains a decision of network participants.

## Current consensus review

*Rincoin is currently conducting a public review of the consensus decision at block height **840,000**.*

*Analysis, candidate specifications, supporting data, and the public discussion are available in the [`consensus-840k`](https://github.com/rincoin-community/consensus-840k) repository.*

*No scenario has been adopted at this stage. The objective is open technical review and, where possible, a common consensus that preserves a single Rincoin chain.*

## Project lineage

Rincoin was created by **Ysmreg** and originally published through [`Rin-Coin/rincoin`](https://github.com/Rin-Coin/rincoin). Rincoin Community recognizes Ysmreg's authorship and foundational contribution to the project.

Rincoin's codebase descends from Litecoin Core and Bitcoin Core. The Rincoin-specific codebase and its public development history established in `Rin-Coin/rincoin` form the foundation on which Rincoin Community Core continues development for the deployed Rincoin network.

Current development, review, and release work for Rincoin Community Core takes place in the [`rincoin-community`](https://github.com/rincoin-community) GitHub organization through Rincoin Community Forge.

## Project channels

For development, releases, announcements, and support relating to Rincoin Community Core, use the project resources below:

* **Website:** [www.rincoin.tech](https://www.rincoin.tech)
* **GitHub organization:** [github.com/rincoin-community](https://github.com/rincoin-community)
* **Core repository:** [github.com/rincoin-community/rincoin-core](https://github.com/rincoin-community/rincoin-core)
* **Discord:** [discord.gg/XFDkSqeUPQ](https://discord.gg/XFDkSqeUPQ)

> [!IMPORTANT]
> The GitHub organization [`github.com/rincoin-core`](https://github.com/rincoin-core) is independently operated. It is not part of Rincoin Community, Rincoin Community Forge, or the development, governance, and release process of Rincoin Community Core. Software, releases, statements, or support information published there should not be assumed to represent this project.

> [!NOTE]
> A separately operated Discord server uses the name **"Rincoin Official."** Rincoin Community does not recognize that server as the sole, exclusive, or official representative of Rincoin or the Rincoin network. It has no official role in Rincoin Community, Rincoin Community Forge, or the development, governance, and release process of Rincoin Community Core. The word **"Official"** in its name should not be interpreted as granting such status. For project announcements, development discussion, release information, and support relating to this repository, use the channels listed above.

## Technical overview

Rincoin is a UTXO-based Proof-of-Work cryptocurrency using the RinHash mining algorithm.

| Parameter                | Mainnet                                                                 |
| ------------------------ | ----------------------------------------------------------------------- |
| Coin / ticker            | Rincoin (`RIN`)                                                         |
| Consensus                | Proof of Work                                                           |
| Proof-of-Work algorithm  | RinHash                                                                 |
| Hash pipeline            | BLAKE3 → Argon2d → SHA3-256                                             |
| Argon2d parameters       | `t=2`, `m=64 KiB`, `lanes=1`, salt `RinCoinSalt`                        |
| Target block time        | 60 seconds                                                              |
| Initial block subsidy    | 50 RIN                                                                  |
| Subsidy halving interval | 210,000 blocks                                                          |
| Difficulty adjustment    | Legacy retarget to block 29,999; Dark Gravity Wave v3 from block 30,000 |
| P2P port                 | `9555`                                                                  |
| RPC port                 | `9556`                                                                  |
| Network magic            | `52 49 4E 43` (`RINC`)                                                  |
| Base58 pubkey prefix     | `R...`                                                                  |
| Bech32 HRP               | `rin`                                                                   |

The authoritative source for consensus and network parameters is the source code. The maintained reference table, including testnet, regtest, previewnet, genesis values, address prefixes, and derivations, is in [`doc/rincoin-parameters.md`](doc/rincoin-parameters.md).

### RinHash

RinHash applies three stages to the block header:

1. **BLAKE3** — initial hashing;
2. **Argon2d** — memory-hard stage;
3. **SHA3-256** — final 256-bit hash.

A valid block satisfies:

```text
SHA3-256(Argon2d(BLAKE3(block_header))) < target
```

The memory-hard stage is intended to increase the cost of specialized mining hardware. It should not be interpreted as a guarantee of permanent ASIC resistance; practical specialization resistance depends on parameters, implementations, hardware economics, and future engineering.

See `GetPoWHash()` and [`src/crypto/rinhash.cpp`](src/crypto/rinhash.cpp) for the implementation.

## Building Rincoin Core

Build instructions are maintained in the repository:

* [Linux / Unix build notes](doc/build-unix.md)
* [Windows build notes](doc/build-windows.md)
* [macOS build notes](doc/build-osx.md)
* [Release build guide](doc/build-rincoin-release.md)

Developers should build from a reviewed branch or release appropriate to their use case. Consensus-sensitive deployments should not assume that arbitrary development commits are production releases.

## Development

Rincoin Community Forge uses an open contributor workflow based on issues, pull requests, peer review, testing, and maintainer review.

* [Contributing guidelines](CONTRIBUTING.md) — how to submit work
* [Branches](BRANCHES.md) — which branch to target
* [Governance](GOVERNANCE.md) — roles, review, and how decisions are made
* [Developer notes](doc/developer-notes.md)
* [Issues](https://github.com/rincoin-community/rincoin-core/issues)
* [Pull requests](https://github.com/rincoin-community/rincoin-core/pulls)

New work is based on and submitted against the `dev` branch. See
[`BRANCHES.md`](BRANCHES.md) for the branch each kind of change belongs on.

Contributions are welcome from developers, reviewers, testers, miners, node operators, researchers, and integrators. Review and testing are first-class contributions, particularly for networking, wallet, mining, and consensus-critical changes.

> [!NOTE]
> Rincoin Community and Rincoin Community Forge are open to cooperation with any individual, team, or project that wants to contribute constructively to the development of Rincoin, regardless of affiliation.

## Releases and history

The consolidated Rincoin release history is maintained in:

* [`doc/release-notes-rincoin.md`](doc/release-notes-rincoin.md)

Historical Bitcoin Core and Litecoin Core release notes remain in the repository as upstream reference material. They are not Rincoin release notes.

## Security

Security-sensitive issues should not be disclosed publicly before maintainers have had a reasonable opportunity to assess and address them.

See [`SECURITY.md`](SECURITY.md) for the current vulnerability-reporting procedure.

## License

Rincoin Core is distributed under the MIT software license. See [`COPYING`](COPYING).

This codebase contains work originating in Bitcoin Core, Litecoin Core, Rincoin, and subsequent contributors. Copyright notices and attribution in individual source files remain applicable.


## Rincoin Community
[![Discord Banner 2](https://discord.com/api/guilds/1496277840398651485/widget.png?style=banner2)](https://discord.gg/XFDkSqeUPQ)