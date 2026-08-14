# Security Policy

## Reporting a vulnerability

Email **security@rincoin.tech**. This address is for security reports only; it is not a general support channel.

Please do not open a public GitHub issue for anything that could put users, funds, nodes, or the Rincoin network at risk. A public issue is visible to an attacker at the same moment it becomes visible to us.

A useful report includes:

* the affected version or commit;
* what an attacker gains — funds, node availability, privacy, consensus divergence;
* steps to reproduce, or a proof of concept if you have one;
* any conditions the attack depends on, such as a specific chain, configuration, or peer topology.

A report that is incomplete is still worth sending. We would rather receive a rough description early than a polished one late.

You may encrypt your report to any of the keys listed below. If you would prefer to receive an encrypted reply, include your own public key.

## Keys

These are the keys used to sign releases and to receive encrypted reports. **Verify the fingerprint before you trust a key**, using more than one of the channels described under [Obtaining and verifying a key](#obtaining-and-verifying-a-key).

| Name | Role | Fingerprint |
|------|------|-------------|
| [@ysmreg](https://github.com/ysmreg) | Creator of Rincoin, Core Developer | (to be added) |
| [@takologi](https://github.com/takologi) | Maintainer — release signing | `FEE1 ACA5 2C65 FF3E BF31 818C B559 5E17 52BC 2A82` |
| [@mrr-hatt](https://github.com/mrr-hatt) | Core Developer | (to be added) |

The role names are the ones defined in [GOVERNANCE.md](GOVERNANCE.md#roles). They describe responsibilities within this repository. They do not confer authority over the Rincoin network, which is determined by the participants who choose what software to run — see [GOVERNANCE.md § Consensus changes](GOVERNANCE.md#consensus-changes).

Signatures made by @takologi's key come from the signing subkey `ABB2 DF8B 79E8 A4E7 6139 4732 B3FF 4116 5803 42CB`. GPG resolves the subkey automatically once the primary key above is imported; you do not need to import it separately. Verify the primary fingerprint, not the subkey.

### Obtaining and verifying a key

Each channel below serves the same key material. Import from whichever is available to you, then check the fingerprint against the table above.

From a public keyserver:

```sh
gpg --keyserver hkps://keys.openpgp.org --recv-keys FEE1ACA52C65FF3EBF31818CB5595E1752BC2A82
```

Via Web Key Directory, which fetches the key directly from the project domain rather than from a third-party keyserver:

```sh
gpg --auto-key-locate clear,wkd --locate-keys security@rincoin.tech
```

Then, in every case:

```sh
gpg --fingerprint FEE1ACA52C65FF3EBF31818CB5595E1752BC2A82
```

Different channels may serve byte-different copies of the same key, because a keyserver can accumulate signatures that another copy has not seen. **Compare fingerprints, not file hashes.** A matching fingerprint means the same key; a differing file digest does not mean a different key.

## Verifying a release

Release verification does not depend on trusting this document. Every release is published with a `SHA256SUMS.txt` covering all archives and a detached signature `SHA256SUMS.txt.asc`:

```sh
gpg --verify SHA256SUMS.txt.asc SHA256SUMS.txt
sha256sum -c SHA256SUMS.txt --ignore-missing
```

Release tags in this repository are signed with the same key, so `git verify-tag v1.1.0` checks the source independently of the published binaries.

If a signature does not verify, do not run the binary, and please report it to the address above.

## Supported versions

| Version | Status |
|---------|--------|
| 1.1.0 | Supported. Terminal release: it validates normally below block height 840,000 and stops there by design. |
| 1.0.x | Not supported. Upgrade to 1.1.0, which is a drop-in replacement. |
| `dev` branch | Not a release. Fixes land here, but it carries no support commitment. |

The halt at block height 840,000 in the 1.1.0 release is **intended behaviour, not a vulnerability**. It exists so that a node cannot silently follow consensus rules the project has not adopted. Reports that a 1.1.0 node stops at that height will be answered, but they are not security issues. The reasoning is set out in the [v1.1.0 release notes](https://github.com/rincoin-community/rincoin-core/releases/tag/v1.1.0).

Rincoin Community Core is a fork of Litecoin Core, which is a fork of Bitcoin Core. A vulnerability inherited from either upstream is in scope here if it affects Rincoin, but please also report it upstream, since their users are affected too and they are better placed to fix the shared code.

## How we handle a report

We acknowledge reports as promptly as we are able. This is a small volunteer project; we would rather set an honest expectation than a schedule we cannot keep.

We do not commit to a fixed disclosure deadline. Security-sensitive matters may be discussed privately while a vulnerability is being investigated and fixed, and the details are published once doing so no longer creates unnecessary risk. This mirrors [GOVERNANCE.md § Security exceptions](GOVERNANCE.md#security-exceptions).

In practice that means we will keep you informed of progress, agree the timing of publication with you rather than announcing it at you, and credit you in the release notes unless you ask us not to. If a flaw is being actively exploited, we will publish what users need in order to protect themselves without waiting for a fix.

There is no bug bounty. We have no funds for one, and we would rather say so than imply otherwise.

## Official channels

Announcements, releases, and security advisories for Rincoin Community Core are published through:

* **Core repository:** [github.com/rincoin-community/rincoin-core](https://github.com/rincoin-community/rincoin-core)
* **GitHub organization:** [github.com/rincoin-community](https://github.com/rincoin-community)
* **Website:** [www.rincoin.tech](https://www.rincoin.tech)
* **Discord:** [Rincoin Community Forge](https://discord.gg/XFDkSqeUPQ) — server owner `tk.lg` (@takologi)
* **Consensus review:** [github.com/rincoin-community/consensus-840k](https://github.com/rincoin-community/consensus-840k)

Releases published elsewhere, and keys other than those listed above, are not part of this project's release process. This is a statement about what we produce and sign, not a judgement about anyone else's work: if you are running a binary we did not sign, this policy does not describe how it was built or who maintains it, and we cannot answer for it.

### Independent verification

A document in a repository can be edited by whoever controls the repository, so it is weak evidence on its own. The repository this project develops and releases from is therefore also published in DNS, where it can be checked without trusting this file:

```sh
dig TXT rincoin.tech +short
```

Expected content:

```
v=rincoin-community1; core=https://github.com/rincoin-community/rincoin-core; org=https://github.com/rincoin-community; web=https://www.rincoin.tech
```

The two records should agree. If they do not, treat both as unverified and ask on the Discord server before trusting either.

## Non-security bugs

For ordinary bugs, use the [issue tracker](https://github.com/rincoin-community/rincoin-core/issues). For how to contribute a fix, see [CONTRIBUTING.md](CONTRIBUTING.md) and [BRANCHES.md](BRANCHES.md).
