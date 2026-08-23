#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Build labeled reference rincoind/rincoin-cli binaries for the fork
scenario test suite (test/functional/feature_fork_*.py).

This purpose-built script replaces test/get_previous_releases.py for fork
scenario testing -- that script is a vestigial, unmodified upstream Bitcoin
Core helper (hardcoded to Bitcoin release tarballs) and isn't usable for
Rincoin-specific refs.

Two kinds of reference are supported:

  --ref LABEL=GITREF
      A ref that already exists in *this* repository's history/remotes
      (e.g. a tag or branch such as v1.1.0, legacy-1.1, or v1.0.1 -- the
      latter fetched via the pre-existing `legacy` remote pointing at
      Rin-coin/rincoin). Built via a detached git worktree, so it never
      touches the caller's checked-out branch or working tree.

  --foreign LABEL=URL#BRANCH
      A ref from a repository that is NOT a remote of this repo and never
      should be (e.g. a competing/foreign implementation such as
      Aevust/rincoin). Cloned standalone into its own directory outside
      this repo's .git, entirely independent of this repo's remotes.

Each label's build is cached by resolved commit sha under
<out-dir>/<label>/COMMIT; a matching cache is reused without rebuilding.

Usage:
    test/build_reference_node.py \\
        --ref v1.1.0=v1.1.0 \\
        --ref v1.0.1=v1.0.1 \\
        --foreign aevust=https://github.com/Aevust/rincoin.git#feature/port-sim-v1.0.7 \\
        --out-dir releases

Resulting binaries land at <out-dir>/<label>/bin/{rincoind,rincoin-cli}.
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def run(cmd, cwd=None, env=None):
    print(f"+ {' '.join(str(c) for c in cmd)}  (cwd={cwd or os.getcwd()})", file=sys.stderr)
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def resolved_commit(cwd, ref="HEAD"):
    out = subprocess.run(["git", "rev-parse", ref], cwd=cwd, check=True,
                          capture_output=True, text=True)
    return out.stdout.strip()


def cache_hit(label_dir, commit):
    commit_file = label_dir / "COMMIT"
    binary = label_dir / "bin" / "rincoind"
    if not (commit_file.exists() and binary.exists()):
        return False
    return commit_file.read_text().strip() == commit


def build_tree(src_dir, jobs):
    """Run autogen/configure/make in src_dir. Returns True on success."""
    env = os.environ.copy()
    try:
        run(["./autogen.sh"], cwd=src_dir, env=env)
        # --with-incompatible-bdb: matches ci/test/00_setup_env_native_ci.sh's
        # BITCOIN_CONFIG -- this machine's system BDB isn't the 4.8 wallet
        # historically required, and older release trees (unlike current
        # dev/master) don't default to tolerating that.
        run(["./configure", "--without-gui", "--disable-tests", "--disable-bench", "--with-incompatible-bdb"],
            cwd=src_dir, env=env)
        run(["make", f"-j{jobs}"], cwd=src_dir, env=env)
        return True
    except subprocess.CalledProcessError as e:
        print(f"!! build failed in {src_dir}: {e}", file=sys.stderr)
        print("!! old release trees may need a pinned/older toolchain -- "
              "see plan Open Risk R1", file=sys.stderr)
        return False


def stage_binaries(src_dir, label_dir):
    bin_dir = label_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    for name in ("rincoind", "rincoin-cli"):
        src = src_dir / "src" / name
        if not src.exists():
            raise FileNotFoundError(f"expected binary not found after build: {src}")
        shutil.copy2(src, bin_dir / name)
        os.chmod(bin_dir / name, 0o755)


def build_local_ref(label, gitref, out_dir, jobs):
    """Build a ref that lives in this repo's own history/remotes via a
    detached git worktree -- never touches the caller's branch/working tree."""
    label_dir = out_dir / label
    worktree_dir = out_dir / f"{label}-worktree"

    commit = resolved_commit(REPO_ROOT, gitref)
    if cache_hit(label_dir, commit):
        print(f"[{label}] cache hit at {commit[:12]}, skipping build")
        return

    if worktree_dir.exists():
        run(["git", "worktree", "remove", "--force", str(worktree_dir)], cwd=REPO_ROOT)
    run(["git", "worktree", "add", "--detach", str(worktree_dir), gitref], cwd=REPO_ROOT)
    try:
        if not build_tree(worktree_dir, jobs):
            raise RuntimeError(f"build failed for ref {label}={gitref}")
        stage_binaries(worktree_dir, label_dir)
        label_dir.mkdir(parents=True, exist_ok=True)
        (label_dir / "COMMIT").write_text(commit + "\n")
        print(f"[{label}] built {commit[:12]} -> {label_dir / 'bin'}")
    finally:
        run(["git", "worktree", "remove", "--force", str(worktree_dir)], cwd=REPO_ROOT)


def build_foreign_ref(label, url, branch, out_dir, jobs):
    """Clone a fully separate, unrelated repository standalone -- never
    added as a remote of this repo."""
    label_dir = out_dir / label
    src_dir = out_dir / f"{label}-src"

    if src_dir.exists():
        run(["git", "fetch", "origin", branch], cwd=src_dir)
        run(["git", "checkout", f"origin/{branch}"], cwd=src_dir)
    else:
        src_dir.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--branch", branch, "--single-branch", url, str(src_dir)])

    commit = resolved_commit(src_dir)
    if cache_hit(label_dir, commit):
        print(f"[{label}] cache hit at {commit[:12]}, skipping build")
        return

    if not build_tree(src_dir, jobs):
        raise RuntimeError(f"build failed for foreign ref {label}={url}#{branch}")
    stage_binaries(src_dir, label_dir)
    label_dir.mkdir(parents=True, exist_ok=True)
    (label_dir / "COMMIT").write_text(commit + "\n")
    print(f"[{label}] built {commit[:12]} -> {label_dir / 'bin'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ref", action="append", default=[], metavar="LABEL=GITREF",
                         help="ref resolvable in this repo's own history/remotes")
    parser.add_argument("--foreign", action="append", default=[], metavar="LABEL=URL#BRANCH",
                         help="ref from a standalone, unrelated repository")
    parser.add_argument("--out-dir", default=str(REPO_ROOT / "releases"),
                         help="output directory for staged binaries (default: releases/)")
    parser.add_argument("--jobs", type=int, default=multiprocessing.cpu_count(),
                         help="parallel make jobs (default: nproc)")
    args = parser.parse_args()

    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    for spec in args.ref:
        label, _, gitref = spec.partition("=")
        if not label or not gitref:
            parser.error(f"--ref must be LABEL=GITREF, got: {spec}")
        build_local_ref(label, gitref, out_dir, args.jobs)

    for spec in args.foreign:
        label, _, rest = spec.partition("=")
        url, _, branch = rest.partition("#")
        if not label or not url or not branch:
            parser.error(f"--foreign must be LABEL=URL#BRANCH, got: {spec}")
        build_foreign_ref(label, url, branch, out_dir, args.jobs)


if __name__ == "__main__":
    main()
