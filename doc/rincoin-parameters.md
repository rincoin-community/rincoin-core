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
| Halving interval `I`     | `210,000` blocks (mainnet) |
| Target block spacing     | `60` seconds             |
| PoW target timespan      | `33` hours               |
| Difficulty algorithm     | legacy retarget until block `30,000`, Dark Gravity Wave (DGW) per-block thereafter (mainnet) |

### Subsidy schedule

Below height `4·I` the subsidy halves every `I` blocks (50, 25, 12.5, 6.25 RIN).
From height `4·I` — 840,000 on mainnet — the S6/b schedule applies
(`Consensus::Params::vS6bSubsidyPhases`, built by `SetS6bSchedule()` in
`src/chainparams.cpp`):

| From height (mainnet) | In intervals | Subsidy |
| --------------------- | ------------ | ------- |
| `840,000`             | `4·I`        | `4 RIN` |
| `2,100,000`           | `10·I`       | `2 RIN` |
| `4,200,000`           | `20·I`       | `1 RIN` |
| `6,300,000`           | `30·I`       | `0.6 RIN` |
| `234,587,500`         | terminal     | `0` |

Derivation of the terminal height: the ceiling is `800 RIN · I` (168,000,000 RIN on
mainnet, which equals `MAX_MONEY`); issuance before height `30·I` is
`(50 + 25 + 12.5 + 6.25)·I + 4·6·I + 2·10·I + 1·10·I = 147.75·I` RIN; the remaining
`652.25·I` RIN are paid at 0.6 RIN per block, so the terminal height is
`30·I + floor(652.25·I / 0.6)` = 234,587,500 on mainnet, where the division is exact
and the total is exactly 168,000,000 RIN. The normative vectors are in the
consensus-840k repository (`analysis/data/S6B_normative_test_vectors.csv`) and are
checked by `s6b_subsidy_tests`.

### The block at the transition height

The coinbase of the block at height `4·I` must claim exactly the subsidy plus the
block's fees (`bad-cb-amount-transition`). At every other height a coinbase may
claim less than the maximum. Checked by `s6b_transition_block_tests` and
`feature_s6b_subsidy.py`.

### Signature hash from the transition height

From height `4·I` every ECDSA signature (pre-SegWit and SegWit v0) is a
replay-protected one, in the form Bitcoin Cash introduced and Bitcoin Gold uses on a
SegWit chain:

