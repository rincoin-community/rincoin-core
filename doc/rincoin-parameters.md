# Rincoin Network & Consensus Parameters (Test Reference)

This document is the single, authoritative reference for the Rincoin-specific
constants that the code and the test suites depend on. It exists for
**transparency**: every value below is either taken directly from
`src/chainparams.cpp` or derived from first principles (a documented formula or
the genesis block), so that maintainers, auditors, and external users can
verify it **independently** rather than trusting a test to compare a value with
itself.

> When a test hard-codes a Rincoin-specific value (a genesis hash, an address
> prefix, the internal IPv6 prefix, …), the expected value must be the one
> documented here and derivable from the chain or a stated formula — not simply
> whatever the current build happens to output.

Source of truth: [`src/chainparams.cpp`](../src/chainparams.cpp) is authoritative
for all network parameters. Values here are for reference and review.

---

## 1. Proof of Work — RinHash

RinHash is the Rincoin PoW function, applied to the 80-byte block header:

```
SHA3-256( Argon2d( BLAKE3(block_header) ) ) < target
```

Argon2d parameters (see [`src/crypto/rinhash.cpp`](../src/crypto/rinhash.cpp)):

| Parameter | Value      | Notes                          |
| --------- | ---------- | ------------------------------ |
| `t_cost`  | `2`        | iterations                     |
| `m_cost`  | `64`       | memory, in KiB                 |
| `lanes`   | `1`        | parallelism                    |
| `salt`    | `RinCoinSalt` | ASCII                       |

Canonical PoW test vector: see `rinhash_canonical_pow_vector` in
[`src/test/rinhash_tests.cpp`](../src/test/rinhash_tests.cpp).

## 2. Block subsidy & timing

| Parameter                | Value                    |
| ------------------------ | ------------------------ |
| Initial block subsidy    | `50 RIN`                 |
| Halving interval         | `210,000` blocks         |
| Target block spacing     | `60` seconds             |
| PoW target timespan      | `33` hours               |
| Difficulty algorithm     | legacy retarget until block `30,000`, Dark Gravity Wave (DGW) per-block thereafter (mainnet) |

> The mainnet emission schedule is the standard halving-by-`210,000` schedule.
> No customized-halving consensus change is in force. Any future change to the
> emission schedule will be documented here and shipped as an explicit,
> community-reviewed consensus change.

## 3. Per-network parameters

All hashes are big-endian as displayed by `getblockhash`/`getbestblockhash`.

### Mainnet

| Parameter                     | Value |
| ----------------------------- | ----- |
| Genesis block hash            | `000096bdd6e4613ca89b074ebd6f609aba6fe3f868b34ee79380aa3bc7a8c9db` |
| Genesis merkle root / coinbase txid | `8590c08530d2ed422b726a938f07df8f380671569e04dcb556dcb9601c47cdad` |
| Message start (magic)         | `52 49 4E 43` ("RINC") |
| Default P2P port              | `9555` |
| Default RPC port              | `9556` |
| Base58 pubkey prefix          | `60` → addresses start with `R` |
| Base58 script prefix          | `122` → `r` |
| Base58 secret-key prefix      | `188` |
| Bech32 HRP                    | `rin` (MWEB: `rinmweb`) |
| Subsidy halving interval      | `210,000` |

### Testnet

| Parameter            | Value |
| -------------------- | ----- |
| Genesis block hash   | `00009d5fbc8579e8b4292f1bab22437d9468c0cc615cb5b0242d8159b31760ad` |
| Message start (magic)| `72 69 6E 74` ("rint") |
| Default P2P port     | `19555` |
| Base58 pubkey prefix | `65` → `T` |
| Bech32 HRP           | `trin` (MWEB: `trmweb`) |

### Regtest

| Parameter            | Value |
| -------------------- | ----- |
| Genesis block hash   | `7d2c8c57ce2597f86c9fe41f9865ad664b04d2aad4321fdaab48ed3da1805fe7` |
| Message start (magic)| `72 72 63 74` ("rrct") |
| Default P2P port     | `29555` |
| Bech32 HRP           | `rrin` (MWEB: `rrmweb`) |
| Subsidy halving interval | `150` |

### Previewnet

A publicly reachable rehearsal chain that reuses testnet's genesis but has its
own network identity.

| Parameter            | Value |
| -------------------- | ----- |
| Message start (magic)| `72 69 6E 70` ("rinp") |
| Default P2P port     | `49555` |
| Base58 pubkey prefix | `56` → `P` |
| Bech32 HRP           | `prin` (MWEB: `prmweb`) |

## 4. Internal IPv6 prefix (ADDRv1)

