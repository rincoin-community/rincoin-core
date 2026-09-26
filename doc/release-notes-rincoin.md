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

## 1.2.0 — in development (`v1.2.0-dev.2`)

> **Status: development build, not a release.** `v1.2.0-dev.2` is the label of the
> current development build of the 1.2.0 line. It exists so that the consensus change
> below can be reviewed and tested. It is not tagged, there are no release binaries,
> and it must not be used on mainnet: a pre-release build refuses to start on
> mainnet unless `RINCOIN_TESTING_ALLOW_MAINNET=1` is set in the environment, and
> it announces itself as `/RincoinCommunityCore:1.2.0(dev.2)/`. The stable 1.2.0
> release will carry neither the guard nor the label. (`dev.2` replaces the
> unpublished `dev.1`: transaction replay protection now has the `SIGHASH_FORKID`
> form described below.)

### Consensus change at block height 840,000

This line implements the height-840,000 transition specified in the
[consensus-840k repository](https://github.com/rincoin-community/consensus-840k)
(`technology/consensus-transition.md` and the S6/b specification). Blocks below
height 840,000 are validated exactly as before.

- **Subsidy schedule (S6/b).** From height 840,000 the block subsidy is 4 RIN, from
  2,100,000 it is 2 RIN, from 4,200,000 it is 1 RIN, from 6,300,000 it is 0.6 RIN,
  and from 234,587,500 it is zero. Total issuance is exactly 168,000,000 RIN, which
  is `MAX_MONEY`. The schedule is a list of `{height, subsidy}` phases in
  `Consensus::Params` (`vS6bSubsidyPhases`), built by `SetS6bSchedule()` from the
  network's halving interval.
- **The block at height 840,000 claims exactly subsidy plus fees.** Its coinbase
  outputs must sum to exactly 4 RIN plus the block's fees (`bad-cb-amount-transition`
  otherwise). This is the only height with such a rule; at every other height a
  coinbase may still claim less than it is entitled to. The rule makes this one
  block invalid under the previous schedule whatever its fees are, and a block made
  for the previous schedule invalid here, so the two rule sets separate at a defined
  block. **Pools and solo miners:** claim the full `coinbasevalue` of
  `getblocktemplate` at that height, as mining software normally does; a payout
  scheme that leaves part of the reward unclaimed would lose that block.
- **Replay protection in the signature hash (`SIGHASH_FORKID`, fork ID 840).** From
  height 840,000 every ECDSA signature, in pre-SegWit and SegWit v0 inputs alike, is
  a replay-protected one in the form that Bitcoin Cash introduced and Bitcoin Gold
  uses on a SegWit chain. Its hash type must have `SIGHASH_FORKID` (`0x40`) set;
  a signature without it makes script evaluation fail with `Signature must use
  SIGHASH_FORKID` in `OP_CHECKSIG`, `OP_CHECKSIGVERIFY`, `OP_CHECKMULTISIG` and
  `OP_CHECKMULTISIGVERIFY` (a hard failure, not a false result that a script could
  invert; empty signatures are allowed as before). The signature hash is the BIP143
  one, for pre-SegWit inputs too, so it commits to the amount of the input, and the
  four-byte hash type that ends the preimage carries the fork ID 840 (`0x000348`) in
  its upper three bytes. A `SIGHASH_ALL` signature therefore ends in `0x41` and its
  preimage in `41 48 03 00`; software that takes the flag and the fork ID as one
  number is configured with `0x00034840`. The rule is keyed to the height of the
  block that confirms the transaction. Only the requirement to set the flag is a
  consensus rule; the other signature-encoding checks stay policy. Taproot (BIP341)
  signatures and MWEB are unchanged. A transaction signed for one side of the
  transition is invalid on the other side, and on any chain that does not use the
  same fork ID. The historical `SIGHASH_SINGLE` quirk (a pre-SegWit signature over
  the constant digest `1` that fits any transaction) is out of reach from the
  transition height on, because BIP143 has no such digest.
- **MWEB is not activated on mainnet.** The deployment inherited from Litecoin would
  have activated MWEB by height at 2,427,264 even without signalling. In line with
  other implementations of this chain it is now set to never activate on mainnet:
  MWEB transactions stay non-standard and MWEB data in a block stays invalid, as they
  are today. In 2026 Litecoin had to fix a consensus flaw in its MWEB validation that
  allowed the MWEB balance to be broken on its mainnet (0.21.5.4 to 0.21.5.6, all
  included in this release); activating MWEB on Rincoin is left to a later release.
  Testnet, the preview network and regtest keep their MWEB deployment. As for any
  deployment that is never active, `getblockchaininfo` no longer lists `mweb` under
  `softforks` on mainnet; software that reads that entry should treat its absence as
  "not active".
- **Nothing in a block identifies the rule set.** There is no mandatory coinbase
  commitment, no required block or transaction version, no new service bit and no
  wire-format change.

### Mempool, wallet and RPC behavior around the transition

- The mempool applies the rule of the next block. A transaction signed for the other
  side of the transition is refused with `old-style-sig-fork-id` or
  `new-style-sig-fork-id` (classified as a recent consensus change, so the peer that
  relayed it is not penalized).
- When the last block below the transition height connects, transactions in the
  mempool that are signed the historical way are removed together with their
  descendants (and the other way round if a reorganization moves the tip back below
  it). Transactions without ECDSA signatures stay.
