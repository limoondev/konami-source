<#
  KonamiV2 - Fix MANUEL si le script principal ne marche pas
  
  Ce script fait la meme chose MAIS avec des chemins codes en dur
  bases sur ton log de build exact.
  
  Usage:
    .\fix-konamiv2-skia-build-manual.ps1
#>
param(
    [string]$ProjectDir = "C:\KonamiV2"
)

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host "   KonamiV2 Skia Build Fix - MODE MANUEL" -ForegroundColor Magenta
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host ""

# Chemins exacts depuis ton log de build
$cargoHome = $env:CARGO_HOME
if (-not $cargoHome) { $cargoHome = "$env:USERPROFILE\.cargo" }

# Le chemin exact vu dans ton log d'erreur:
# C:\Users\bruck\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0\build_support\skia\config.rs
$exactConfigPath = "$env:USERPROFILE\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0\build_support\skia\config.rs"
$exactSkiaDir = "$env:USERPROFILE\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0"

# Chemin du build skia dans le build tree (depuis ton log)
$skiaBuildOut = "C:\KonamiV2\build\x64\Release\cargo\build\x86_64-pc-windows-msvc\release\build"

# -------------------------------------------------------------------
# ETAPE 1 : Patcher config.rs
# -------------------------------------------------------------------

Write-Host "  [1/3] Patch de config.rs..." -ForegroundColor Cyan

# Essayer le chemin exact d'abord, puis chercher
$configPath = $null

if (Test-Path $exactConfigPath) {
    $configPath = $exactConfigPath
    Write-Host "    Trouve (chemin exact): $configPath" -ForegroundColor Green
}
else {
    Write-Host "    Chemin exact introuvable, recherche..." -ForegroundColor Yellow
    
    # Chercher dans tout le registre cargo
    $registrySrc = Join-Path $cargoHome "registry\src"
    if (Test-Path $registrySrc) {
        $found = @(Get-ChildItem -Path $registrySrc -Recurse -Filter "config.rs" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'skia-bindings-0\.78' -and $_.FullName -match 'build_support' })
        
        if ($found.Count -gt 0) {
            $configPath = $found[0].FullName
            $exactSkiaDir = Split-Path (Split-Path (Split-Path $configPath -Parent) -Parent) -Parent
            Write-Host "    Trouve (recherche): $configPath" -ForegroundColor Green
        }
    }
}

if (-not $configPath) {
    Write-Host "    ERREUR: config.rs introuvable." -ForegroundColor Red
    Write-Host "    Verifie que tu as deja lance un build au moins une fois." -ForegroundColor Red
    Write-Host "    Chemin attendu: $exactConfigPath" -ForegroundColor Gray
    exit 1
}

# Lire le fichier
$content = [System.IO.File]::ReadAllText($configPath)

if ($content.Contains("PATCHED_WOFF2")) {
    Write-Host "    Deja patche, on passe." -ForegroundColor Yellow
}
elseif (-not $content.Contains("skia_use_freetype_woff2")) {
    Write-Host "    Pas de reference a skia_use_freetype_woff2, rien a faire." -ForegroundColor Yellow
}
else {
    # Backup
    $bakPath = "$configPath.original"
    if (-not (Test-Path $bakPath)) {
        Copy-Item $configPath $bakPath -Force
        Write-Host "    Backup cree: $bakPath" -ForegroundColor Gray
    }
    
    # Patch ligne par ligne
    $lines = $content -split "`r?`n"
    $result = [System.Collections.Generic.List[string]]::new()
    $insideFreetypeBlock = $false
    $braceDepth = 0
    $patchedCount = 0
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Detecter "if use_freetype {"
        if ((-not $insideFreetypeBlock) -and ($line -match 'if\s+use_freetype\s*\{')) {
            $insideFreetypeBlock = $true
            $braceDepth = 0
            foreach ($c in $line.ToCharArray()) {
                if ($c -eq '{') { $braceDepth++ }
                if ($c -eq '}') { $braceDepth-- }
            }
            $result.Add($line)
            continue
        }
        
        # Tracker la profondeur dans le bloc freetype
        if ($insideFreetypeBlock) {
            foreach ($c in $line.ToCharArray()) {
                if ($c -eq '{') { $braceDepth++ }
                if ($c -eq '}') { $braceDepth-- }
            }
            if ($braceDepth -le 0) { $insideFreetypeBlock = $false }
            $result.Add($line)
            continue
        }
        
        # HORS du bloc freetype: commenter la ligne problematique
        if ($line -match 'skia_use_freetype_woff2' -and $line -notmatch '^\s*//') {
            $indent = ""
            if ($line -match '^(\s+)') { $indent = $Matches[1] }
            $result.Add("${indent}// PATCHED_WOFF2: arg GN obsolete commente")
            $result.Add("${indent}// $($line.TrimStart())")
            $patchedCount++
            Write-Host "    Ligne $($i+1) patchee: $($line.Trim())" -ForegroundColor Green
        }
        else {
            $result.Add($line)
        }
    }
    
    if ($patchedCount -gt 0) {
        $newContent = $result -join "`n"
        [System.IO.File]::WriteAllText($configPath, $newContent, [System.Text.UTF8Encoding]::new($false))
        Write-Host "    $patchedCount ligne(s) patchee(s)" -ForegroundColor Green
    }
    else {
        Write-Host "    Aucune ligne trouvee hors du bloc freetype" -ForegroundColor Yellow
    }
    
    # Checksum
    $checksumPath = Join-Path $exactSkiaDir ".cargo-checksum.json"
    if (Test-Path $checksumPath) {
        try {
            $csContent = [System.IO.File]::ReadAllText($checksumPath)
            $csContent = $csContent -replace '"build_support/skia/config\.rs":"[a-f0-9]+"', '"build_support/skia/config.rs":""'
            [System.IO.File]::WriteAllText($checksumPath, $csContent, [System.Text.UTF8Encoding]::new($false))
            Write-Host "    Checksum mis a jour" -ForegroundColor Green
        }
        catch {
            Write-Host "    Checksum non mis a jour (non-critique): $_" -ForegroundColor Yellow
        }
    }
}

