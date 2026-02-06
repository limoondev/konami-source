<#
.SYNOPSIS
    Konami Client - Cross-platform build script for PowerShell.

.DESCRIPTION
    Automates the full CMake configure, build, and optional install/package steps.
    Detects the platform, available generators, and compilers automatically.

.PARAMETER BuildType
    Build configuration: Debug, Release, RelWithDebInfo, MinSizeRel.
    Default: Release

.PARAMETER Generator
    CMake generator. Auto-detected when omitted.

.PARAMETER Clean
    Remove the build directory before configuring.

.PARAMETER Install
    Run cmake --install after building.

.PARAMETER Package
    Run cpack to create distributable packages.

.PARAMETER Jobs
    Number of parallel build jobs (0 = auto).

.PARAMETER EnableTests
    Build and run unit tests.

.PARAMETER EnableLTO
    Enable Link Time Optimization.

.PARAMETER EnableASAN
    Enable AddressSanitizer (GCC/Clang only).

.PARAMETER Verbose
    Show verbose compiler output.

.EXAMPLE
    .\Build.ps1
    .\Build.ps1 -BuildType Debug -EnableTests -Clean
    .\Build.ps1 -BuildType Release -Package -EnableLTO
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",

    [string]$Generator = "",

    [switch]$Clean,
    [switch]$Install,
    [switch]$Package,

    [int]$Jobs = 0,

    [switch]$EnableTests,
    [switch]$EnableLTO,
    [switch]$EnableASAN,
    [switch]$Verbose
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# Configuration
# ============================================================================
$ProjectName   = "KonamiClient"
$SourceDir     = $PSScriptRoot
$BuildDir      = Join-Path $SourceDir "build" $BuildType.ToLower()
$InstallDir    = Join-Path $SourceDir "install"

# ============================================================================
# Utilities
# ============================================================================
function Write-Header {
    param([string]$Message)
    $line = "=" * 60
    Write-Host ""
    Write-Host $line -ForegroundColor Cyan
    Write-Host "  $Message" -ForegroundColor Cyan
    Write-Host $line -ForegroundColor Cyan
}

function Write-Step {
    param([string]$Message)
    Write-Host "[*] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[!] $Message" -ForegroundColor Yellow
}

function Write-Err {
    param([string]$Message)
    Write-Host "[X] $Message" -ForegroundColor Red
}

function Test-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-JobCount {
    if ($Jobs -gt 0) { return $Jobs }
    try {
        $cores = [Environment]::ProcessorCount
        if ($cores -gt 0) { return $cores }
    } catch {}
    return 4
}

# ============================================================================
# Pre-flight checks
# ============================================================================
Write-Header "$ProjectName Build System (PowerShell)"

Write-Step "Checking prerequisites..."

# CMake
if (-not (Test-Command "cmake")) {
    Write-Err "CMake not found. Install from https://cmake.org/download/"
    exit 1
}
$cmakeVersion = (cmake --version | Select-Object -First 1) -replace "cmake version ", ""
Write-Step "CMake $cmakeVersion"

# Git (needed for FetchContent)
if (-not (Test-Command "git")) {
    Write-Err "Git not found. Install from https://git-scm.com/"
    exit 1
}
Write-Step "Git found"

# ============================================================================
# Detect generator
# ============================================================================
function Select-Generator {
    if ($Generator -ne "") { return $Generator }

    if ($IsWindows -or ($env:OS -eq "Windows_NT")) {
        # Prefer Ninja, then Visual Studio
        if (Test-Command "ninja") {
            Write-Step "Generator: Ninja (detected)"
            return "Ninja"
        }
        # Detect Visual Studio via vswhere
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $vsVersion = & $vsWhere -latest -property catalog_productLineVersion 2>$null
            switch ($vsVersion) {
                "2022" { Write-Step "Generator: Visual Studio 17 2022"; return "Visual Studio 17 2022" }
                "2019" { Write-Step "Generator: Visual Studio 16 2019"; return "Visual Studio 16 2019" }
            }
        }
        Write-Step "Generator: Ninja Multi-Config (fallback)"
        return "Ninja Multi-Config"
    }

    # Linux / macOS: prefer Ninja, then Makefiles
    if (Test-Command "ninja") {
        Write-Step "Generator: Ninja (detected)"
        return "Ninja"
    }
    if (Test-Command "make") {
        Write-Step "Generator: Unix Makefiles (detected)"
        return "Unix Makefiles"
    }

    Write-Err "No supported build generator found. Install Ninja or Make."
    exit 1
}

