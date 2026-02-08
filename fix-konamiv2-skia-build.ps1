<#
  KonamiV2 - Fix Skia Build (skia-bindings 0.78.x)
  
  Probleme: skia_use_freetype_woff2 est un argument GN obsolete.
  Skia l'a supprime, mais skia-bindings 0.78.0 le passe toujours.
  GN le voit comme inconnu -> marque build.ninja dirty -> boucle infinie x100.
  
  Ce script:
  1. Trouve config.rs dans le registre Cargo
  2. Commente la ligne qui passe skia_use_freetype_woff2 hors du bloc freetype
  3. Met a jour le checksum Cargo
  4. Nettoie les artefacts de build corrompus (args.gn, build.ninja)
  
  Usage:
    .\fix-konamiv2-skia-build.ps1
    .\fix-konamiv2-skia-build.ps1 -ProjectDir "D:\MonProjet"
#>
param(
    [string]$ProjectDir = "C:\KonamiV2"
)

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host "   KonamiV2 Skia Build Fix" -ForegroundColor Magenta
Write-Host "   Cible: skia-bindings 0.78.x / skia_use_freetype_woff2" -ForegroundColor Magenta
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host ""

# -------------------------------------------------------------------
# ETAPE 1 : Trouver config.rs dans le registre Cargo
# -------------------------------------------------------------------

Write-Host "  [1/4] Recherche de skia-bindings dans le registre Cargo..." -ForegroundColor Cyan

$cargoHome = $env:CARGO_HOME
if (-not $cargoHome) { $cargoHome = "$env:USERPROFILE\.cargo" }
$registrySrc = Join-Path $cargoHome "registry\src"

if (-not (Test-Path $registrySrc)) {
    Write-Host "    ERREUR: Registre Cargo introuvable: $registrySrc" -ForegroundColor Red
    Write-Host "    Verifie que Rust est installe." -ForegroundColor Red
    exit 1
}

# Chercher tous les dossiers skia-bindings-0.78.* dans tous les index
$allConfigFiles = @()
$indexDirs = @(Get-ChildItem -Path $registrySrc -Directory -ErrorAction SilentlyContinue)

foreach ($indexDir in $indexDirs) {
    $skiaDirs = @(Get-ChildItem -Path $indexDir.FullName -Directory -Filter "skia-bindings-0.78.*" -ErrorAction SilentlyContinue)
    foreach ($skiaDir in $skiaDirs) {
        $configPath = Join-Path $skiaDir.FullName "build_support\skia\config.rs"
        if (Test-Path $configPath) {
            $allConfigFiles += @{
                ConfigPath = $configPath
                SkiaDir    = $skiaDir.FullName
                Version    = $skiaDir.Name
            }
            Write-Host "    Trouve: $($skiaDir.Name)" -ForegroundColor Green
            Write-Host "      -> $configPath" -ForegroundColor Gray
        }
    }
}

if ($allConfigFiles.Count -eq 0) {
    Write-Host "    ERREUR: Aucun skia-bindings-0.78.* trouve dans $registrySrc" -ForegroundColor Red
    Write-Host ""
    Write-Host "    Essaie de lancer un build d'abord pour que Cargo telecharge les sources:" -ForegroundColor Yellow
    Write-Host "    cd $ProjectDir\build && cmake --build . --config Release" -ForegroundColor Yellow
    exit 1
}

# -------------------------------------------------------------------
# ETAPE 2 : Patcher config.rs
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  [2/4] Patch de config.rs..." -ForegroundColor Cyan

