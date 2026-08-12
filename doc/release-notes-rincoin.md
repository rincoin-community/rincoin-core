# Rincoin Core — Release History

> **Status: active — this is the canonical Rincoin release history.** The
> separate detailed narrative that used to sit in the repository root has been
> folded into this file and removed, so this is now the single source of truth.

This is the consolidated release history for **Rincoin Core**. It complements,
rather than replaces, the upstream per-version notes archived under
[`doc/litecoin-release-notes/`](litecoin-release-notes/) and
[`doc/bitcoin-release-notes/`](bitcoin-release-notes/), which are kept for
historical reference and to ease adoption of upstream changes.

Network and consensus constants referenced below are documented, with their
derivations, in [`doc/rincoin-parameters.md`](rincoin-parameters.md).

Version scheme: `v[GENERATION].[MAJOR].[MINOR]`. Rincoin Core `v1.0.0`
corresponds to the Litecoin `v0.21.4` base.

---

## Unreleased — current development

No consensus rules change in this line; it is maintenance and infrastructure
work only. Highlights so far:

- **Reverted the v1.1.0-rc1 RinHash "activations table."** The JSON-driven,
  code-generated consensus table has been removed. RinHash Argon2d parameters
  are hard-coded again (`t=2, m=64, lanes=1, salt="RinCoinSalt"`); the PoW
  output is unchanged. The per-network peer-protocol-version floor is retained
  as plain `Consensus::Params` constants (see
  [`doc/rincoin-parameters.md`](rincoin-parameters.md) §5).
- **Small correctness fixes:** add missing `<stdexcept>` include; use
  `CHECK_NONFATAL` instead of `assert` for the MWEB HogEx `vout` invariant so a
  construction-time violation cannot abort the node.
- **Network identity:** the internal IPv6 prefix is now derived from
  `SHA256("rincoin")` (`FD 2D DD 82 F5 C8`) instead of the inherited
  Litecoin-derived value, with matching `net`/`netbase` test vectors.
- **Regtest block spacing corrected to match mainnet.** The regtest
  `nPowTargetSpacing` was `60 * 50` (3000 s), an outlier introduced during the
  fork setup; upstream convention (Bitcoin, Litecoin) is for regtest to use the
  same spacing as mainnet. It is now `60` s, matching Rincoin mainnet. This is a
  regtest-only change (mainnet/testnet consensus is unaffected; regtest already
  disables retargeting and DGW). It also corrects a time-derived edge case where
  the equivalent-proof-of-work age of a moderately deep stale block exceeded the
  30-day stale-relay limit, which had prevented serving BIP157 compact-filter
  checkpoints for stale blocks on regtest.
- **Test-network parameter clean-up (testnet, previewnet, regtest).** Mainnet is
  unchanged. On the non-canonical test networks:
  - Fixed `defaultAssumeValid` on testnet and previewnet, which previously
    pointed at the *mainnet* genesis hash (a copy-paste error); they now point at
    their own genesis. Harmless but correct.
  - Regtest `fRequireStandard` is now `false`, matching Bitcoin/Litecoin regtest
    (non-standard transactions are relayed by default).
  - Realigned the private-key (WIF) version bytes to the `PUBKEY_ADDRESS + 128`
    convention: **testnet `SECRET_KEY` 209 → 193**, **previewnet 219 → 184**.

    > ⚠️ **Compatibility (testnet/previewnet only, WIF export format):** this
    > changes the version byte of exported *private keys* (`dumpprivkey`) on
    > testnet and previewnet — old WIF strings (previous `8…` prefix) will no
    > longer import into a node built after this change. **Addresses are
    > unchanged** (`PUBKEY_ADDRESS` is still 65/56, i.e. `T…`/`P…`), and
    > `wallet.dat` stores raw keys, so **existing wallets keep working** without
    > action. Only if you copied a raw WIF *text* string out of an old
    > testnet/previewnet node do you need to re-run `dumpprivkey` after
    > upgrading. Mainnet WIF (`7…`, byte 188) is unaffected. There is no
    > canonical testnet/previewnet yet, so real-world impact is expected to be
    > nil.
