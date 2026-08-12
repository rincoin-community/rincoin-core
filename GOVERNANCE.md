# Rincoin Community Core Governance

Rincoin Community Core is developed through **Rincoin Community Forge**, the engineering and governance structure of Rincoin Community.

This document describes how the Core repository and its development process are governed. It does not claim ownership or administrative control over the decentralized Rincoin network.

## Principles

Rincoin Community Forge follows a small set of practical principles:

- **Open participation.** Anyone may propose changes, review code, test releases, operate infrastructure, or otherwise contribute to Rincoin. Contributions are considered on their merits rather than on affiliation.
- **Technical merit.** Decisions should be based primarily on technical reasoning, security, compatibility, and long-term maintainability.
- **Peer review.** Important changes should receive review proportionate to their risk and impact.
- **Transparency.** Development and ordinary project decisions should take place publicly whenever practical.
- **Network decentralization.** Repository maintainers publish software; they do not unilaterally determine network consensus.

## Roles

### Contributors

Anyone contributing code, documentation, testing, review, research, infrastructure work, or technical discussion is a contributor.

No permission is required to participate.

### Reviewers

Anyone may review a proposed change. Review from contributors with relevant experience or demonstrated knowledge naturally carries greater weight on specialized or high-risk changes.

### Maintainers

Maintainers are trusted contributors with responsibility for repository access, merging reviewed changes, coordinating releases, and keeping the development process functional.

Maintainer status is based on sustained contribution, technical competence, judgement, and trust. Repository access is a practical responsibility, not ownership of the project or network.

## Decision making

For ordinary changes, maintainers determine whether a pull request has received sufficient review and testing to be merged.

For larger or controversial changes, maintainers should seek broader technical discussion and rough consensus among active contributors before proceeding.

A maintainer may decline or postpone a change because of technical risk, insufficient review, unclear benefit, maintenance cost, or incompatibility with project direction.

## Consensus changes

Changes to Rincoin consensus rules require a higher standard than ordinary software changes.

Such changes should normally include:

1. a clear public proposal and rationale;
2. discussion of compatibility, security, and economic consequences;
3. implementation and independent review;
4. appropriate testing;
5. communication with affected network participants before activation.

Merging or releasing code does not by itself establish network consensus. Miners, node operators, exchanges, users, and other participants ultimately decide which software and consensus rules they run and accept.

No repository, maintainer, Discord server, or other off-chain group can by itself determine consensus for the Rincoin network.

## Security exceptions

Security-sensitive matters may be discussed privately while a vulnerability is being investigated or fixed. Relevant information should be disclosed publicly when doing so no longer creates unnecessary risk.

See [SECURITY.md](SECURITY.md).

## Evolving this document

This governance model is intentionally lightweight.

As Rincoin Community Forge grows, its processes may become more formal. Changes to this document should be proposed publicly and reviewed like other significant project changes.
