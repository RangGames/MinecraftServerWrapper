<#
.SYNOPSIS
    Builds a Release configuration and packages it as dist\ServerDesk-<version>-win64.zip.

.DESCRIPTION
    Shared by scripts\package.cmd (local, CLion toolchain) and the GitHub Actions workflow
    (MSYS2 toolchain). The compiler, Ninja and CMake are taken from PATH unless -CMake is given;
    callers that need a specific toolchain prepend it to PATH before invoking this script.

.PARAMETER BuildDirectory
    Build tree to configure/build/package from. Default: <repo>\cmake-build-release.

.PARAMETER CMake
    CMake executable to use. Default: "cmake" from PATH.

.PARAMETER Label
    Optional pre-release label appended to the version, e.g. "beta" -> ServerDesk-1.0.0-beta-win64.

.PARAMETER SkipBuild
    Package an existing build without configuring or building it (CI builds and tests first).
#>
[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$CMake = "cmake",
    [string]$Label,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $root "cmake-build-release"
}
if (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path (Get-Location).Path $BuildDirectory
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)

# Version comes from project(... VERSION x.y.z ...) in CMakeLists.txt.
$cmakeLists = Get-Content -Raw (Join-Path $root "CMakeLists.txt")
$match = [regex]::Match($cmakeLists, 'project\s*\([^)]*?\bVERSION\s+([0-9]+(?:\.[0-9]+)*)')
if (-not $match.Success) {
    throw "Could not find project(... VERSION x.y.z ...) in CMakeLists.txt"
}
$version = $match.Groups[1].Value
if ($Label) { $version = "$version-$Label" }
$name = "ServerDesk-$version-win64"

if (-not $SkipBuild) {
    if (-not (Test-Path (Join-Path $BuildDirectory "CMakeCache.txt"))) {
        & $CMake -S $root -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
    }
    & $CMake --build $BuildDirectory --target MinecraftServerWrapper -j 4
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($LASTEXITCODE)" }
}

$exe = Join-Path $BuildDirectory "MinecraftServerWrapper.exe"
if (-not (Test-Path $exe)) {
    throw "Missing $exe - build the Release configuration first"
}

# Stage the package layout, then zip the staged folder's contents.
$dist = Join-Path $root "dist"
$stage = Join-Path $BuildDirectory "package\$name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path (Join-Path $stage "assets\fonts") | Out-Null
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $root "assets\fonts\*") (Join-Path $stage "assets\fonts")
foreach ($doc in "LICENSE", "THIRD_PARTY.md", "README.md") {
    $source = Join-Path $root $doc
    if (Test-Path $source) {
        Copy-Item $source $stage
    } else {
        Write-Warning "Skipping $doc (not found)"
    }
}

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Created $zip"

# Also leave an unpacked copy next to the zip so the build can be run directly.
$unpacked = Join-Path $dist $name
if (Test-Path $unpacked) { Remove-Item -Recurse -Force $unpacked }
Copy-Item -Recurse $stage $unpacked
Write-Host "Unpacked to $unpacked"