foreach ($entry in $allConfigFiles) {
    $configPath = $entry.ConfigPath
    $skiaDir = $entry.SkiaDir
    $version = $entry.Version
    
    $content = [System.IO.File]::ReadAllText($configPath)
    
    # Deja patche ?
    if ($content.Contains("PATCHED_WOFF2")) {
        Write-Host "    [$version] Deja patche, on passe." -ForegroundColor Yellow
        continue
    }
    
    # Verifier que le fichier contient bien le probleme
    if (-not $content.Contains("skia_use_freetype_woff2")) {
        Write-Host "    [$version] Pas de reference a skia_use_freetype_woff2, rien a faire." -ForegroundColor Yellow
        continue
    }
    
    # Backup
    $bakPath = "$configPath.original"
    if (-not (Test-Path $bakPath)) {
        Copy-Item $configPath $bakPath -Force
        Write-Host "    Backup: $bakPath" -ForegroundColor Gray
    }
    
    # Strategie: lire ligne par ligne, tracker si on est dans le bloc "if use_freetype {"
    # et commenter la ligne skia_use_freetype_woff2 quand on est EN DEHORS de ce bloc
    $lines = $content -split "`r?`n"
    $result = [System.Collections.Generic.List[string]]::new()
    $insideFreetypeBlock = $false
    $braceDepth = 0
    $patchedCount = 0
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Detecter l'entree dans "if use_freetype {"
        if ((-not $insideFreetypeBlock) -and ($line -match 'if\s+use_freetype\s*\{')) {
            $insideFreetypeBlock = $true
            $braceDepth = 0
            # Compter les accolades sur cette ligne
            foreach ($c in $line.ToCharArray()) {
                if ($c -eq '{') { $braceDepth++ }
                if ($c -eq '}') { $braceDepth-- }
            }
            $result.Add($line)
            continue
        }
        
        # Tracker la profondeur des accolades dans le bloc freetype
        if ($insideFreetypeBlock) {
            foreach ($c in $line.ToCharArray()) {
                if ($c -eq '{') { $braceDepth++ }
                if ($c -eq '}') { $braceDepth-- }
            }
            if ($braceDepth -le 0) {
                $insideFreetypeBlock = $false
            }
            $result.Add($line)
            continue
        }
        
        # EN DEHORS du bloc freetype: commenter skia_use_freetype_woff2
        if ($line -match 'skia_use_freetype_woff2' -and $line -notmatch '^\s*//') {
            # Extraire l'indentation
            $indent = ""
            if ($line -match '^(\s+)') { $indent = $Matches[1] }
            $result.Add("${indent}// PATCHED_WOFF2: ligne commentee car arg GN obsolete dans Skia upstream")
            $result.Add("${indent}// $($line.TrimStart())")
            $patchedCount++
            Write-Host "    [$version] Ligne $($i+1) commentee:" -ForegroundColor Green
            Write-Host "      $($line.Trim())" -ForegroundColor DarkGray
        }
        else {
            $result.Add($line)
        }
    }
    
    if ($patchedCount -gt 0) {
        $newContent = $result -join "`n"
        [System.IO.File]::WriteAllText($configPath, $newContent, [System.Text.UTF8Encoding]::new($false))
        Write-Host "    [$version] $patchedCount ligne(s) patchee(s) avec succes" -ForegroundColor Green
    }
    else {
        Write-Host "    [$version] Aucune ligne a patcher (toutes dans le bloc freetype)" -ForegroundColor Yellow
    }
    
    # -------------------------------------------------------------------
    # ETAPE 3 : Mettre a jour le checksum Cargo
    # -------------------------------------------------------------------
    
    $checksumPath = Join-Path $skiaDir ".cargo-checksum.json"
    if (Test-Path $checksumPath) {
        try {
            $csContent = [System.IO.File]::ReadAllText($checksumPath)
            # Approche simple: remplacer le hash de config.rs par une chaine vide
            # Cargo accepte les fichiers sans hash dans le manifest
            $csContent = $csContent -replace '"build_support/skia/config\.rs":"[a-f0-9]+"', '"build_support/skia/config.rs":""'
            [System.IO.File]::WriteAllText($checksumPath, $csContent, [System.Text.UTF8Encoding]::new($false))
            Write-Host "    [$version] Checksum mis a jour" -ForegroundColor Green
        }
        catch {
            Write-Host "    [$version] Impossible de mettre a jour le checksum (non-critique): $_" -ForegroundColor Yellow
        }
    }
}

# -------------------------------------------------------------------
# ETAPE 4 : Nettoyer les artefacts de build corrompus
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  [3/4] Nettoyage des artefacts de build corrompus..." -ForegroundColor Cyan

$buildDir = Join-Path $ProjectDir "build"

if (Test-Path $buildDir) {
    # 4a. Nettoyer les args.gn qui contiennent l'argument problematique
    $argsFiles = @(Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue)
    $cleanedArgs = 0
    
    foreach ($af in $argsFiles) {
        $afContent = ""
        try { $afContent = [System.IO.File]::ReadAllText($af.FullName) } catch { continue }
        
        if ($afContent.Contains("skia_use_freetype_woff2")) {
            $newLines = ($afContent -split "`r?`n") | Where-Object { $_ -notmatch 'skia_use_freetype_woff2' }
            [System.IO.File]::WriteAllText($af.FullName, ($newLines -join "`n"), [System.Text.UTF8Encoding]::new($false))
            $cleanedArgs++
            Write-Host "    args.gn nettoye: $($af.FullName)" -ForegroundColor Green
        }
    }
    Write-Host "    $cleanedArgs fichier(s) args.gn nettoye(s)" -ForegroundColor Gray
    
    # 4b. Supprimer les build.ninja et caches ninja dans les dossiers skia
    $ninjaFiles = @(Get-ChildItem -Path $buildDir -Recurse -Include @("build.ninja",".ninja_deps",".ninja_log") -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match 'skia-bindings|\\skia\\|/skia/' })
    $ninjaCount = 0
    
    foreach ($nf in $ninjaFiles) {
        try {
            Remove-Item $nf.FullName -Force -ErrorAction Stop
            $ninjaCount++
            Write-Host "    Supprime: $($nf.FullName)" -ForegroundColor Green
        }
        catch {
            Write-Host "    Impossible de supprimer: $($nf.FullName) - $_" -ForegroundColor Yellow
        }
    }
    Write-Host "    $ninjaCount fichier(s) ninja supprime(s)" -ForegroundColor Gray
    
    # 4c. Supprimer les dossiers 'out' de skia-bindings dans le cache cargo du build
    $skiaOutDirs = @(Get-ChildItem -Path $buildDir -Recurse -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "out" -and $_.Parent.Name -match 'skia-bindings' })
    
    foreach ($outDir in $skiaOutDirs) {
        $skiaSubDir = Join-Path $outDir.FullName "skia"
        if (Test-Path $skiaSubDir) {
            try {
                Remove-Item $outDir.FullName -Recurse -Force -ErrorAction Stop
                Write-Host "    Cache skia supprime: $($outDir.FullName)" -ForegroundColor Green
            }
            catch {
                Write-Host "    Impossible de supprimer: $($outDir.FullName) - $_" -ForegroundColor Yellow
            }
        }
    }
    
    # 4d. Supprimer les fingerprints cargo de skia-bindings
    $fpDirs = @(Get-ChildItem -Path $buildDir -Recurse -Directory -Filter ".fingerprint" -ErrorAction SilentlyContinue)
    foreach ($fpDir in $fpDirs) {
        $skiaFPs = @(Get-ChildItem -Path $fpDir.FullName -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue)
        foreach ($fp in $skiaFPs) {
            try {
                Remove-Item $fp.FullName -Recurse -Force -ErrorAction Stop
                Write-Host "    Fingerprint supprimee: $($fp.Name)" -ForegroundColor Green
            }
            catch {
                Write-Host "    Impossible de supprimer fingerprint: $($fp.FullName) - $_" -ForegroundColor Yellow
            }
        }
    }
}
else {
    Write-Host "    Dossier build introuvable: $buildDir (pas grave si premier build)" -ForegroundColor Yellow
}

