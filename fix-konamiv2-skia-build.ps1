#Requires -Version 5.1
<#
.SYNOPSIS
    Fix KonamiV2 Skia build failure - skia-bindings v0.78.0
    
.DESCRIPTION
    This script fixes the infinite ninja regeneration loop caused by the
    deprecated "skia_use_freetype_woff2" GN argument in skia-bindings 0.78.0.
    
    Root cause: Skia upstream removed the "skia_use_freetype_woff2" build arg,
    but skia-bindings 0.78.0 still passes it. GN treats unknown args as warnings
    that dirty the build.ninja manifest, causing ninja to loop 100 times and crash.
    
    Fix: Patch config.rs to only emit skia_use_freetype_woff2 when freetype is
    actually enabled (it isn't on Windows), and clean stale build artifacts.
    
.NOTES
    Safe to run multiple times (idempotent).
    Does NOT downgrade or disable anything.
    
.EXAMPLE
    .\fix-konamiv2-skia-build.ps1
    .\fix-konamiv2-skia-build.ps1 -ProjectDir "D:\MyProject" -CargoHome "D:\cargo"
#>

[CmdletBinding()]
param(
    [string]$ProjectDir = "C:\KonamiV2",
    [string]$CargoHome = "$env:USERPROFILE\.cargo"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# UTILITIES
# ============================================================================

function Write-Step {
    param([string]$Message, [string]$Color = "Cyan")
    Write-Host ""
    Write-Host "  [$([char]0x2192)] $Message" -ForegroundColor $Color
}

function Write-OK {
    param([string]$Message)
    Write-Host "      [OK] $Message" -ForegroundColor Green
}

function Write-Skip {
    param([string]$Message)
    Write-Host "      [SKIP] $Message" -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host "      [FAIL] $Message" -ForegroundColor Red
}

function Write-Banner {
    $banner = @"

  ==============================================================
   KonamiV2 Skia Build Fix
   Target: skia-bindings v0.78.0 / Slint 1.8.0
   Issue:  ninja infinite regeneration (skia_use_freetype_woff2)
  ==============================================================

"@
    Write-Host $banner -ForegroundColor Magenta
}

# ============================================================================
# STEP 1: Locate skia-bindings source in Cargo registry
# ============================================================================

function Find-SkiaBindingsSource {
    Write-Step "Locating skia-bindings v0.78.0 in Cargo registry..."
    
    $registryBase = Join-Path $CargoHome "registry\src"
    
    if (-not (Test-Path $registryBase)) {
        Write-Fail "Cargo registry not found at: $registryBase"
        Write-Host "         Make sure Rust/Cargo is installed and has been used at least once." -ForegroundColor Gray
        return $null
    }
    
    # Search all registry index folders
    $candidates = Get-ChildItem -Path $registryBase -Directory | ForEach-Object {
        $skiaDir = Join-Path $_.FullName "skia-bindings-0.78.0"
        if (Test-Path $skiaDir) { $skiaDir }
    }
    
    # Also check for 0.78.x variants
    $candidatesAlt = Get-ChildItem -Path $registryBase -Directory | ForEach-Object {
        Get-ChildItem -Path $_.FullName -Directory -Filter "skia-bindings-0.78.*" -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName }
    }
    
    $allCandidates = @($candidates) + @($candidatesAlt) | Sort-Object -Unique | Where-Object { $_ }
    
    if ($allCandidates.Count -eq 0) {
        Write-Fail "skia-bindings 0.78.x not found in Cargo registry"
        Write-Host "         Expected location: $registryBase\*\skia-bindings-0.78.*" -ForegroundColor Gray
        return $null
    }
    
    foreach ($dir in $allCandidates) {
        $version = Split-Path $dir -Leaf
        Write-OK "Found: $version at $dir"
    }
    
    return $allCandidates
}

# ============================================================================
# STEP 2: Patch config.rs - Move skia_use_freetype_woff2 inside freetype block
# ============================================================================

function Patch-ConfigRs {
    param([string]$SkiaBindingsDir)
    
    $configPath = Join-Path $SkiaBindingsDir "build_support\skia\config.rs"
    
    if (-not (Test-Path $configPath)) {
        # Try alternate path
        $configPath = Join-Path $SkiaBindingsDir "build_support\skia\config.rs"
        if (-not (Test-Path $configPath)) {
            Write-Fail "config.rs not found at expected path"
            Write-Host "         Searched: $configPath" -ForegroundColor Gray
            return $false
        }
    }
    
    $version = Split-Path $SkiaBindingsDir -Leaf
    Write-Step "Patching config.rs in $version..."
    
    $content = Get-Content $configPath -Raw -Encoding UTF8
    $originalContent = $content
    
    # Create backup
    $backupPath = "$configPath.bak"
    if (-not (Test-Path $backupPath)) {
        Copy-Item $configPath $backupPath -Force
        Write-OK "Backup created: $backupPath"
    } else {
        Write-Skip "Backup already exists"
    }
    
    # -------------------------------------------------------------------------
    # PATCH STRATEGY:
    # The problematic line is approximately:
    #   .arg("skia_use_freetype_woff2", yes_if(features.freetype_woff2))
    # 
    # This line is at the TOP level of the GN args builder, meaning it's ALWAYS
    # passed to Skia regardless of whether freetype is enabled.
    #
    # On Windows, skia_use_freetype=false, so skia_use_freetype_woff2 is
    # meaningless. But newer Skia versions removed this arg entirely, so
    # passing it (even as false) causes GN to mark build.ninja as dirty
    # in an infinite loop.
    #
    # Fix: Comment out / remove the line that unconditionally sets this arg.
    # The arg is still set correctly inside the `if use_freetype { ... }` block
    # when freetype IS enabled (Linux etc.), where it's still valid.
    # -------------------------------------------------------------------------
    
    $patched = $false
    
    # Pattern 1: Direct .arg("skia_use_freetype_woff2", ...) outside the freetype block
    # We need to find and comment out the FIRST occurrence that's NOT inside an if block
    
    # Check if already patched
    if ($content -match '// PATCHED: skia_use_freetype_woff2') {
        Write-Skip "Already patched (found patch marker)"
        return $true
    }
    
    # Strategy A: Find the exact line with .arg("skia_use_freetype_woff2"
    # and comment it out, adding our marker
    $patterns = @(
        # Exact pattern from skia-bindings 0.78.0
        '\.arg\("skia_use_freetype_woff2",\s*yes_if\(features\.freetype_woff2\)\)',
        # Variant with no_if
        '\.arg\("skia_use_freetype_woff2",\s*no\(\)\)',
        # Generic catch-all
        '\.arg\("skia_use_freetype_woff2",[^)]*\)'
    )
    
    foreach ($pattern in $patterns) {
        if ($content -match $pattern) {
            $match = [regex]::Match($content, $pattern)
            $matchValue = $match.Value
            
            # We need to determine if this match is inside an `if use_freetype` block or not.
            # Look at the context: find the position and check if it's between 
            # "if use_freetype {" and the matching "}"
            
            $pos = $match.Index
            $beforeMatch = $content.Substring(0, $pos)
            
            # Count how many times this pattern appears
            $allMatches = [regex]::Matches($content, $pattern)
            
            if ($allMatches.Count -eq 1) {
                # Only one occurrence - this is the problematic one at top level
                # Comment it out
                $replacement = "// PATCHED: skia_use_freetype_woff2 removed (arg deprecated in Skia upstream)"
                $content = $content.Substring(0, $match.Index) + $replacement + $content.Substring($match.Index + $match.Length)
                $patched = $true
                Write-OK "Commented out: $matchValue"
                break
            }
            elseif ($allMatches.Count -ge 2) {
                # Multiple occurrences - only patch the FIRST one (top-level)
                # The second one is inside the if use_freetype block and is fine
                $firstMatch = $allMatches[0]
                $replacement = "// PATCHED: skia_use_freetype_woff2 removed (arg deprecated in Skia upstream)"
                $content = $content.Substring(0, $firstMatch.Index) + $replacement + $content.Substring($firstMatch.Index + $firstMatch.Length)
                $patched = $true
                Write-OK "Commented out first (top-level) occurrence: $matchValue"
                Write-OK "Kept second occurrence (inside freetype block)"
                break
            }
        }
    }
    
    if (-not $patched) {
        # Strategy B: Line-by-line approach
        Write-Step "  Trying line-by-line patch strategy..."
        
        $lines = $content -split "`n"
        $newLines = @()
        $inFreetypeBlock = $false
        $braceDepth = 0
        $patchedLine = $false
        
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $line = $lines[$i]
            
            # Track if we're inside the `if use_freetype {` block
            if ($line -match 'if\s+use_freetype\s*\{') {
                $inFreetypeBlock = $true
                $braceDepth = 1
            }
            elseif ($inFreetypeBlock) {
                $openBraces = ([regex]::Matches($line, '\{')).Count
                $closeBraces = ([regex]::Matches($line, '\}')).Count
                $braceDepth += $openBraces - $closeBraces
                if ($braceDepth -le 0) {
                    $inFreetypeBlock = $false
                }
            }
            
            # Only patch if NOT inside the freetype block
            if (-not $inFreetypeBlock -and $line -match 'skia_use_freetype_woff2' -and -not $patchedLine) {
                $newLines += "            // PATCHED: skia_use_freetype_woff2 removed (arg deprecated in Skia upstream)"
                $newLines += "            // Original: $($line.Trim())"
                $patchedLine = $true
                Write-OK "Patched line $($i + 1): $($line.Trim())"
            }
            else {
                $newLines += $line
            }
        }
        
        if ($patchedLine) {
            $content = $newLines -join "`n"
            $patched = $true
        }
    }
    
    if ($patched) {
        # Write the patched file
        [System.IO.File]::WriteAllText($configPath, $content, [System.Text.Encoding]::UTF8)
        Write-OK "config.rs patched successfully"
        return $true
    }
    else {
        Write-Skip "No skia_use_freetype_woff2 reference found in config.rs (may already be fixed in this version)"
        return $true
    }
}

