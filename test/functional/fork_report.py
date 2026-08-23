#!/usr/bin/env python3
# Copyright (c) 2026 The Rincoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Run the fork scenario test suite (feature_fork_*.py) and emit a
structured pass/fail report as CI artifacts.

Each feature_fork_*.py test is a standalone script (BitcoinTestFramework
subclass); this runner invokes each one as its own subprocess -- the same
process-per-test model test_runner.py uses -- and classifies the result by
its exit code (0 = passed, 77 = skipped, anything else = failed, matching
TEST_EXIT_PASSED/SKIPPED/FAILED in test_framework/test_framework.py).

The current build's binary is picked up the normal way (BUILDDIR in
config.ini, or the BITCOIND/BITCOINCLI env vars); reference binaries for
old/foreign nodes are resolved from releases/<label>/ (see
test/build_reference_node.py) and their resolved commit recorded in the
report for traceability.

Usage:
    test/functional/fork_report.py --out-json fork-test-report.json --out-md fork-test-report.md
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
FUNCTIONAL_DIR = REPO_ROOT / "test" / "functional"

DEFAULT_TESTS = [
    "feature_fork_commitment.py",
    "feature_fork_subsidy.py",
    "feature_fork_sig_fork_id.py",
    "feature_fork_reorg.py",
    "feature_fork_vs_legacy.py",
    "feature_fork_vs_aevust.py",
]

SCENARIO_BY_TEST = {
    "feature_fork_commitment.py": "1: one node",
    "feature_fork_subsidy.py": "1: one node",
    "feature_fork_sig_fork_id.py": "1: one node",
    "feature_fork_reorg.py": "2: two new nodes",
    "feature_fork_vs_legacy.py": "3: old+new node",
    "feature_fork_vs_aevust.py": "4: new+foreign node",
}

STATUS_BY_CODE = {0: "pass", 77: "skip"}


def git_rev(cwd):
    try:
        out = subprocess.run(["git", "rev-parse", "HEAD"], cwd=cwd, check=True,
                              capture_output=True, text=True)
        return out.stdout.strip()
    except Exception:
        return None


def reference_commit(label):
    commit_file = REPO_ROOT / "releases" / label / "COMMIT"
    if commit_file.exists():
        return commit_file.read_text().strip()
    return None


def run_one(test_name, extra_args):
    test_path = FUNCTIONAL_DIR / test_name
    start = time.time()
    proc = subprocess.run(
        [sys.executable, str(test_path)] + extra_args,
        cwd=FUNCTIONAL_DIR,
        capture_output=True,
        text=True,
    )
    duration = time.time() - start
    status = STATUS_BY_CODE.get(proc.returncode, "fail")
    return {
        "test": test_name,
        "scenario": SCENARIO_BY_TEST.get(test_name, "unknown"),
        "status": status,
        "exit_code": proc.returncode,
        "duration_seconds": round(duration, 1),
        "output_tail": "\n".join((proc.stdout + proc.stderr).splitlines()[-40:]),
    }


def write_markdown(path, results, binary_refs):
    lines = ["# Fork scenario test report", ""]
    lines.append("## Binaries")
    for label, commit in binary_refs.items():
        lines.append(f"- **{label}**: `{commit or 'unresolved'}`")
    lines.append("")
    lines.append("## Results")
    lines.append("")
    lines.append("| Test | Scenario | Status | Duration (s) |")
    lines.append("|---|---|---|---|")
    for r in results:
        glyph = {"pass": "PASS", "skip": "SKIP", "fail": "FAIL"}[r["status"]]
        lines.append(f"| {r['test']} | {r['scenario']} | {glyph} | {r['duration_seconds']} |")
    lines.append("")
    failed = [r for r in results if r["status"] == "fail"]
    if failed:
        lines.append("## Failure output (last 40 lines each)")
        for r in failed:
            lines.append(f"### {r['test']}")
            lines.append("```")
            lines.append(r["output_tail"])
            lines.append("```")
    path.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tests", nargs="*", default=DEFAULT_TESTS,
                         help="feature_fork_*.py test filenames to run (default: all)")
    parser.add_argument("--out-json", default="fork-test-report.json")
    parser.add_argument("--out-md", default="fork-test-report.md")
    parser.add_argument("test_args", nargs=argparse.REMAINDER,
                         help="extra args forwarded to each test script, e.g. -- --nocleanup")
    args = parser.parse_args()

    extra_args = args.test_args
    if extra_args and extra_args[0] == "--":
        extra_args = extra_args[1:]

    binary_refs = {"current_tree": git_rev(REPO_ROOT)}
    for label in ("v1.1.0", "v1.0.1", "aevust"):
        binary_refs[label] = reference_commit(label)

    results = []
    any_failed = False
    for test_name in args.tests:
        print(f"[fork_report] running {test_name} ...", file=sys.stderr)
        result = run_one(test_name, extra_args)
        results.append(result)
        print(f"[fork_report] {test_name}: {result['status']} ({result['duration_seconds']}s)", file=sys.stderr)
        if result["status"] == "fail":
            any_failed = True

    report = {"binaries": binary_refs, "results": results}
    Path(args.out_json).write_text(json.dumps(report, indent=2) + "\n")
    write_markdown(Path(args.out_md), results, binary_refs)

    print(f"[fork_report] wrote {args.out_json} and {args.out_md}", file=sys.stderr)
    sys.exit(1 if any_failed else 0)


if __name__ == "__main__":
    main()