- The wallet, `signrawtransactionwithwallet`, `signrawtransactionwithkey`, the PSBT
  RPCs and the GUI sign for the block after the current tip, and check signatures
  made by other parties the same way, so multi-party signing
  (`combinerawtransaction`, `combinepsbt` with `finalizepsbt`, `analyzepsbt`) works on
  both sides of the transition. `rincoin-tx` has no chain state and takes
  `-signheight=<n>`.
- Decoded scripts name the new hash types (`[ALL|FORKID]` and so on). The
  `sighashtype` arguments of the signing RPCs and `rincoin-tx` accept these names
  from the transition height on, where the flag is added whether it is named or
  not; naming it for a block below the transition height is an error.
- From the transition height on, signing needs the amount of every spent output.
  The wallet and the node know it for coins they can see; for outputs described by
  the caller in `prevtxs` (`signrawtransactionwithkey`,
  `signrawtransactionwithwallet`, `rincoin-tx`) the `amount` field is now required
  for pre-SegWit outputs too (`Missing amount` otherwise).
- **Operational consequences.** A transaction that is still unconfirmed when block
  839,999 connects becomes invalid; abandon it (`abandontransaction`) and send it
  again. The same holds for transactions that were signed in advance and kept for
  later. External signers and other wallet software must produce the new signatures
  before they can spend after the transition; software that already supports Bitcoin
  Gold or Bitcoin Cash has the construction and needs the fork ID 840 and the height.

### Voluntary coinbase tag

`getblocktemplate` offers the tag `/RCC/` in `coinbaseaux.flags` (`052f5243432f`),
and the internal miner puts it into the coinbase scriptSig after the height and the
extra nonce. The tag only identifies the software that assembled a block. It is not
a consensus rule, and blocks without it, or with any other marker, are valid.

### Network protocol

- `PROTOCOL_VERSION` is 70019. The peer protocol floor is unchanged: 70017 from
  genesis, 70018 from height 840,000.
- The user agent is `/RincoinCommunityCore:1.2.0/` (with `(dev.2)` in development
  builds).

### Checkpoints and block assembly

Mainnet checkpoints now run to block `744,278`, 77 entries further than the `435,935` of
the 1.1 line, generated with `contrib/devtools/generate_checkpoints.py`. `nMinimumChainWork`
is still unset, so a node that starts from nothing has no work threshold below which it
refuses a chain; that value belongs to the release that ships for mainnet.

Block assembly now leaves out a mempool transaction signed for the other side of the
transition height instead of failing on it. The mempool is emptied of such transactions
when the tip crosses the height, so this is a second line of defence; without it a single
entry left behind would make every block template fail its validity check and stop block
production on that node until the entry expired.

### Test networks

Mainnet parameters other than the ones above are unchanged. The test networks now
scale the mainnet schedule with their halving interval (heights rounded down):
testnet uses an interval of 2,100 (1/100, transition at 8,400), regtest and the
preview network use 210 (1/1000, transition at 840). Deployment start and timeout
heights scale the same way and are then rounded down to a multiple of the network's
versionbits window, so that they fall on a period boundary as they do on mainnet (see
[`doc/rincoin-parameters.md`](rincoin-parameters.md)). Testnet keeps its genesis
block and message start; its earlier chain is not valid under the new parameters.
The preview network has a new genesis block and the versionbits window of regtest
(144 / 108), which gives it the same deployment heights as regtest. Regtest keeps the upstream regtest
conventions for the buried deployments, always-active Taproot and disabled
difficulty adjustment, so the inherited test suite stays meaningful.

### Synchronized with Litecoin Core 0.21.5.8

All upstream changes up to Litecoin Core 0.21.5.8 that apply to Rincoin have been
adopted (60 commits, each cherry-picked with a reference to its upstream commit).
They are mostly MWEB hardening: data-corruption and durability fixes in the MMR
files, stricter handling of mutated MWEB blocks, additional consensus and policy
checks for peg-ins, peg-outs and kernels, mempool and relay fixes, and rate limits
for light-client requests. Also included: the wallet directory fix for Boost 1.78
and later, MWEB view keys in `dumpwallet`, a lost-transaction-index fix, and a
32 MB limit for received P2P messages so that the largest valid MWEB block can be
relayed. Litecoin's network-specific activation heights for the new MWEB rules are
set to zero on Rincoin, where MWEB has never been active, so the rules apply from the
first MWEB block; its list of frozen mainnet MWEB outputs is empty here for the same
reason.

### Other changes

- The wallet refuses to pay to a witness version that the chain does not enforce
  yet (such outputs would be spendable by anyone until then). Adapted from
  Aevust/rincoin-sim (commits 4aba34a3d and dfe2f64ef).
- `HEADERSYNC-PERF` log lines are printed only with `-debug=bench`. Adapted from
  Rin-coin/rincoin (commit 2515fc659, by Aevust).

### Maintenance carried over from the 1.1 line

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
- **Peer-protocol-version floor is now a height schedule.** The single
  height/version floor is replaced by a sorted list of `{height, min_version}`
  entries (`Consensus::Params::vMinPeerProtoVersionFloors`) so the floor can be
  raised at successive heights as the protocol is bumped over time. Current
  schedule requires `70017` (MWEB-capable) from genesis and `70018` from each
  network's transition height. This is networking policy only and does
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