- **Taproot and MWEB now activate early on the test networks (testnet,
  previewnet).** These deployments inherited Litecoin-scale activation heights
  (~2.2M blocks) that, on Rincoin's faster chain, would not be reached on the
  test networks for years — leaving both features effectively untestable there.
  They now activate at low heights (testnet: after SegWit at 8064; previewnet:
  after a lowered SegWit at 864), so the networks can exercise them end to end.
  Previewnet additionally gets a shorter BIP9 window and lower BIP34/CSV/SegWit
  heights so a fresh preview chain reaches every upgrade quickly.

  > This is purely maintenance to make the test networks usable for validation.
  > **Mainnet is not touched:** its Taproot/MWEB activation is already defined in
  > the code and is unchanged by this. Activating early on the test networks does
  > not advance, defer, or pre-empt the mainnet schedule in any way. Whether
  > mainnet activation should ultimately be brought forward, deferred, or left at
  > its already-defined height is a separate decision for the community, and that
  > discussion is treated as such.
- **Peer-protocol-version floor is now a height schedule.** The single
  height/version floor is replaced by a sorted list of `{height, min_version}`
  entries (`Consensus::Params::vMinPeerProtoVersionFloors`) so the floor can be
  raised at successive heights as the protocol is bumped over time. Current
  schedule requires `70017` (MWEB-capable) from genesis and `70018` (RinHash) at
  each network's existing floor height. This is networking policy only and does
  not affect block validity; effective behaviour is unchanged for the peers on
  the network today (which already advertise `70017`+).
- **Block-download timeout floored at Litecoin's 150 s spacing.** The P2P
  block-download timeout scales with `nPowTargetSpacing` (a peer is dropped if a
  requested block stays in flight for ~one block interval). Rincoin's 60 s
  spacing shrank that window to ~60 s — far tighter than Litecoin's ~150 s —
  even though the wall-clock cost of downloading/validating a block does not
  depend on block cadence. Under load this spuriously disconnected honest-but-slow
  peers and could dead-lock a heavy regtest sync (both downloaders dropping their
  only block source). The effective interval used for this timeout is now floored
  at Litecoin's 150 s (`BLOCK_DOWNLOAD_TIMEOUT_MIN_SPACING`, `net_processing.cpp`).
  Networking robustness only — no effect on consensus, block validity, or chain
  state; chains whose spacing already meets/exceeds 150 s are unaffected.
- **Continuous integration:** a GitHub Actions workflow runs the upstream
  container-based CI harness with two legs — a plain unit+functional build and
  an ASan/UBSan build. The gate is headless (core unit tests + functional
  suite); Qt GUI test vectors are a separate follow-up. Each run publishes a
  downloadable `test-results-<leg>` artifact.
- **Test suite made green:** rebranded the `bitcoin-util` test fixtures to
  `rincoin-tx` with Rincoin addresses, dropped the stale Litecoin smoke
  benchmark from `make check`, and added UBSan suppressions for the intentional
  wrapping arithmetic in the crypto primitives.
- **Fixed a signed-integer overflow in the MWEB fee calculation** (`CFeeRate`):
  `mweb_weight * BASE_MWEB_FEE` now saturates instead of overflowing on
  pathological weights. Caught by UBSan; the result is unchanged for any valid
  transaction (real MWEB weights are far below the saturation bound).
- **Fixed a missing `cs_main` lock in `CChainState::InitCoinsDB`.** The MWEB
  coins-view initialization reads the best block and coins DB, which require
  `cs_main`; the lock was not taken, so `DEBUG_LOCKORDER` (sanitizer) builds
  aborted during test setup. Behavior is unchanged in release builds.
- **Fixed a data race in the peer-protocol-version floor check.** The floor
  read the active-chain tip (`::ChainActive().Tip()/.Height()`) in the `version`
  message handler without holding `cs_main`, even though the active chain is
  guarded by `cs_main` everywhere else (the same handler already takes the lock
  a few lines later). Without it there was no happens-before with block
  connection on the validation thread, so a peer connecting at the exact
  per-network activation height could be evaluated against a stale tip and, on
  some thread schedules, not be disconnected. The tip is now read under a short
  `cs_main` lock. Impact is limited to peer-reachability policy at/after the
  activation height; there is no effect on consensus, block validity, the UTXO
  set, or funds. The race was introduced with the floor in `v1.1.0` and
  surfaced as an intermittent functional-test hang under the CI scheduler.
- **Further sanitizer fixes:** held `cs_wallet` in the MWEB stealth-address
  unit test (matching production callers, so `DEBUG_LOCKORDER` no longer
  aborts), and suppressed the intentional wrapping in Boost's `hash_combine`
  used by libmw aggregation.
