# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working in this repository.

## What this is

Rincoin Core — the full-node implementation of Rincoin (RIN). It is a fork of
Litecoin Core 0.21.x (which is itself a Bitcoin Core fork), so the entire
Bitcoin/Litecoin source layout, build system, and test framework apply. What
makes it Rincoin lives in a fairly small, well-identified set of deltas
(see "Rincoin-specific deltas" below); everything else should be read as
upstream code and changed with upstream conventions.

Key properties: RinHash PoW (BLAKE3 → Argon2d → SHA3-256), 60 s target
spacing, 50 RIN initial subsidy halving every 210,000 blocks, DGW difficulty
after height 30,000, MWEB inherited from Litecoin, `RINC` network magic,
P2P 9555 / RPC 9556, base58 `R` addresses, bech32 HRP `rrin`.

## Repository topology

Four remotes, and knowing which is which matters when reading history:

| Remote | Points at | Role |
|---|---|---|
| `origin` | `rincoin-community/rincoin-core` | Current home; community-maintained. The repository was recently moved here. |
| `takologi` | `takologi/rincoin` | The fork this repo's recent history came from. |
| `legacy` | `Rin-coin/rincoin` | Original upstream maintainer's repo; stale. |
| `litecoin` | `litecoin-project/litecoin` | Original upstream; useful for `git diff`/blame against unmodified Litecoin. |

Branches (see [BRANCHES.md](BRANCHES.md), which is the public statement of this):

- `dev` — integration branch and the repository default; new work goes there.
- `master` — production line; updated from `dev` at each release, and release
  tags are cut on it.
- `legacy-1.1` — **you are here, and this is a leaf that is never merged into
  anything.** The terminal v1.1.0 release: it validates normally up to block
  839,999 and then shuts down rather than connect a block at 840,000, because
  the height-840,000 consensus rules have not been selected yet. Its defining
  commits are exactly what a successor release must *not* inherit, which is why
  they live on this branch alone. Changes here should be limited to critical
  bug fixes and security fixes for the 1.0/1.1 line.
- `consensus/<codename>` — a named consensus workstream, cut from `dev`.

CI runs on `dev`, `master`, `legacy-1.1`, `consensus/**`, and all PRs.

## Build

Standard autotools. Berkeley DB 4.8 is required for legacy wallet support.

```bash
./autogen.sh
./configure            # add --with-incompatible-bdb or use contrib/install_db4.sh
make -j"$(nproc)"
make check             # unit tests (Boost) + util tests
```

Deliverables are `rincoind`, `rincoin-cli`, `rincoin-tx`, `rincoin-wallet`,
`rincoin-qt`. Note the asymmetry: *binary* and *man page* names are `rincoin*`,
but source files, libraries, and macros are still upstream-named
(`bitcoin-cli.cpp`, `libbitcoin_server.a`, `BITCOIN_CONF_FILENAME`). Do not
"fix" those names as a drive-by; the rename was deliberately limited to
user-visible deliverables. The conf file is `rincoin.conf`, datadir `~/.rincoin`.

Release binaries (Docker, multi-target Linux/Windows): see
[doc/build-rincoin-release.md](doc/build-rincoin-release.md) and
[contrib/build_release.sh](contrib/build_release.sh).

## Testing

Unit tests: `make check`, or run one suite directly:

```bash
src/test/test_rincoin --run_test=rinhash_tests
```

Functional tests:

```bash
test/functional/test_runner.py                 # everything
test/functional/test_runner.py feature_block.py
```

**CI does not run the whole functional suite.** It runs only the allowlist in
[test/functional/ci_passing_tests.txt](test/functional/ci_passing_tests.txt),
consumed by [ci/test/06_script_b.sh](ci/test/06_script_b.sh). A test is added
there only after it has been verified green on a Rincoin build. When you fix a
functional test, adding its name to that file is part of the change.

### Local CI parity (the important one)

Reproducing the GitHub Actions legs locally in Docker avoids commit/push/wait
cycles:

```bash
contrib/test-asan-local.sh                                   # full asan+ubsan 'make check'
contrib/test-asan-local.sh --suite scriptpubkeyman_tests     # single unit suite
contrib/test-asan-local.sh --functional feature_min_peer_proto_floor.py
contrib/test-asan-local.sh --load-hogs 4                     # CPU-starve, to reproduce races
```

