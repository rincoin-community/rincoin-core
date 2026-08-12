# Rincoin Community Core Branches

This document describes the long-lived branches used by Rincoin Community Core.

Branch names describe development lines. They should not be treated as release identifiers: **published release tags and release notes are the reference for released software**.

## Quick answer

If you want to develop for Rincoin: fork the repository, branch from **`dev`**, and open a pull request against **`dev`**.

The exception is a fix to the currently released version, which targets `master` — and which must then be merged back into `dev`, so the fix is not lost from the next release.

## `dev`

`dev` is the default branch and the integration point for ordinary development.

Reviewed work lands here: features, maintenance, testing, build and infrastructure changes, upstream integrations, and documentation. The tip of `dev` is expected to build and pass tests, but it is **not** a release. Its content may change frequently, and it should not be treated as production software merely because it builds.

Contributors should base new work on `dev` unless an issue or pull request specifies another target.

## `master`

`master` is the production line. It tracks the software the project has released.

It is updated from `dev` when a release is prepared, and release tags are created on it. Between releases it changes only for fixes to the released version.

Because the currently shipping release is the terminal `legacy-1.1` build (see below), `master` presently tracks the line from which the **next** production release will be built rather than the binary running today. That exception ends with the first non-terminal release.

For deployed nodes, prefer a published release from the [releases page](https://github.com/rincoin-community/rincoin-core/releases) over building any branch.

## `legacy-1.1`

`legacy-1.1` preserves the Rincoin **1.0/1.1 lineage** for compatibility and essential maintenance.

This legacy line is intentionally configured to stop operating at block height **840,000**, preventing it from continuing past the point where the next Rincoin consensus rules must take effect. It is not intended to operate beyond that height.

It is a leaf: it is tagged and released from, but never merged into `dev` or `master`. The changes that make it stop are exactly the changes a successor release must not inherit.

It is not a general development branch. Changes should normally be limited to critical bug fixes and necessary security fixes for the legacy line.

## Development lines and topic branches

Work on a larger, named development line uses a branch under its area, for example `consensus/<codename>` for a consensus workstream. Such a branch is created from `dev` and merged back into `dev` when the work is agreed.

Smaller work uses ordinary topic branches named `<area>/<slug>`, also based on `dev`.

The existence of a branch does not imply that its work has been accepted, scheduled for release, or adopted as project policy. A change becomes part of Rincoin Community Core through the review and integration process described in [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`GOVERNANCE.md`](GOVERNANCE.md).

### Consensus at height 840,000

A public review of the consensus decision at block height **840,000** is in progress. Analysis, candidate specifications, supporting material, and discussion are maintained separately in the [`consensus-840k`](https://github.com/rincoin-community/consensus-840k) repository.

Consensus-sensitive changes should follow that process in addition to the normal development and review workflow, and are developed on their own branch until the decision is made.

## Summary

| Branch | Purpose | Base new work on it? |
| ------ | ------- | -------------------- |
| `dev` | integration; default branch | **yes** |
| `master` | production line; release tags | only fixes to the released version |
| `legacy-1.1` | terminal 1.0/1.1 lineage, stops at height 840,000 | only critical and security fixes |
| `consensus/<codename>` | a named consensus workstream | by arrangement |
| `<area>/<slug>` | topic branches | they are the normal unit of work |

## Choosing a branch

For **users and node operators**, use published releases unless a development or testing purpose requires otherwise.

For **contributors**, target the branch appropriate to the work:

* ordinary development, including features, fixes, tests, and documentation → `dev`;
* a fix to the currently released version → `master`, then merged back into `dev`;
* critical bug fixes or necessary security fixes specific to the 1.0/1.1 lineage → `legacy-1.1`;
* consensus-sensitive work → the relevant `consensus/<codename>` branch, following the process above.