| Item | Value |
| ---- | ----- |
| Flag in the hash type byte | `SIGHASH_FORKID` = `0x40`, mandatory (`Signature must use SIGHASH_FORKID`) |
| Fork ID (`Consensus::Params::sigForkId`, every network) | `840` = `0x000348` |
| Signature-hash algorithm | BIP143, for pre-SegWit inputs too (it commits to the amount of the input) |
| Hash type that ends the preimage (4 bytes, little-endian) | `hash type byte \| (fork ID << 8)` |
| ... for `SIGHASH_ALL` | `0x00034841`, bytes `41 48 03 00`; the signature ends in `0x41` |
| Flag and fork ID as one number, as some software is configured (e.g. `fork_id` in Komodo DeFi Framework's `coins`) | `0x00034840` |
| For comparison | Bitcoin Cash: fork ID `0`; Bitcoin Gold: fork ID `79` |

Defined in `src/consensus/sigforkid.h` and `SignatureHash()`
(`src/script/interpreter.cpp`). Only the requirement to set the flag is a consensus
rule; the other signature-encoding checks (`STRICTENC`) stay policy. Empty signatures
are allowed as before, Taproot and MWEB signatures are unchanged. Test vectors:
`src/test/data/s6b_sighash.json`, generated by `test/util/gen_s6b_sighash_vectors.py`
independently of the C++ code; the file also holds signatures of real Bitcoin Gold
transactions, which the same code accepts when it runs with fork ID 79.

## 3. Per-network parameters

All hashes are big-endian as displayed by `getblockhash`/`getbestblockhash`.

The test networks scale the mainnet schedule with their halving interval: a mainnet
height `h` becomes `floor(h · I / 210,000)`. This applies to the buried deployment
heights, the DGW start, the subsidy phases and the peer protocol floor. Versionbits
windows are not scaled, and a versionbits state only changes on a window boundary, so
the scaled start and timeout heights of a deployment are additionally rounded down to
a multiple of the network's window, with at least one window between them
(`SetScaledDeploymentHeights()`). The configured heights are then, as on mainnet, the
heights at which the state changes: STARTED at the start height, LOCKED_IN at the
timeout height at the latest (height-based deployments lock in at their timeout even
without signalling), ACTIVE one window later. With the current values the timeout of
every test-network deployment is exactly one window after its start, so the ACTIVE
heights in the tables hold whether or not blocks signal. Regtest is the exception
noted below.

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
| Transition height             | `840,000`; terminal height `234,587,500` |
| BIP34/65/66, CSV, SegWit      | `26,500` |
| DGW from                      | `30,000` |
| Versionbits window / threshold | `8,064` / `6,048` |
| Taproot (height-based)        | start `2,161,152`, timeout `2,370,816` |
| MWEB                          | never activated (the test networks derive theirs from Litecoin's `2,217,600` / `2,427,264`) |
| Last checkpoint               | `744,278` |
| Minimum chain work            | `0x…0790415d8f4bee`, the work of block `750,000` |
| Assumed-valid block           | `00000001115a0298260b3f6d0ed73a1174aefcc337aaea997c79ce7e7c3d683a` (`750,000`) |

### Testnet

| Parameter            | Value |
| -------------------- | ----- |
| Genesis block hash   | `00009d5fbc8579e8b4292f1bab22437d9468c0cc615cb5b0242d8159b31760ad` |
| Message start (magic)| `72 69 6E 74` ("rint") |
| Default P2P port     | `19555` |
| Base58 pubkey prefix | `65` → `T` |
| Bech32 HRP           | `trin` (MWEB: `trmweb`) |
| Subsidy halving interval | `2,100` (1/100 of mainnet) |
| Transition height    | `8,400`; phases from `21,000`, `42,000`, `63,000`; terminal height `2,345,875` (exact: 1,680,000 RIN) |
| BIP34/65/66, CSV, SegWit | `265` |
| DGW from             | `300` |
| Versionbits window / threshold | `2,016` / `1,512` |
| Taproot              | start `20,160`, timeout `22,176` (scaled `21,611` / `23,708`); ACTIVE at `24,192` |
| MWEB                 | start `22,176`, timeout `24,192` (scaled `22,176` / `24,272`); ACTIVE at `26,208` |

The genesis block and the message start are the ones testnet has always had. A
testnet chain built under the earlier parameters is not valid under these.

### Regtest

| Parameter            | Value |
| -------------------- | ----- |
| Genesis block hash   | `7d2c8c57ce2597f86c9fe41f9865ad664b04d2aad4321fdaab48ed3da1805fe7` |
| Message start (magic)| `72 72 63 74` ("rrct") |
| Default P2P port     | `29555` |
| Bech32 HRP           | `rrin` (MWEB: `rrmweb`) |
| Subsidy halving interval | `210` (1/1000 of mainnet) |
| Transition height    | `840`; phases from `2,100`, `4,200`, `6,300`; terminal height `234,587` (the last 0.3 RIN below the scaled ceiling of 168,000 RIN is never issued) |
| Versionbits window / threshold | `144` / `108` |
| MWEB                 | start `2,160`, timeout `2,304` (scaled `2,217` / `2,427`); ACTIVE at `2,448` |

Regtest keeps the upstream regtest conventions instead of scaling them: BIP34 at
`500`, BIP66 at `1,251`, BIP65 at `1,351`, CSV at `432`, SegWit from genesis,
Taproot always active, no difficulty adjustment. The inherited functional tests
depend on these. `-vbparams` can still move a deployment for a single test.

### Previewnet

A publicly reachable rehearsal chain with its own genesis block and network
identity: real proof of work as on testnet, with the compressed schedule and the
versionbits window of regtest. At the 60-second block spacing the transition height
is about 14 hours from the genesis block. A rehearsal that needs a fresh chain gets
a new genesis block in a new build, because a fresh chain on the same genesis block
would lose against any node that kept the earlier, longer one.

`fPowAllowMinDifficultyBlocks` is set on testnet and on the preview network but has
no effect on either: the difficulty is the proof-of-work limit until DGW takes over,
and DGW does not know the min-difficulty rule. Difficulty therefore behaves as on
mainnet, including the slow recovery after a large miner leaves.

| Parameter            | Value |
| -------------------- | ----- |
| Genesis block hash   | `00004282aaa888c5b7a1bb210464788510d3c5976a8cec49061a3eb49d04ff33` |
| Genesis merkle root  | `687b2f9d3bbd319b5d841fa72f2269657606a777d289f1b7f612cc8396de7843` |
| Genesis coinbase text / time / nonce | `RinCoin Genesis Block - RinHash Preview2` / `1789862400` / `104436` |
| Message start (magic)| `72 69 6E 70` ("rinp") |
| Default P2P port     | `49555` |
| Base58 pubkey prefix | `56` → `P` |
| Bech32 HRP           | `prin` (MWEB: `prmweb`) |
| Subsidy halving interval | `210` (1/1000 of mainnet) |
| Transition height    | `840`; phases from `2,100`, `4,200`, `6,300`; terminal height `234,587` |
| BIP34/65/66, CSV, SegWit | `26` |
| DGW from             | `30` |
| Versionbits window / threshold | `144` / `108` (as on regtest) |
| Taproot              | start `2,160`, timeout `2,304` (scaled `2,161` / `2,370`); ACTIVE at `2,448` |
| MWEB                 | start `2,160`, timeout `2,304` (scaled `2,217` / `2,427`); ACTIVE at `2,448` |

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

## 5. Peer-protocol-version floor and protocol version

`PROTOCOL_VERSION` is `70019` (Rincoin Community Core 1.2.0).

A per-network schedule of minimum peer protocol versions
(`Consensus::Params::vMinPeerProtoVersionFloors`) applies by chain height. Peers
below the floor in force are disconnected during the version handshake.

| Network  | From genesis | From the transition height |
| -------- | ------------ | -------------------------- |
| mainnet  | `70017`      | `70018` from `840,000` |
| testnet  | `70017`      | `70018` from `8,400` |
| regtest  | `70017`      | `70018` from `840` |
| preview  | `70017`      | `70018` from `840` |

See `rinhash_peer_proto_floor_params` in
[`src/test/rinhash_tests.cpp`](../src/test/rinhash_tests.cpp) and
`feature_min_peer_proto_floor.py`.

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