$SelectedGenerator = Select-Generator

# ============================================================================
# Detect compiler on non-Windows
# ============================================================================
$cmakeExtraArgs = @()

if (-not ($IsWindows -or ($env:OS -eq "Windows_NT"))) {
    if ($SelectedGenerator -ne "Xcode") {
        if (Test-Command "g++") {
            Write-Step "Compiler: g++"
            $cmakeExtraArgs += "-DCMAKE_CXX_COMPILER=g++"
            $cmakeExtraArgs += "-DCMAKE_C_COMPILER=gcc"
        } elseif (Test-Command "clang++") {
            Write-Step "Compiler: clang++"
            $cmakeExtraArgs += "-DCMAKE_CXX_COMPILER=clang++"
            $cmakeExtraArgs += "-DCMAKE_C_COMPILER=clang"
        }
    }
}

# ============================================================================
# Clean
# ============================================================================
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Cleaning build directory: $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

# ============================================================================
# CMake Configure
# ============================================================================
Write-Header "Configuring ($BuildType)"

$configArgs = @(
    "-S", $SourceDir
    "-B", $BuildDir
    "-G", $SelectedGenerator
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DCMAKE_INSTALL_PREFIX=$InstallDir"
)

# Multi-config generators set build type at build time, not configure time
$isMultiConfig = $SelectedGenerator -match "Visual Studio|Ninja Multi-Config|Xcode"

# Options
if ($EnableTests)  { $configArgs += "-DKONAMI_BUILD_TESTS=ON" }
if ($EnableLTO)    { $configArgs += "-DKONAMI_ENABLE_LTO=ON" }
if ($EnableASAN)   { $configArgs += "-DKONAMI_ENABLE_ASAN=ON" }

$configArgs += $cmakeExtraArgs

Write-Step "Running: cmake $($configArgs -join ' ')"
& cmake @configArgs
if ($LASTEXITCODE -ne 0) {
    Write-Err "CMake configuration failed (exit code $LASTEXITCODE)"
    exit $LASTEXITCODE
}

# ============================================================================
# Build
# ============================================================================
Write-Header "Building ($BuildType)"

$jobCount = Get-JobCount
Write-Step "Parallel jobs: $jobCount"

$buildArgs = @(
    "--build", $BuildDir
    "--parallel", $jobCount
)

if ($isMultiConfig) {
    $buildArgs += "--config"
    $buildArgs += $BuildType
}

if ($Verbose) {
    $buildArgs += "--verbose"
}

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Err "Build failed (exit code $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Step "Build succeeded."

# ============================================================================
# Tests
# ============================================================================
if ($EnableTests) {
    Write-Header "Running Tests"
    $testArgs = @("--test-dir", $BuildDir, "--output-on-failure")
    if ($isMultiConfig) {
        $testArgs += "-C"
        $testArgs += $BuildType
    }
    & ctest @testArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "Some tests failed (exit code $LASTEXITCODE)"
    }
}

# ============================================================================
# Install
# ============================================================================
if ($Install) {
    Write-Header "Installing to $InstallDir"
    $installArgs = @("--install", $BuildDir)
    if ($isMultiConfig) {
        $installArgs += "--config"
        $installArgs += $BuildType
    }
    & cmake @installArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Install failed (exit code $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
    Write-Step "Installed to $InstallDir"
}

# ============================================================================
# Package
# ============================================================================
if ($Package) {
    Write-Header "Packaging"
    Push-Location $BuildDir
    try {
        $cpackArgs = @()
        if ($isMultiConfig) {
            $cpackArgs += "-C"
            $cpackArgs += $BuildType
        }
        & cpack @cpackArgs
        if ($LASTEXITCODE -ne 0) {
            Write-Err "Packaging failed (exit code $LASTEXITCODE)"
            exit $LASTEXITCODE
        }
        Write-Step "Package created in $BuildDir"
    } finally {
        Pop-Location
    }
}

# ============================================================================
# Summary
# ============================================================================
Write-Header "Build Complete"
Write-Step "Project:    $ProjectName"
Write-Step "Type:       $BuildType"
Write-Step "Generator:  $SelectedGenerator"
Write-Step "Build dir:  $BuildDir"

$binaryDir = Join-Path $BuildDir "bin"
if ($isMultiConfig) { $binaryDir = Join-Path $binaryDir $BuildType }
if (Test-Path $binaryDir) {
    Write-Step "Binaries:   $binaryDir"
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
