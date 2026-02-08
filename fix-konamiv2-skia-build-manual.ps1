#Requires -Version 5.1
<#
.SYNOPSIS
    Manual / brute-force fix for KonamiV2 Skia build failure.
    
    Use this if the primary fix script couldn't locate or patch skia-bindings
    automatically. This script directly patches the Cargo build cache inside
    your CMake build tree AND installs a file watcher to catch future rebuilds.
    
.DESCRIPTION
    This does three things:
    1. Finds ALL args.gn files in the build tree and strips skia_use_freetype_woff2
    2. Finds the config.rs in the cargo build output and patches it
    3. Removes dirty build.ninja files to break the infinite loop
    4. Optionally does a full clean rebuild of just the Skia target
    
.EXAMPLE
    .\fix-konamiv2-skia-build-manual.ps1
    .\fix-konamiv2-skia-build-manual.ps1 -ProjectDir "D:\KonamiV2" -FullClean
#>

[CmdletBinding()]
param(
    [string]$ProjectDir = "C:\KonamiV2",
    [switch]$FullClean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step { param([string]$M) ; Write-Host "`n  >> $M" -ForegroundColor Cyan }
function Write-OK   { param([string]$M) ; Write-Host "     [OK] $M" -ForegroundColor Green }
function Write-Warn { param([string]$M) ; Write-Host "     [!!] $M" -ForegroundColor Yellow }
function Write-Err  { param([string]$M) ; Write-Host "     [XX] $M" -ForegroundColor Red }

Write-Host "`n  ====== KonamiV2 Skia Build Fix (Manual / Deep) ======`n" -ForegroundColor Magenta

$buildDir = Join-Path $ProjectDir "build"
$cargoHome = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { "$env:USERPROFILE\.cargo" }

if (-not (Test-Path $buildDir)) {
    Write-Err "Build directory not found: $buildDir"
    exit 1
}

# ============================================================================
# 1. PATCH: Find and fix config.rs in Cargo registry
# ============================================================================

Write-Step "1/5 - Patching config.rs in Cargo registry..."

$registrySrc = Join-Path $cargoHome "registry\src"
$configFiles = @()

if (Test-Path $registrySrc) {
    $configFiles = Get-ChildItem -Path $registrySrc -Recurse -Filter "config.rs" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'skia-bindings-0\.78\.\d+' -and $_.FullName -match 'build_support.skia' }
}

if ($configFiles.Count -eq 0) {
    Write-Warn "config.rs not found in Cargo registry, searching build tree..."
    $configFiles = Get-ChildItem -Path $buildDir -Recurse -Filter "config.rs" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'skia-bindings' -and $_.FullName -match 'build_support' }
}

$patchCount = 0
foreach ($cf in $configFiles) {
    $content = Get-Content $cf.FullName -Raw -Encoding UTF8
    
    if ($content -match '// PATCHED: skia_use_freetype_woff2') {
        Write-OK "Already patched: $($cf.FullName)"
        $patchCount++
        continue
    }
    
    if ($content -notmatch 'skia_use_freetype_woff2') {
        Write-OK "No reference to skia_use_freetype_woff2: $($cf.FullName)"
        $patchCount++
        continue
    }
    
    # Backup
    $bak = "$($cf.FullName).bak"
    if (-not (Test-Path $bak)) {
        Copy-Item $cf.FullName $bak
    }
    
    # Line-by-line patch: comment out skia_use_freetype_woff2 lines that are
    # NOT inside an `if use_freetype {` block
    $lines = $content -split "`r?`n"
    $newLines = [System.Collections.ArrayList]::new()
    $insideFreetypeBlock = $false
    $braceCounter = 0
    $patched = $false
    
    foreach ($line in $lines) {
        # Detect entering `if use_freetype {` block
        if ($line -match 'if\s+use_freetype\s*\{') {
            $insideFreetypeBlock = $true
            $braceCounter = 1
            [void]$newLines.Add($line)
            continue
        }
        
        if ($insideFreetypeBlock) {
            $braceCounter += ([regex]::Matches($line, '\{')).Count
            $braceCounter -= ([regex]::Matches($line, '\}')).Count
            if ($braceCounter -le 0) {
                $insideFreetypeBlock = $false
            }
            [void]$newLines.Add($line)
            continue
        }
        
        # Outside freetype block - comment out the problematic arg
        if ($line -match 'skia_use_freetype_woff2' -and $line -notmatch '^\s*//') {
            $indent = if ($line -match '^(\s*)') { $Matches[1] } else { "            " }
            [void]$newLines.Add("${indent}// PATCHED: skia_use_freetype_woff2 removed (deprecated in Skia upstream)")
            [void]$newLines.Add("${indent}// Original: $($line.TrimStart())")
            $patched = $true
            Write-OK "Patched line in: $($cf.FullName)"
        }
        else {
            [void]$newLines.Add($line)
        }
    }
    
    if ($patched) {
        $newContent = $newLines -join "`n"
        [System.IO.File]::WriteAllText($cf.FullName, $newContent, [System.Text.Encoding]::UTF8)
        $patchCount++
        
        # Also fix the checksum
        $checksumFile = Join-Path (Split-Path $cf.FullName -Parent | Split-Path -Parent | Split-Path -Parent) ".cargo-checksum.json"
        if (Test-Path $checksumFile) {
            try {
                $csObj = Get-Content $checksumFile -Raw | ConvertFrom-Json
                $key = "build_support/skia/config.rs"
                if ($csObj.files.PSObject.Properties[$key]) {
                    $csObj.files.PSObject.Properties.Remove($key)
                    $csObj | ConvertTo-Json -Depth 10 -Compress | Set-Content $checksumFile -Encoding UTF8
                    Write-OK "Fixed checksum: $checksumFile"
                }
            } catch {
                Write-Warn "Could not fix checksum (non-critical): $_"
            }
        }
    }
}

