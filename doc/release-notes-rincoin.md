# Rincoin Core — Release History

This is the consolidated release history for **Rincoin Core**. It complements,
rather than replaces, the upstream Litecoin/Bitcoin per-version notes archived
under [`doc/release-notes/`](release-notes/), which are kept for backward
compatibility and to ease adoption of upstream changes.

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
- **Local build helpers (developer tooling, not shipped):**
  `contrib/build-windows-local.ps1` (Docker + MinGW cross-build on Windows) and
  `contrib/build-linux-local.sh` (native, ccache-accelerated), plus
  `contrib/test-asan-local.ps1` / `.sh`, which reproduce the CI ASan/UBSan leg
  in Docker (with a fast single-suite mode) so sanitizer issues can be caught
  locally without commit/push. All are for local testing only and keep their
  outputs and caches out of the repository.
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
change mainnet consensus rules. Notable items:

- ARM64 (aarch64) Linux release targets and CI/release-engineering
  improvements; MinGW release optimization.
- A RinHash "activations table" (JSON → generated header → runtime resolver)
  and a peer-protocol-version floor (`70018`) scheduled per network.
- MWEB HogEx empty-`vin` handling fix.
- GPG-signed release tags.

> Note: the activations-table machinery introduced here has since been
> **reverted** in the current development line (see above); the peer-version
> floor is retained as plain constants.

---

## v1.0.5 — unit-test correctness & sync

- Fixed all unit tests that still referenced Litecoin (LTC) constants so they
  use Rincoin (RIN) parameters throughout.
- Header-synchronization optimization for faster initial headers download.
- DNS-seed logging improvements.

## v1.0.4 — maintenance

- Fixed MWEB file operations failing on Windows with non-ASCII data paths.
- DNS-seed updates and additional seeders.
- Qt: shift+click range selection in the coin-control dialog.
- Updated checkpoints and build tooling; assorted build fixes.

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