[contrib/ci-local-runner.sh](contrib/ci-local-runner.sh) is the **single source
of truth** for container-side build/test logic; `test-asan-local.sh` and
`test-asan-local.ps1` are thin host wrappers that only build the image, manage
cache volumes, and `docker run` the runner. Never re-implement build or config
logic in a wrapper — add it to the runner so every host stays in lockstep.
Build objects and ccache live in Docker named volumes; nothing is written into
the repo.

### Test-value convention (non-obvious, and enforced by review)

[doc/rincoin-parameters.md](doc/rincoin-parameters.md) is the authoritative
reference for Rincoin-specific constants. When a test hard-codes a
Rincoin-specific value (genesis hash, address prefix, subsidy, magic bytes,
internal IPv6 prefix, …), the expected value must be the one documented there
and independently derivable from the chain or a stated formula — **not simply
whatever the current build happens to output.** Regenerating a vector from the
code under test defeats the point of the test. If you change a parameter,
update that doc in the same change.

## Rincoin-specific deltas

Everything else is upstream Litecoin/Bitcoin. When hunting for "where is the
Rincoin behaviour", start here:

- **PoW** — [src/crypto/rinhash.cpp](src/crypto/rinhash.cpp) (Argon2d params
  `t=2, m=64 KiB, lanes=1`, salt `RinCoinSalt`), called from
  `CBlockHeader::GetPoWHash()` in [src/primitives/block.cpp](src/primitives/block.cpp).
  Argon2 and BLAKE3 vendored under [src/crypto/argon2/](src/crypto/argon2/) and
  [src/crypto/blake3/](src/crypto/blake3/).
- **Difficulty** — `DarkGravityWave()` in [src/pow.cpp](src/pow.cpp), gated by
  `consensus.DGWHeight`; legacy retarget below it.
- **Chain parameters** — [src/chainparams.cpp](src/chainparams.cpp) for all four
  networks (main/test/regtest/signet). Regtest uses 60 s spacing to match
  mainnet, which differs from upstream.
- **Peer protocol floor** — `vMinPeerProtoVersionFloors` +
  `MinPeerProtoVersionFloorAt()` in
  [src/consensus/params.h](src/consensus/params.h), enforced during the version
  handshake in [src/net_processing.cpp](src/net_processing.cpp). It is a
  height-scheduled *networking policy* (disconnect peers below the floor); it
  does not affect block validity. Mainnet schedule: `{0, 70017}, {840000, 70018}`.
  Read the active tip under `cs_main` here — a race there has already been fixed
  once.
- **Client/protocol identity** — `CLIENT_NAME` in
  [src/clientversion.cpp](src/clientversion.cpp), `PROTOCOL_VERSION` in
  [src/version.h](src/version.h), version numbers in
  [configure.ac](configure.ac).
- **Header-sync performance** — parallel `GetHash()`/`GetPoWHash()` precompute
  for header batches in [src/validation.cpp](src/validation.cpp) /
  [src/validation.h](src/validation.h). RinHash is memory-hard, so header
  validation cost is nothing like Bitcoin's; keep that in mind before adding
  per-header work.
- **Test framework** — [test/functional/test_framework/rinhash.py](test/functional/test_framework/rinhash.py)
  (block-id computation) and `rin_util.py`; `rin_replacebyfee.py`.
- **DNS-seed tester** — [src/test-dns-seeds.cpp](src/test-dns-seeds.cpp),
  [DNS_SEED_TESTER.md](DNS_SEED_TESTER.md), built via
  [Makefile.test-dns-seeds](Makefile.test-dns-seeds).

## Conventions

- Follow [doc/developer-notes.md](doc/developer-notes.md) (upstream Bitcoin
  style) — it applies unchanged.
- Small, reviewable commits with their tests, subject prefixed by area:
  `net:`, `test:`, `consensus:`, `doc:`, `ci:`, `build:`, `policy:`,
  `contrib:`. Doc-only commits use a `[skip ci]` suffix.
- Release notes: [doc/release-notes-rincoin.md](doc/release-notes-rincoin.md) is
  the single Rincoin release history. Upstream notes under
  `doc/bitcoin-release-notes/` and `doc/litecoin-release-notes/` are historical
  reference only and are not Rincoin's.
- Line endings: `.gitattributes` forces LF for shell, Python, autotools, and
  sanitizer-suppression files. Windows checkouts otherwise break the CI runners.
- `tmp/` is deliberately gitignored local scratch space. Never commit anything
  from it and never reference its contents from tracked files.