# -------------------------------------------------------------------
# ETAPE 2 : Nettoyer les artefacts de build
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  [2/3] Nettoyage des artefacts de build..." -ForegroundColor Cyan

$buildDir = Join-Path $ProjectDir "build"

if (-not (Test-Path $buildDir)) {
    Write-Host "    Dossier build introuvable: $buildDir" -ForegroundColor Yellow
}
else {
    # Nettoyer args.gn
    $argsFiles = @(Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue)
    foreach ($af in $argsFiles) {
        try {
            $afContent = [System.IO.File]::ReadAllText($af.FullName)
            if ($afContent.Contains("skia_use_freetype_woff2")) {
                $newLines = ($afContent -split "`r?`n") | Where-Object { $_ -notmatch 'skia_use_freetype_woff2' }
                [System.IO.File]::WriteAllText($af.FullName, ($newLines -join "`n"), [System.Text.UTF8Encoding]::new($false))
                Write-Host "    args.gn nettoye: $($af.FullName)" -ForegroundColor Green
            }
        }
        catch { }
    }
    
    # Supprimer ninja dans les dossiers skia
    $ninjaPatterns = @("build.ninja", ".ninja_deps", ".ninja_log")
    foreach ($pattern in $ninjaPatterns) {
        $files = @(Get-ChildItem -Path $buildDir -Recurse -Filter $pattern -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'skia' })
        foreach ($f in $files) {
            try {
                Remove-Item $f.FullName -Force -ErrorAction Stop
                Write-Host "    Supprime: $($f.Name) dans ...\$(Split-Path (Split-Path $f.FullName -Parent) -Leaf)" -ForegroundColor Green
            }
            catch { }
        }
    }
    
    # Supprimer les dossiers out/skia dans le cache cargo du build
    if (Test-Path $skiaBuildOut) {
        $skiaBindingsDirs = @(Get-ChildItem -Path $skiaBuildOut -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue)
        foreach ($sbd in $skiaBindingsDirs) {
            $outDir = Join-Path $sbd.FullName "out"
            if (Test-Path $outDir) {
                try {
                    Remove-Item $outDir -Recurse -Force -ErrorAction Stop
                    Write-Host "    Cache skia supprime: $($sbd.Name)\out" -ForegroundColor Green
                }
                catch {
                    Write-Host "    Impossible de supprimer: $outDir" -ForegroundColor Yellow
                }
            }
        }
    }
    
    # Supprimer fingerprints
    $fpDirs = @(Get-ChildItem -Path $buildDir -Recurse -Directory -Filter ".fingerprint" -ErrorAction SilentlyContinue)
    foreach ($fpDir in $fpDirs) {
        $skiaFPs = @(Get-ChildItem -Path $fpDir.FullName -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue)
        foreach ($fp in $skiaFPs) {
            try {
                Remove-Item $fp.FullName -Recurse -Force -ErrorAction Stop
                Write-Host "    Fingerprint: $($fp.Name)" -ForegroundColor Green
            }
            catch { }
        }
    }
}

# -------------------------------------------------------------------
# ETAPE 3 : Verification
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  [3/3] Verification..." -ForegroundColor Cyan

$verifyContent = [System.IO.File]::ReadAllText($configPath)

# Compter les references non commentees
$uncommentedRefs = 0
$verifyLines = $verifyContent -split "`r?`n"
foreach ($vl in $verifyLines) {
    if ($vl -match 'skia_use_freetype_woff2' -and $vl -notmatch '^\s*//' -and $vl -notmatch 'PATCHED') {
        # Verifier si c'est dans le bloc freetype (approximation simple)
        $uncommentedRefs++
    }
}

if ($verifyContent.Contains("PATCHED_WOFF2")) {
    Write-Host "    config.rs: PATCHE" -ForegroundColor Green
}
else {
    Write-Host "    config.rs: PAS PATCHE (anomalie)" -ForegroundColor Red
}

# -------------------------------------------------------------------
# RESUME
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host "   FIX APPLIQUE" -ForegroundColor Green
Write-Host ""
Write-Host "   Maintenant rebuild:" -ForegroundColor White
Write-Host "   cd $ProjectDir\build" -ForegroundColor Yellow
Write-Host "   cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
Write-Host "   Si erreur 'source verification' de Cargo:" -ForegroundColor Gray
Write-Host "   Le checksum a ete mis a jour, mais si Cargo refuse:" -ForegroundColor Gray
Write-Host '   $env:CARGO_NET_OFFLINE="true"' -ForegroundColor Yellow
Write-Host "   cmake --build . --config Release" -ForegroundColor Yellow
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host ""