- **Local build & test helpers (developer tooling, not shipped):**
  `contrib/build-windows-local.ps1` (Docker + MinGW cross-build on Windows) and
  `contrib/build-linux-local.sh` (native, ccache-accelerated) for builds; plus a
  local CI-parity harness that reproduces the GitHub CI legs in Docker without
  commit/push. `contrib/ci-local-runner.sh` is the single container-side
  entrypoint (self-documented via its header) driven by env vars —
  `LEG=asan|plain`, `MODE=check|suite:<name>|func:<spec>`, `JOBS_ARG`,
  `LOAD_HOGS` — over the shared image `contrib/ci-local.Dockerfile`; the host
  wrappers `contrib/test-asan-local.ps1` / `.sh` only build the image, manage the
  ccache/build volumes, and `docker run` that entrypoint. This gives both the
  ASan/UBSan leg and a fast plain gcc leg for unit-suite or functional-test
  iteration. All are for local testing only and keep their outputs and caches out
  of the repository.
- **Docs:** added [`doc/rincoin-parameters.md`](rincoin-parameters.md) and this
  consolidated history.

> **Acknowledgement.** Some of the above test-suite adaptations overlap with
> work in the parallel Aevust fork, which reached a number of them first; a few
> we arrived at independently before noticing theirs. Credit to the Aevust
> contributors for the test-fixture and benchmark clean-ups. This applies to
> **test and tooling changes only** — consensus rules are decided and
> implemented independently by this project.

No public version number is assigned to this development line yet.

---

## v1.1.0-rc1 — community maintenance (release candidate)

A community-maintenance and infrastructure release candidate. It did **not**
change mainnet consensus rules.

> The RinHash "activations table" (JSON → generated header → runtime resolver)
> shipped in this release candidate and has since been **reverted** in the
> current development line, together with its `getrinhashparams` RPC, the
> `rinhash` object it added to `getblockchaininfo`, and its code-generation
> guard. RinHash parameters are hard-coded again and the peer-protocol-version
> floor is retained as plain `Consensus::Params` constants. The detail below
> describes what survives.

### Peer-protocol-version floor

A per-network minimum peer protocol version becomes effective at a set height.
From that height forward, peers advertising a lower `nVersion` are disconnected
during the version handshake (`net_processing.cpp`, VERSION handler).

| Network | Floor height | Floor |
|---------|--------------|-------|
| mainnet | 840000       | 70018 |
| testnet | 4200         | 70018 |
| regtest | 600          | 70018 |
| preview | 600          | 70018 |

Below the floor height the schedule's baseline (`70017`) applies.

### `PROTOCOL_VERSION` bumped to 70018

`src/version.h` advertises `PROTOCOL_VERSION = 70018`. v1.0.x peers (`70017`)
remain interoperable everywhere except at and above the per-network floor
height, where they are disconnected during the version handshake.

### Previewnet (`preview` chain)

A fourth chain dedicated to rehearsal mining and integration drills:

- `-preview` command-line flag, `[preview]` config section.
- P2P magic `rinp` (`0x72 0x69 0x6E 0x70`); ports `49555` (P2P), `49556` (RPC).
- Bech32 HRPs `prin` / `prmweb`; base58 prefixes `56` (PUBKEY), `118` (SCRIPT),
  `219` (SECRET); ext-key prefixes `0x03E25D80` / `0x03E25946`.
- Reuses testnet's genesis verbatim, which simplifies sync and tooling.

### MWEB HogEx empty-`vin` fix

`src/consensus/tx_check.cpp` exempts HogEx transactions from the "transaction
has no inputs" check, and `src/mweb/mweb_miner.cpp` asserts the HogEx
structural invariant before block assembly so a malformed HogEx fails fast
rather than producing an invalid block. This restores the ability to mine MWEB
blocks containing a HogEx aggregator transaction with an empty `vin`, which is
its specified shape.

### Block-download timeout floor (Litecoin parity)

The P2P block-download timeout scales with the block interval
(`nPowTargetSpacing`): a peer is disconnected if a requested block stays in
flight for roughly one block interval. Rincoin's 60-second mainnet spacing made
that window only ~60s — far tighter than the ~150s Litecoin runs with and the
~600s of Bitcoin — even though the wall-clock cost of downloading and
validating a block is independent of how often blocks are produced. Under load
this spuriously disconnected honest-but-slow peers, and on regtest it could
deadlock a heavy sync when both downloaders dropped their only block source.
The effective interval is now floored at Litecoin's 150-second spacing
(`BLOCK_DOWNLOAD_TIMEOUT_MIN_SPACING`, `net_processing.cpp`). This is a
networking-robustness change only: it does not affect consensus, block
validity, or mainnet chain state, and chains whose spacing already meets or
exceeds 150s are unaffected.