# -------------------------------------------------------------------
# ETAPE 4 : Creer le hook CMake permanent
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  [4/4] Installation du hook CMake permanent..." -ForegroundColor Cyan

$cmakeFixPath = Join-Path $ProjectDir "cmake\fix-skia-woff2.cmake"
$cmakeFixDir = Join-Path $ProjectDir "cmake"

if (-not (Test-Path $cmakeFixDir)) {
    New-Item -ItemType Directory -Path $cmakeFixDir -Force | Out-Null
}

$cmakeFixContent = @'
# fix-skia-woff2.cmake
# Hook CMake qui nettoie automatiquement l'argument GN obsolete
# "skia_use_freetype_woff2" des fichiers args.gn generes par skia-bindings.
#
# Ajouter dans CMakeLists.txt APRES le FetchContent de Slint:
#   include(cmake/fix-skia-woff2.cmake)

# Fonction appelee avant chaque build pour nettoyer les args.gn
function(fix_skia_woff2_args BUILD_DIR)
    file(GLOB_RECURSE ARGS_GN_FILES "${BUILD_DIR}/**/args.gn")
    foreach(ARGS_FILE ${ARGS_GN_FILES})
        file(READ "${ARGS_FILE}" ARGS_CONTENT)
        if(ARGS_CONTENT MATCHES "skia_use_freetype_woff2")
            string(REGEX REPLACE "[^\n]*skia_use_freetype_woff2[^\n]*\n?" "" FIXED_CONTENT "${ARGS_CONTENT}")
            file(WRITE "${ARGS_FILE}" "${FIXED_CONTENT}")
            message(STATUS "[fix-skia-woff2] Cleaned: ${ARGS_FILE}")
        endif()
    endforeach()
endfunction()

# Appeler automatiquement au moment du configure
fix_skia_woff2_args("${CMAKE_BINARY_DIR}")
'@

[System.IO.File]::WriteAllText($cmakeFixPath, $cmakeFixContent, [System.Text.UTF8Encoding]::new($false))
Write-Host "    Cree: $cmakeFixPath" -ForegroundColor Green
Write-Host "    Pour rendre le fix permanent, ajoute dans ton CMakeLists.txt:" -ForegroundColor Gray
Write-Host '    include(cmake/fix-skia-woff2.cmake)' -ForegroundColor Yellow

# -------------------------------------------------------------------
# RESUME
# -------------------------------------------------------------------

Write-Host ""
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host "   FIX APPLIQUE" -ForegroundColor Green
Write-Host "" 
Write-Host "   Ce qui a ete fait:" -ForegroundColor White
Write-Host "   - config.rs patche (skia_use_freetype_woff2 commente)" -ForegroundColor Gray
Write-Host "   - Checksum Cargo mis a jour" -ForegroundColor Gray
Write-Host "   - args.gn corrompus nettoyes" -ForegroundColor Gray
Write-Host "   - Cache ninja et fingerprints supprimes" -ForegroundColor Gray
Write-Host "   - Hook CMake permanent cree" -ForegroundColor Gray
Write-Host ""
Write-Host "   Prochaine etape:" -ForegroundColor White
Write-Host "   cd $ProjectDir\build" -ForegroundColor Yellow
Write-Host "   cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
Write-Host "   Si le build recree le probleme plus tard:" -ForegroundColor Gray
Write-Host "   Ajoute dans CMakeLists.txt:" -ForegroundColor Gray
Write-Host '   include(cmake/fix-skia-woff2.cmake)' -ForegroundColor Yellow
Write-Host "  ==========================================================" -ForegroundColor Magenta
Write-Host ""