Non-IP peers (Tor/I2P/CJDNS/internal) embedded in ADDRv1 use a chain-specific
6-byte prefix. Rincoin derives it the same way Bitcoin/Litecoin do, from the
coin name:

```
INTERNAL_IN_IPV6_PREFIX = 0xFD || SHA256("rincoin")[0:5]
                        = FD 2D DD 82 F5 C8
```

Independently verifiable, e.g.:

```sh
printf 'rincoin' | sha256sum
# 2ddd82f5c8...  → prefix = FD 2D DD 82 F5 C8
```

Defined in [`src/netaddress.h`](../src/netaddress.h); exercised by
`cnetaddr_serialize_v1/v2`, `cnetaddr_unserialize_v2` in
[`src/test/net_tests.cpp`](../src/test/net_tests.cpp) and
`netbase_lookupnumeric` in
[`src/test/netbase_tests.cpp`](../src/test/netbase_tests.cpp).

## 5. Peer-protocol-version floor

A per-network minimum peer protocol version, effective from a given height.
Below the floor, peers are disconnected during the version handshake. This is a
networking policy and does not affect block validity.

This release introduces **no protocol bump**, so every network carries a flat
single-entry schedule:

| Network  | Schedule (height → `min_peer_protocol_version`) |
| -------- | ----------------------------------------------- |
| mainnet  | `{0 → 70017}` |
| testnet  | `{0 → 70017}` |
| regtest  | `{0 → 70017}` |
| preview  | `{0 → 70017}` |

`PROTOCOL_VERSION` is also `70017` ([`src/version.h`](../src/version.h)), the
MWEB-capable baseline the network already runs. `MIN_PEER_PROTO_VERSION` is
`70017` as well, so the hard obsolete-version cutoff and the floor coincide and
the floor's own rejection path is currently unreachable. The schedule mechanism
(`Consensus::Params::vMinPeerProtoVersionFloors` /
`MinPeerProtoVersionFloorAt()`) is retained for a future release that raises
either value. See `rinhash_peer_proto_floor_params` in
[`src/test/rinhash_tests.cpp`](../src/test/rinhash_tests.cpp) and
[`test/functional/feature_min_peer_proto_floor.py`](../test/functional/feature_min_peer_proto_floor.py).

## 6. Terminal height

**This release stops at a height.** It is a terminal build for the current
lineage: it validates normally up to the last block below the terminal height
and then shuts down rather than connect a block at it.

| Network  | `nTerminalHeight` | `nTerminalWarningLead` |
| -------- | ----------------- | ---------------------- |
| mainnet  | `840,000`         | `43,200` (30 days at 60 s spacing) |
| testnet  | `0` (disabled)    | `43,200` |
| regtest  | `0` (disabled)    | `10` |
| preview  | `0` (disabled)    | `100` |

Derived values on mainnet: the last block this release will ever connect is
`839,999`, and the persistent warning begins at `840,000 − 43,200 = 796,800`.

This is **not an activation height and adds no consensus rule.** Below it the
node is byte-for-byte the same validator as a build without the halt — proven by
[`test/functional/feature_terminal_neutrality.py`](../test/functional/feature_terminal_neutrality.py),
which runs an armed and an unarmed node over the same chain and requires
identical tips, identical UTXO-set hashes and identical `getblockchaininfo`.
A block at the terminal height is left in the block index as an ordinary
unconnected candidate: it is not marked invalid, the peer that sent it is not
punished, and the chain left behind is directly resumable by a successor release.

On test chains the height and lead can be set with `-terminalheight` and
`-terminalwarninglead`. On mainnet both options are **refused**: the terminal
height is compiled in and no runtime option can move, disable, or re-enable it.
See [`src/test/terminal_tests.cpp`](../src/test/terminal_tests.cpp).

---

## How to obtain / re-derive these values

- **Genesis hashes / merkle roots**: start a node on the target network and run
  `rincoin-cli getblockhash 0` and `getblock <hash>`; or read the `assert(...)`
  lines in `CMainParams` / `CTestNetParams` / `CRegTestParams` in
  `src/chainparams.cpp`.
- **Coinbase txid of genesis** (used by `RPCNestedTests`): for a single-tx
  block the coinbase txid equals the block's merkle root (mainnet:
  `8590c085…c47cdad`).
- **Internal IPv6 prefix**: `printf 'rincoin' | sha256sum` (see §4).
- **Address prefixes / HRPs / ports / magic**: read directly from
  `src/chainparams.cpp`.

Keeping these in one reviewable place is deliberate: it lets a reader confirm
that a test's expected value matches an externally-derivable fact, which is
especially important once consensus-affecting changes are introduced.