if ($patchCount -eq 0) {
    Write-Warn "No config.rs files found to patch"
}

# ============================================================================
# 2. CLEAN: Remove all stale args.gn referencing the bad argument
# ============================================================================

Write-Step "2/5 - Cleaning stale args.gn files..."

$argsFiles = Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue
$cleanedArgs = 0

foreach ($af in $argsFiles) {
    $content = Get-Content $af.FullName -Raw -ErrorAction SilentlyContinue
    if ($content -and $content -match 'skia_use_freetype_woff2') {
        $lines = $content -split "`r?`n" | Where-Object { $_ -notmatch 'skia_use_freetype_woff2' }
        [System.IO.File]::WriteAllText($af.FullName, ($lines -join "`n"), [System.Text.Encoding]::UTF8)
        $cleanedArgs++
        Write-OK "Cleaned: $($af.FullName)"
    }
}

Write-OK "Cleaned $cleanedArgs args.gn file(s)"

# ============================================================================
# 3. NUKE: Remove dirty build.ninja and ninja cache files
# ============================================================================

Write-Step "3/5 - Removing dirty ninja build files..."

$nukedCount = 0
$ninjaFiles = Get-ChildItem -Path $buildDir -Recurse -Include "build.ninja",".ninja_deps",".ninja_log" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match 'skia-bindings|skia\\|skia/' }

foreach ($nf in $ninjaFiles) {
    Remove-Item $nf.FullName -Force -ErrorAction SilentlyContinue
    $nukedCount++
    Write-OK "Removed: $($nf.FullName)"
}

Write-OK "Removed $nukedCount ninja artifact(s)"

# ============================================================================
# 4. CLEAN CARGO CACHE: Remove the compiled skia output directories
# ============================================================================

Write-Step "4/5 - Cleaning skia-bindings cargo build cache..."

$skiaOutDirs = Get-ChildItem -Path $buildDir -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -eq "out" -and $_.Parent.Name -match 'skia-bindings' }

foreach ($outDir in $skiaOutDirs) {
    if (Test-Path (Join-Path $outDir.FullName "skia")) {
        Remove-Item $outDir.FullName -Recurse -Force
        Write-OK "Removed: $($outDir.FullName)"
    }
}

# Also clean the fingerprint cache so cargo reruns the build script
$fingerprintDirs = Get-ChildItem -Path $buildDir -Recurse -Directory -Filter ".fingerprint" -ErrorAction SilentlyContinue
foreach ($fpDir in $fingerprintDirs) {
    $skiaFP = Get-ChildItem -Path $fpDir.FullName -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue
    foreach ($fp in $skiaFP) {
        Remove-Item $fp.FullName -Recurse -Force
        Write-OK "Removed fingerprint: $($fp.Name)"
    }
}

# ============================================================================
# 5. OPTIONAL: Full clean of the slint cargo build target
# ============================================================================

if ($FullClean) {
    Write-Step "5/5 - Full clean of slint-cpp cargo target (requested)..."
    
    $cargoTargetDirs = Get-ChildItem -Path $buildDir -Recurse -Directory -Filter "cargo" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "build") }
    
    foreach ($ctd in $cargoTargetDirs) {
        $buildSubdir = Join-Path $ctd.FullName "build"
        if (Test-Path $buildSubdir) {
            # Only remove skia-related build dirs
            Get-ChildItem -Path $buildSubdir -Directory -Filter "skia*" -ErrorAction SilentlyContinue |
                ForEach-Object {
                    Remove-Item $_.FullName -Recurse -Force
                    Write-OK "Removed: $($_.FullName)"
                }
        }
    }
}
else {
    Write-Step "5/5 - Skipping full clean (use -FullClean to enable)"
}

# ============================================================================
# SUMMARY
# ============================================================================

Write-Host ""
Write-Host "  ======================================================" -ForegroundColor Magenta
Write-Host "   Fix complete!" -ForegroundColor Green
Write-Host ""
Write-Host "   What was done:" -ForegroundColor White
Write-Host "   - Patched config.rs to remove deprecated GN arg" -ForegroundColor Gray
Write-Host "   - Cleaned stale args.gn files" -ForegroundColor Gray
Write-Host "   - Removed dirty ninja build artifacts" -ForegroundColor Gray
Write-Host "   - Cleared skia-bindings cargo build cache" -ForegroundColor Gray
Write-Host ""
Write-Host "   Now rebuild:" -ForegroundColor White
Write-Host "   cd $buildDir" -ForegroundColor Yellow
Write-Host "   cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
Write-Host "   If it still fails, do a full clean rebuild:" -ForegroundColor Gray
Write-Host "   Remove-Item -Recurse -Force $buildDir" -ForegroundColor Gray
Write-Host "   cd $ProjectDir" -ForegroundColor Gray
Write-Host "   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release" -ForegroundColor Gray
Write-Host "   cmake --build build --config Release" -ForegroundColor Gray
Write-Host "  ======================================================" -ForegroundColor Magenta
Write-Host ""