### Other items

- ARM64 (aarch64) Linux release targets and CI/release-engineering
  improvements; MinGW release optimization.
- Qt splash and logo assets restored to their v1.0.1 form, reverting an
  unintended asset change that landed in the v1.0.4 line.
- GPG-signed release tags.

### Testing

- Unit tests (`src/test/rinhash_tests.cpp`): canonical PoW vector, mainnet
  header vectors, and the per-network peer-protocol floor schedule.
- Functional test (`test/functional/feature_min_peer_proto_floor.py`): regtest
  end to end, covering acceptance below the floor height and disconnection at
  and above it.

### Upgrade notes

Standard upgrade for wallet users: replace binaries and restart. No datadir
migration and no reindex are required. Miners, pools, explorers and exchanges
should upgrade before mainnet block `840000`, after which peers still running
v1.0.x cannot maintain connections to v1.1.0 peers.

---

## v1.0.5 — unit-test correctness & sync

- Fixed all unit tests that still referenced Litecoin (LTC) constants so they
  use Rincoin (RIN) parameters throughout.
- Header-synchronization optimization for faster initial headers download.
- DNS-seed logging improvements.

## v1.0.4 — maintenance

Released 4 February 2026, on top of v1.0.1.

### Argon2d SIMD acceleration, and the fixes it needed

Runtime-dispatched SIMD implementations of the Argon2d stage of RinHash
(`src/crypto/argon2/argon2_dispatch.c`): CPUID detection selects AVX512, AVX2,
SSSE3 or the reference implementation, so one binary adapts to the host and no
recompilation is needed. `configure.ac` applies `-mssse3` / `-mavx2` /
`-mavx512f` only to their respective modules, so a CPU lacking a feature never
executes its instructions.

The first cut of the AVX2, SSSE3 and AVX512 paths **computed wrong hashes**.
Each used a 4-argument BLAMKA round macro that shuffled only within a single
register, where the PHC reference shuffles between registers. They were
replaced with the correct 8-argument `BLAKE2_ROUND_1_*` / `BLAKE2_ROUND_2_*`
forms (`SWAP_HALVES` for rows, `SWAP_QUARTERS` / `UNSWAP_QUARTERS` for
columns). Without those fixes, SIMD-optimised builds produced incorrect
Argon2d output.

### Other changes

- Fixed MWEB file operations failing on Windows with non-ASCII data paths.
- DNS-seed updates and additional seeders; checkpoints updated through block
  435,935, with a checkpoint-generation tool added.
- Qt: shift+click range selection in the coin-control dialog, and wallet
  performance improvements in the sync and payment dialogs.
- Release build tooling for Linux (x86_64 and aarch64) and Windows, plus
  toolchain-compatibility fixes for Ubuntu 24.04, MinGW-w64 and Python 3.12.
- Developer tools: a genesis-block miner and a base58 prefix test utility.
- Sequential block-file read hints (`posix_fadvise(POSIX_FADV_SEQUENTIAL)`)
  to reduce I/O latency during initial block download.
- New application icons.

## v1.0.2 / v1.0.3 — RinHash v2 (rolled back)

These releases introduced **RinHash v2**. It was **not adopted by the network**
and was subsequently **rolled back**; the RinHash v1 proof-of-work remains in
force. These versions are listed here only for historical completeness.

## v1.0.1 — maintenance

- Version bump and minor fixes over v1.0.0 (icons, chainparams touch-ups,
  max-supply information).

## v1.0.0 (= Litecoin v0.21.4) — Rincoin base

The initial Rincoin Core baseline, forked from Litecoin `v0.21.4`, introducing
the RinHash proof-of-work and Rincoin network identity. It carried the upstream
security backports present in Litecoin `v0.21.4`, including:

- **CVE-2024-35202** — remote DoS via `blocktxn` message handling (backported
  from Bitcoin Core).
- **Mutated-blocks propagation** fix (backported from Bitcoin Core).
- **miniupnp** infinite-loop / OOM fix (backported).
- Default `-peerblockfilters`/`-blockfilterindex` to off when pruning is
  enabled, plus functional-test fixes.

---

## Credits

Thanks to everyone who contributed, including the upstream
[Bitcoin Core](https://github.com/bitcoin/bitcoin/) and
[Litecoin](https://github.com/litecoin-project/litecoin) developers whose work
this builds on.