# ============================================================================
# STEP 3: Clean stale Skia build artifacts (args.gn with bad args)
# ============================================================================

function Clean-StaleBuildArtifacts {
    Write-Step "Cleaning stale Skia build artifacts..."
    
    $buildDir = Join-Path $ProjectDir "build"
    
    if (-not (Test-Path $buildDir)) {
        Write-Skip "Build directory not found: $buildDir"
        return
    }
    
    # Find all args.gn files that contain the problematic argument
    $argsFiles = Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue
    $cleanedCount = 0
    
    foreach ($argsFile in $argsFiles) {
        $argsContent = Get-Content $argsFile.FullName -Raw -ErrorAction SilentlyContinue
        if ($argsContent -and $argsContent -match 'skia_use_freetype_woff2') {
            # Remove the problematic line from args.gn
            $newContent = ($argsContent -split "`n" | Where-Object { $_ -notmatch 'skia_use_freetype_woff2' }) -join "`n"
            [System.IO.File]::WriteAllText($argsFile.FullName, $newContent, [System.Text.Encoding]::UTF8)
            $cleanedCount++
            Write-OK "Cleaned: $($argsFile.FullName)"
        }
    }
    
    if ($cleanedCount -eq 0) {
        Write-Skip "No stale args.gn files found"
    }
    
    # Find and remove the cached skia build output that has the dirty build.ninja
    $skiaBuildDirs = Get-ChildItem -Path $buildDir -Recurse -Directory -Filter "skia" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "build.ninja") }
    
    foreach ($skiaDir in $skiaBuildDirs) {
        $buildNinja = Join-Path $skiaDir.FullName "build.ninja"
        if (Test-Path $buildNinja) {
            Remove-Item $buildNinja -Force
            Write-OK "Removed dirty: $buildNinja"
        }
        
        # Also remove the .ninja_deps and .ninja_log which cache the dirty state
        @(".ninja_deps", ".ninja_log") | ForEach-Object {
            $ninjaFile = Join-Path $skiaDir.FullName $_
            if (Test-Path $ninjaFile) {
                Remove-Item $ninjaFile -Force
                Write-OK "Removed: $ninjaFile"
            }
        }
    }
    
    # Clean the cargo build cache for skia-bindings specifically
    $cargoBuildDirs = Get-ChildItem -Path $buildDir -Recurse -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue
    
    foreach ($dir in $cargoBuildDirs) {
        # Only remove the 'out' subfolder which contains the compiled skia
        $outDir = Join-Path $dir.FullName "out"
        if (Test-Path $outDir) {
            Remove-Item $outDir -Recurse -Force
            Write-OK "Cleaned cargo build cache: $($dir.Name)\out"
        }
    }
}

