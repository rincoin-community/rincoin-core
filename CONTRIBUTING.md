# Contributing to Rincoin Community Core

Rincoin Community Core uses an open contributor model. Anyone who wants to contribute constructively to the development of Rincoin is welcome.

Development is coordinated through **Rincoin Community Forge**. See [GOVERNANCE.md](GOVERNANCE.md) for the project's development and decision-making structure.

## Ways to contribute

Contributions include more than code. Useful work includes:

- patches and bug fixes;
- code review;
- testing and reproducible bug reports;
- documentation;
- build and release engineering;
- mining, networking, and infrastructure testing;
- operating and maintaining reliable Rincoin Core nodes and network infrastructure;
- research and technical proposals.

Reviewing and testing existing changes is particularly valuable.

## Before you start

For small fixes, documentation changes, and straightforward improvements, opening a pull request directly is fine.

For substantial changes, new features, protocol changes, or anything affecting consensus, please open an issue or start a technical discussion before investing heavily in an implementation.

Security vulnerabilities should be reported privately according to [SECURITY.md](SECURITY.md).

Build and developer documentation is available under [`doc/`](doc/). The branch
layout is described in [BRANCHES.md](BRANCHES.md).

## Contributor workflow

The usual workflow is:

1. Fork the repository.
2. Create a focused topic branch.
3. Make and test your changes.
4. Commit changes in logical, reviewable units.
5. Open a pull request against `dev`.

Most work belongs on `dev`. Fixes to the currently released version target
`master` instead — see [BRANCHES.md](BRANCHES.md), which describes each
long-lived branch and what belongs on it.

Pull requests should explain:

- **what** is being changed;
- **why** the change is useful or necessary;
- **how** it was tested.

Keep pull requests focused. Avoid mixing functional changes with unrelated formatting, code movement, or cleanup.

Changes should follow the existing coding conventions and should not break the test suite. New tests are expected where they are practical and useful.

## Review and merging

Anyone may participate in review. Maintainer responsibilities, decision-making, and the additional requirements for consensus-sensitive changes are described in [GOVERNANCE.md](GOVERNANCE.md).

## Communication

Code changes, bugs, and review should normally be discussed in GitHub issues and pull requests so that the technical history remains public and searchable.

Broader discussion and contributor coordination also take place on the Rincoin Community Discord:

https://discord.gg/XFDkSqeUPQ

Important technical conclusions reached elsewhere should be reflected back into the relevant GitHub issue or pull request.

## Upstream work

Rincoin Core descends from Litecoin Core and Bitcoin Core. Relevant upstream fixes and improvements are welcome, but they must be reviewed for compatibility with Rincoin-specific consensus, networking, wallet, and mining changes.

Please preserve upstream authorship and attribution when porting existing work.

## License

By contributing to Rincoin Community Core, you agree that your contribution may be distributed under the project's MIT license.
