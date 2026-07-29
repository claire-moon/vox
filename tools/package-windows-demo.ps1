# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param(
    [string]$Version,
    [string]$DistDir,
    [string]$Triplet = 'x64-windows-static',
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Stop-Package {
    param([string]$Message)
    throw "package-windows-demo: $Message"
}

function Copy-RequiredFile {
    param(
        [string]$Source,
        [string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        Stop-Package "required file is missing: $Source"
    }
    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-RequiredTree {
    param(
        [string]$Source,
        [string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        Stop-Package "required directory is missing: $Source"
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination `
        -Recurse -Force
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($Version)) {
    if ($env:VOX_PACKAGE_VERSION) {
        $Version = $env:VOX_PACKAGE_VERSION
    } else {
        $Version = 'v0.0.3'
    }
}
if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $Root 'dist'
}
if ($Version -notmatch '^[A-Za-z0-9][A-Za-z0-9._+-]*$') {
    Stop-Package 'Version may contain only letters, digits, dot, underscore, plus, and hyphen'
}
$DistDir = [System.IO.Path]::GetFullPath($DistDir)
$dirty = (& git -C $Root status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0) {
    Stop-Package 'git status failed'
}
if ($dirty -and -not $AllowDirty) {
    $dirty | Write-Error
    Stop-Package 'the source tree is dirty; commit/stash changes before making a release bundle'
}

$vcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
if (-not $vcpkgRoot) {
    $vcpkgRoot = $env:VCPKG_ROOT
}
if (-not $vcpkgRoot -and (Test-Path -LiteralPath 'C:\vcpkg')) {
    $vcpkgRoot = 'C:\vcpkg'
}
if (-not $vcpkgRoot) {
    Stop-Package 'VCPKG_INSTALLATION_ROOT or VCPKG_ROOT is required to build SDL2'
}
$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path -LiteralPath $vcpkgExe -PathType Leaf) -or
    -not (Test-Path -LiteralPath $toolchain -PathType Leaf)) {
    Stop-Package 'the vcpkg executable or CMake toolchain file is missing'
}

if ($Triplet -notmatch '^[A-Za-z0-9_-]+$') {
    Stop-Package 'Vcpkg triplet may contain only letters, digits, underscore, and hyphen'
}

& $vcpkgExe install ("sdl2:" + $Triplet)
if ($LASTEXITCODE -ne 0) {
    Stop-Package "vcpkg could not install SDL2:$Triplet"
}

$stamp = (& git -C $Root show -s --format=%ct HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $stamp -notmatch '^[0-9]+$') {
    Stop-Package 'could not read the release source timestamp'
}
$commit = (& git -C $Root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    Stop-Package 'could not read the release commit'
}

$stem = "vox-digs-$Version-windows-x86_64"
$archive = Join-Path $DistDir "$stem.zip"
$work = Join-Path ([System.IO.Path]::GetTempPath()) (
    "vox-windows-package-" + [System.Guid]::NewGuid().ToString('N'))
$build = Join-Path $work 'build'
$stage = Join-Path $work $stem

try {
    New-Item -ItemType Directory -Force -Path $DistDir, $work, $stage | Out-Null

    & cmake -S $Root -B $build -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
        "-DVCPKG_TARGET_TRIPLET=$Triplet" `
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' `
        '-DVOX_BUILD_TESTS=ON' `
        '-DVOX_BUILD_SDL2_DEMO=ON' `
        '-DVOX_BUILD_NASM_ACCEL=OFF'
    if ($LASTEXITCODE -ne 0) {
        Stop-Package 'CMake configure failed'
    }
    & cmake --build $build --config Release --parallel 2
    if ($LASTEXITCODE -ne 0) {
        Stop-Package 'CMake build failed'
    }
    & ctest --test-dir $build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Stop-Package 'CTest failed'
    }
    Push-Location $Root
    try {
        & cargo test --workspace --locked
        if ($LASTEXITCODE -ne 0) {
            Stop-Package 'Cargo tests failed'
        }
    } finally {
        Pop-Location
    }

    $demo = Join-Path $build 'Release\digs_demo.exe'
    $share = Join-Path $build 'share'
    if (-not (Test-Path -LiteralPath $demo -PathType Leaf)) {
        Stop-Package 'the Release digs_demo.exe was not produced'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $share 'digs\scripts\manifest.txt') -PathType Leaf)) {
        Stop-Package 'the DIGS runtime data was not staged by CMake'
    }

    Push-Location $Root
    try {
        & $demo --input-self-test
        if ($LASTEXITCODE -ne 0) {
            Stop-Package 'digs_demo input self-test failed'
        }
        & $demo --load-self-test 600
        if ($LASTEXITCODE -ne 0) {
            Stop-Package 'digs_demo deterministic load self-test failed'
        }
    } finally {
        Pop-Location
    }

    New-Item -ItemType Directory -Force -Path (Join-Path $stage 'bin') | Out-Null
    Copy-RequiredFile $demo (Join-Path $stage 'bin\digs_demo.exe')
    Copy-RequiredTree $share (Join-Path $stage 'share')
    # SDL_GetBasePath resolves from bin/.  Copy the small data tree here too
    # so the released executable is self-contained without a Windows symlink.
    Copy-RequiredTree $share (Join-Path $stage 'bin\share')
    Copy-RequiredFile (Join-Path $Root 'packaging\windows\run-digs.bat') `
        (Join-Path $stage 'run-digs.bat')
    Copy-RequiredFile (Join-Path $Root 'packaging\windows\START-HERE.txt') `
        (Join-Path $stage 'START-HERE.txt')
    Copy-RequiredFile (Join-Path $Root 'README.md') (Join-Path $stage 'README.md')
    Copy-RequiredFile (Join-Path $Root 'LICENSE') (Join-Path $stage 'LICENSE')
    Copy-RequiredFile (Join-Path $Root 'THIRD_PARTY.md') `
        (Join-Path $stage 'THIRD_PARTY.md')
    Copy-RequiredTree (Join-Path $Root 'LICENSES') (Join-Path $stage 'LICENSES')

    & python (Join-Path $Root 'tools\generate-spdx-sbom.py') `
        --root $stage `
        --output (Join-Path $stage 'SBOM.spdx.json') `
        --version $Version `
        --commit $commit `
        --epoch $stamp `
        --document-name "VOX + DIGS $Version Windows x86_64 binary bundle" `
        --purpose APPLICATION `
        --namespace-label windows-x86_64-binary `
        --bundled-sdl2
    if ($LASTEXITCODE -ne 0) {
        Stop-Package 'SPDX SBOM generation failed'
    }

    if (Test-Path -LiteralPath $archive) {
        Remove-Item -Force -LiteralPath $archive
    }
    Compress-Archive -Path $stage -DestinationPath $archive -CompressionLevel Optimal
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or
        (Get-Item -LiteralPath $archive).Length -eq 0) {
        Stop-Package 'ZIP archive was not created'
    }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
    Set-Content -LiteralPath "$archive.sha256" `
        -Value "$hash  $(Split-Path -Leaf $archive)" -NoNewline
    Write-Host "Created $archive"
    Write-Host "SHA256 $hash"
} finally {
    if (Test-Path -LiteralPath $work) {
        Remove-Item -Recurse -Force -LiteralPath $work
    }
}