# ============================================================================
# STEP 4: Also patch the Cargo .cargo-checksum.json to allow modified source
# ============================================================================

function Patch-CargoChecksum {
    param([string]$SkiaBindingsDir)
    
    $version = Split-Path $SkiaBindingsDir -Leaf
    Write-Step "Updating Cargo checksum for $version..."
    
    $checksumPath = Join-Path $SkiaBindingsDir ".cargo-checksum.json"
    
    if (-not (Test-Path $checksumPath)) {
        Write-Skip "No .cargo-checksum.json found (source may not be from crates.io)"
        return
    }
    
    $checksumContent = Get-Content $checksumPath -Raw -Encoding UTF8
    
    # Parse the JSON
    try {
        $checksumObj = $checksumContent | ConvertFrom-Json
    }
    catch {
        Write-Fail "Failed to parse checksum JSON: $_"
        return
    }
    
    # The "files" field contains checksums for each file.
    # We need to remove the entry for build_support/skia/config.rs
    # so Cargo doesn't reject our patched file.
    $configKey = "build_support/skia/config.rs"
    
    if ($checksumObj.files.PSObject.Properties[$configKey]) {
        # Remove the specific file checksum - Cargo will skip verification for this file
        $checksumObj.files.PSObject.Properties.Remove($configKey)
        
        $newChecksumJson = $checksumObj | ConvertTo-Json -Depth 10 -Compress
        [System.IO.File]::WriteAllText($checksumPath, $newChecksumJson, [System.Text.Encoding]::UTF8)
        Write-OK "Removed checksum for config.rs (allows patched source)"
    }
    else {
        Write-Skip "config.rs checksum not found (already removed or not present)"
    }
}

