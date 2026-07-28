<#
.SYNOPSIS
    Run the CI ASan/UBSan build + tests LOCALLY in Docker, reproducing the
    GitHub Actions "asan+ubsan" leg so you can iterate without commit/push/wait.

.DESCRIPTION
    Thin wrapper: it only builds the shared image (contrib/ci-local.Dockerfile),
    manages the cache volumes, and runs the shared container-side logic
    (contrib/ci-local-runner.sh) with LEG=asan. All build/config/test logic
    lives in that one script, so this wrapper and its .sh sibling can never
    drift. If it passes here it should pass the asan+ubsan CI leg.

    Requires only Docker Desktop. The build tree, object files and ccache live
    in Docker named volumes, so the first run is slow (~15-25 min) and later
    runs are incremental and fast. Nothing is written into the repo or git.

.PARAMETER Suite
    Run only one Boost unit-test suite from the main test binary (fast path),
    e.g. -Suite scriptpubkeyman_tests. Omit to run the full 'make check'.

.PARAMETER Functional
    Run functional test(s) instead of unit tests. Space-separated bare names,
    or ';'-separated entries when an entry needs its own flags, e.g.
    -Functional "wallet_dump.py --legacy-wallet;p2p_addr.py".

.PARAMETER Check
    Force a full 'make check' even if -Suite is given (the default otherwise).

.PARAMETER Jobs
    Parallel build/test jobs. Defaults to the container's CPU count.

.PARAMETER LoadHogs
    Spawn N `yes` CPU hogs during the test phase to reproduce scheduler-
    starvation races under load (0 = none, the default).

.PARAMETER Clean
    Remove the cached build + ccache volumes, then rebuild from scratch.

.EXAMPLE
    contrib\test-asan-local.ps1 -Suite scriptpubkeyman_tests   # fast iterate
    contrib\test-asan-local.ps1                                 # full make check
    contrib\test-asan-local.ps1 -Functional feature_min_peer_proto_floor.py
    contrib\test-asan-local.ps1 -Clean                          # scratch rebuild
#>
[CmdletBinding()]
param(
    [string]$Suite,
    [string]$Functional,
    [switch]$Check,
    [int]$Jobs = 0,
    [int]$LoadHogs = 0,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$RepoRoot  = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Image     = 'rincoin-local:asan'
$BuildVol  = 'rincoin_local_build_asan'   # source copy + objects + configure
$CcacheVol = 'rincoin_local_ccache_asan'  # ccache

function Info($m) { Write-Host $m -ForegroundColor Cyan }
function Fail($m) { Write-Host "ERROR: $m" -ForegroundColor Red; exit 1 }

# Invoke `docker` with $ErrorActionPreference temporarily relaxed. Under Windows
# PowerShell 5.1 + $ErrorActionPreference='Stop', a native tool that writes to
# stderr (e.g. `docker build` BuildKit progress, or `docker run` test output on
# stderr) raises a terminating NativeCommandError before we can inspect the exit
# code. Running docker under 'Continue' lets its output stream through; callers
# check $LASTEXITCODE explicitly.
function Invoke-Docker {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & docker @args } finally { $ErrorActionPreference = $prev }
}

# --- 1. Prerequisites ------------------------------------------------------
Info '== Checking Docker =='
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Fail 'Docker not found. Install Docker Desktop and start it.'
}
Invoke-Docker info *> $null
if ($LASTEXITCODE -ne 0) { Fail 'Docker daemon not responding. Start Docker Desktop and try again.' }

# --- 2. Clean --------------------------------------------------------------
if ($Clean) {
    Info '== Removing cached build/ccache volumes =='
    Invoke-Docker volume rm $BuildVol $CcacheVol *> $null
}

# --- 3. Build the shared image (single source of truth) --------------------
Info '== Preparing local image (cached after first run) =='
Invoke-Docker build -t $Image -f (Join-Path $PSScriptRoot 'ci-local.Dockerfile') $RepoRoot
if ($LASTEXITCODE -ne 0) { Fail 'Docker image build failed.' }

# --- 4. Run the shared container-side runner -------------------------------
$mode = if ($Functional) { "func:$Functional" }
        elseif ($Suite -and -not $Check) { "suite:$Suite" }
        else { 'check' }
$jobsArg = if ($Jobs -gt 0) { "-j$Jobs" } else { '' }
Info "== Building + testing (LEG=asan, mode: $mode) =="

# ASan needs a relaxed seccomp profile + SYS_PTRACE under Docker Desktop, whose
# kernel uses high ASLR entropy; otherwise the sanitized binary segfaults at
# startup. The runner script is piped through `tr -d '\r'` so a CRLF checkout on
# Windows cannot break it.
Invoke-Docker run --rm `
    --security-opt seccomp=unconfined --cap-add SYS_PTRACE `
    -e LEG=asan -e "MODE=$mode" -e "JOBS_ARG=$jobsArg" -e "LOAD_HOGS=$LoadHogs" `
    -v "${RepoRoot}:/src:ro" `
    -v "${BuildVol}:/build" `
    -v "${CcacheVol}:/ccache" `
    $Image bash -c "tr -d '\r' < /src/contrib/ci-local-runner.sh | bash"
$code = $LASTEXITCODE

Info ''
if ($code -eq 0) {
    Info '== PASSED (matches the asan+ubsan CI leg config) =='
    Info '  Iterate: edit code, re-run this script (incremental + ccache = fast).'
    Info '  Fast single suite:  contrib\test-asan-local.ps1 -Suite <suite_name>'
} else {
    Fail "Tests failed (exit $code). Fix and re-run; the build stays cached."
}
