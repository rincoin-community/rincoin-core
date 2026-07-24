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
- **Docs:** added [`doc/rincoin-parameters.md`](rincoin-parameters.md) and this
  consolidated history.

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