# ============================================================================
# STEP 5: Verify the fix
# ============================================================================

function Verify-Fix {
    param([string[]]$SkiaBindingsDirs)
    
    Write-Step "Verifying fix..."
    
    $allGood = $true
    
    foreach ($dir in $SkiaBindingsDirs) {
        $configPath = Join-Path $dir "build_support\skia\config.rs"
        if (Test-Path $configPath) {
            $content = Get-Content $configPath -Raw -Encoding UTF8
            
            # Check that skia_use_freetype_woff2 is either:
            # 1. Completely absent
            # 2. Only present inside comments (// PATCHED)
            # 3. Only present inside the if use_freetype block
            
            $uncommentedMatches = [regex]::Matches($content, '(?<!//.*?)\.arg\("skia_use_freetype_woff2"')
            
            if ($uncommentedMatches.Count -eq 0) {
                Write-OK "$(Split-Path $dir -Leaf): No unconditional skia_use_freetype_woff2 found"
            }
            else {
                # Check if remaining matches are inside the freetype block
                foreach ($match in $uncommentedMatches) {
                    $before = $content.Substring(0, $match.Index)
                    $lastFreetypeBlock = $before.LastIndexOf("if use_freetype")
                    $lastCloseBrace = $before.LastIndexOf("}")
                    
                    if ($lastFreetypeBlock -gt $lastCloseBrace) {
                        Write-OK "$(Split-Path $dir -Leaf): Remaining reference is safely inside freetype block"
                    }
                    else {
                        Write-Fail "$(Split-Path $dir -Leaf): WARNING - Unconditional reference still exists at position $($match.Index)"
                        $allGood = $false
                    }
                }
            }
        }
    }
    
    # Check build directory
    $buildDir = Join-Path $ProjectDir "build"
    if (Test-Path $buildDir) {
        $dirtyArgs = Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue |
            Where-Object { (Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue) -match 'skia_use_freetype_woff2' }
        
        if ($dirtyArgs.Count -eq 0) {
            Write-OK "No stale args.gn files with skia_use_freetype_woff2"
        }
        else {
            Write-Fail "Found $($dirtyArgs.Count) stale args.gn file(s)"
            $allGood = $false
        }
    }
    
    return $allGood
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Banner

# Pre-flight checks
Write-Step "Pre-flight checks..."

if (-not (Test-Path $ProjectDir)) {
    Write-Fail "Project directory not found: $ProjectDir"
    Write-Host ""
    Write-Host "  Usage: .\fix-konamiv2-skia-build.ps1 -ProjectDir 'C:\YourProject'" -ForegroundColor Yellow
    exit 1
}
Write-OK "Project directory: $ProjectDir"

if (-not (Test-Path $CargoHome)) {
    Write-Fail "Cargo home not found: $CargoHome"
    exit 1
}
Write-OK "Cargo home: $CargoHome"

# Step 1: Find skia-bindings
$skiaBindingsDirs = Find-SkiaBindingsSource

if (-not $skiaBindingsDirs -or $skiaBindingsDirs.Count -eq 0) {
    Write-Host ""
    Write-Fail "Cannot proceed without skia-bindings source. Exiting."
    exit 1
}

# Step 2 & 4: Patch each found version
foreach ($dir in $skiaBindingsDirs) {
    $patchResult = Patch-ConfigRs -SkiaBindingsDir $dir
    if ($patchResult) {
        Patch-CargoChecksum -SkiaBindingsDir $dir
    }
}

# Step 3: Clean stale build artifacts
Clean-StaleBuildArtifacts

# Step 5: Verify
$verified = Verify-Fix -SkiaBindingsDirs $skiaBindingsDirs

Write-Host ""
Write-Host "  ==============================================================" -ForegroundColor Magenta

if ($verified) {
    Write-Host "   FIX APPLIED SUCCESSFULLY" -ForegroundColor Green
    Write-Host ""
    Write-Host "   Next steps:" -ForegroundColor White
    Write-Host "   1. cd $ProjectDir\build" -ForegroundColor Gray
    Write-Host "   2. cmake --build . --config Release" -ForegroundColor Gray
    Write-Host ""
    Write-Host "   The build should now compile Skia from source without" -ForegroundColor Gray
    Write-Host "   the infinite ninja regeneration loop." -ForegroundColor Gray
}
else {
    Write-Host "   FIX PARTIALLY APPLIED - Manual review needed" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "   Some issues remain. Check the [FAIL] messages above." -ForegroundColor Gray
    Write-Host "   You may need to fully clean and rebuild:" -ForegroundColor Gray
    Write-Host ""
    Write-Host "   1. Remove-Item -Recurse -Force $ProjectDir\build" -ForegroundColor Gray
    Write-Host "   2. cmake -B build -S . -DCMAKE_BUILD_TYPE=Release" -ForegroundColor Gray
    Write-Host "   3. cmake --build build --config Release" -ForegroundColor Gray
}

Write-Host "  ==============================================================" -ForegroundColor Magenta
Write-Host ""
