<#
.SYNOPSIS
    LOCAL, TESTING-ONLY build of Rincoin Core Windows (x64) binaries, on Windows,
    using Docker + the MinGW-w64 cross toolchain.

.DESCRIPTION
    Requires only Docker Desktop (no WSL distro and no local compiler needed).

    >>> These binaries are for LOCAL TESTING / VERIFICATION ONLY.           <<<
    >>> They are UNSIGNED and NOT reproducible. Do NOT publish them or use  <<<
    >>> them for official releases — use the project's release process for  <<<
    >>> that (contrib/build_release.sh in a clean, reproducible env).       <<<

    Everything expensive is cached and reused between runs:
      * the MinGW dependency build (depends/) and the compiled objects live
        in a Docker named volume, so the slow first build (~30-60 min) is a
        one-time cost; later builds are incremental and fast;
      * ccache is persisted in its own volume.

    Nothing is added to the git repo: output goes to release-builds-local/
    (git-ignored) and all caches are Docker volumes outside the tree.

.PARAMETER Clean
    Remove the cached build/deps/ccache volumes and previous output, then
    rebuild from scratch.

.EXAMPLE
    contrib\build-windows-local.ps1
    contrib\build-windows-local.ps1 -Clean
#>
[CmdletBinding()]
param([switch]$Clean)

$ErrorActionPreference = 'Stop'

$RepoRoot  = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$OutDir    = Join-Path $RepoRoot 'release-builds-local\windows'
$Image     = 'rincoin-local:winbuild'
$BuildVol  = 'rincoin_local_build_win'    # source copy + depends + objects
$CcacheVol = 'rincoin_local_ccache_win'   # ccache
$TargetHost = 'x86_64-w64-mingw32'

function Info($m) { Write-Host $m -ForegroundColor Cyan }
function Fail($m) { Write-Host "ERROR: $m" -ForegroundColor Red; exit 1 }

# --- 1. Prerequisite checks ------------------------------------------------
Info '== Checking prerequisites =='
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Fail 'Docker not found. Install Docker Desktop (https://www.docker.com/products/docker-desktop/) and start it.'
}
try { docker info *> $null } catch {
    Fail 'Docker is installed but the daemon is not responding. Start Docker Desktop and try again.'
}
Info 'Docker OK.'

# --- 2. Clean if requested -------------------------------------------------
if ($Clean) {
    Info '== Cleaning caches and previous output =='
    docker volume rm $BuildVol $CcacheVol *> $null
    if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# --- 3. Build the toolchain image (cached by Docker layers) ----------------
Info '== Preparing build image (cached after first run) =='
$dockerfile = @'
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive TZ=UTC
RUN apt-get update && apt-get install -y \
      build-essential libtool autotools-dev automake pkg-config bsdmainutils \
      python3 curl git cmake ccache rsync \
      g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 mingw-w64-tools \
      nsis zip ca-certificates \
 && rm -rf /var/lib/apt/lists/* \
 && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
 && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
WORKDIR /build
'@
$dockerfile | docker build -t $Image -f - $RepoRoot
if ($LASTEXITCODE -ne 0) { Fail 'Docker image build failed.' }

# --- 4. Build --------------------------------------------------------------
Info '== Building (first run is slow; caches make later runs fast) =='
$buildScript = @'
set -euo pipefail
export CCACHE_DIR=/ccache HOST="$HOST"
mkdir -p /build/rincoin
# Sync source changes into the persistent build volume (no --delete: keep objects).
rsync -a --exclude=.git /src/ /build/rincoin/
cd /build/rincoin
# Dependencies (cached in the build volume -> fast on reruns).
make -C depends -j"$(nproc)" HOST="$HOST"
# Configure once; reuse the configuration on later runs (incremental make).
[ -x configure ] || ./autogen.sh
if [ ! -f config.status ]; then
  CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure \
      --prefix=/ --disable-tests --disable-bench --enable-reduce-exports --disable-gui-tests \
      CC="ccache $HOST-gcc" CXX="ccache $HOST-g++"
fi
make -j"$(nproc)"
for b in src/rincoind.exe src/rincoin-cli.exe src/rincoin-tx.exe src/rincoin-wallet.exe src/qt/rincoin-qt.exe; do
  [ -f "$b" ] && cp "$b" /out/ || true
done
( cd /out && sha256sum *.exe > SHA256SUMS.txt )
'@
docker run --rm `
    -v "${RepoRoot}:/src:ro" `
    -v "${OutDir}:/out" `
    -v "${BuildVol}:/build" `
    -v "${CcacheVol}:/ccache" `
    -e "HOST=$TargetHost" `
    $Image bash -c $buildScript
if ($LASTEXITCODE -ne 0) { Fail 'Build failed (see output above).' }

# --- 5. Summary / what to do next -----------------------------------------
Info ''
Info "== Done. Binaries written to: $OutDir =="
Get-ChildItem $OutDir | ForEach-Object { Write-Host "   $($_.Name)" }
Info ''
Info 'What now:'
Info '  * These are UNSIGNED, testing-only binaries — run/verify them locally.'
Info '  * Edit code, then run this script again: only changed files recompile'
Info '    (deps + objects are cached), so it will be much faster.'
Info '  * Full clean rebuild:  contrib\build-windows-local.ps1 -Clean'
Info '  * For an official build, use the reproducible release process instead.'
