<#
  KonamiV2 - Fix MANUEL Skia Build
  
  Ce script utilise les chemins EXACTS de ton log de build.
  Zero StrictMode, zero .Count sur des non-arrays, zero risque PowerShell.
  
  Il fait 3 choses:
  1. Patch config.rs dans le registre Cargo (chemin exact de ton log)
  2. Nettoie le cache de build Skia (chemin exact de ton log)
  3. Nettoie les args.gn corrompus
  
  Usage:
    .\fix-konamiv2-skia-build-manual.ps1
#>

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "  ======================================" -ForegroundColor Cyan
Write-Host "   Fix Skia Build - Mode Direct" -ForegroundColor Cyan
Write-Host "  ======================================" -ForegroundColor Cyan
Write-Host ""

# ---------------------------------------------------------------
# Chemins exacts tires de ton log de build
# ---------------------------------------------------------------

$ProjectDir = "C:\KonamiV2"

# Chemin exact du config.rs depuis ton log d'erreur ligne 437:
# C:\Users\bruck\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0\build_support\skia\config.rs:359
$exactConfigPath = "$env:USERPROFILE\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0\build_support\skia\config.rs"
$exactSkiaDir    = "$env:USERPROFILE\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\skia-bindings-0.78.0"

# Chemin exact du build skia depuis ton log ligne 330:
# C:/KonamiV2/build/x64/Release/cargo/build/x86_64-pc-windows-msvc/release/build/skia-bindings-5688a081a1b30eb6/out/skia
$skiaBuildOut = "C:\KonamiV2\build\x64\Release\cargo\build\x86_64-pc-windows-msvc\release\build"

# ---------------------------------------------------------------
# ETAPE 1 : Trouver config.rs
# ---------------------------------------------------------------

Write-Host "  [1/3] Recherche de config.rs..." -ForegroundColor Yellow

$configPath = $null

if (Test-Path $exactConfigPath) {
    $configPath = $exactConfigPath
    Write-Host "    OK chemin exact: $configPath" -ForegroundColor Green
}
else {
    Write-Host "    Chemin exact introuvable, recherche dans le registre Cargo..." -ForegroundColor Yellow
    
    $cargoRegistry = "$env:USERPROFILE\.cargo\registry\src"
    if (Test-Path $cargoRegistry) {
        # Chercher manuellement sans dependre de .Count
        $searchResult = Get-ChildItem -Path $cargoRegistry -Recurse -Filter "config.rs" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match 'skia-bindings-0\.78' -and $_.FullName -match 'build_support' } |
            Select-Object -First 1
        
        if ($null -ne $searchResult) {
            $configPath = $searchResult.FullName
            # Remonter de 2 niveaux: build_support/skia/config.rs -> skia-bindings-0.78.x
            $exactSkiaDir = Split-Path (Split-Path (Split-Path $configPath)) 
            Write-Host "    Trouve par recherche: $configPath" -ForegroundColor Green
        }
    }
}

if ($null -eq $configPath) {
    Write-Host "    ERREUR: config.rs introuvable." -ForegroundColor Red
    Write-Host "    Chemin attendu: $exactConfigPath" -ForegroundColor Red
    Write-Host "    Lance un build d'abord pour telecharger les sources Cargo." -ForegroundColor Yellow
    exit 1
}

# ---------------------------------------------------------------
# ETAPE 1b : Patcher config.rs
# ---------------------------------------------------------------

Write-Host ""
Write-Host "  [1b/3] Patch de config.rs..." -ForegroundColor Yellow

$content = [System.IO.File]::ReadAllText($configPath)

# Verifier si deja patche
if ($content.Contains("PATCHED_WOFF2")) {
    Write-Host "    Deja patche. On continue au nettoyage." -ForegroundColor Yellow
}
elseif (-not $content.Contains("skia_use_freetype_woff2")) {
    Write-Host "    Pas de skia_use_freetype_woff2 dans ce fichier. Rien a faire." -ForegroundColor Yellow
}
else {
    # Backup
    $bakPath = "$configPath.bak"
    if (-not (Test-Path $bakPath)) {
        Copy-Item $configPath $bakPath -Force
        Write-Host "    Backup: $bakPath" -ForegroundColor Gray
    }
    
    # --- PATCH ---
    # Le probleme: skia-bindings 0.78.0 passe l'argument GN "skia_use_freetype_woff2=false"
    # meme quand freetype est desactive (Windows). Or Skia upstream a supprime cet argument.
    # GN voit un argument inconnu -> marque build.ninja dirty -> boucle 100x -> crash.
    #
    # Fix: commenter la ligne ".arg("skia_use_freetype_woff2", ...)" 
    # UNIQUEMENT quand elle est en dehors du bloc "if use_freetype {" 
    # (= quand elle est toujours executee, meme si freetype est off).
    
    $lines = $content -split "`r?`n"
    $newLines = [System.Collections.Generic.List[string]]::new()
    $inFreetypeBlock = $false
    $braceCount = 0
    $patchCount = 0
    
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        
        # Detecter entree dans le bloc "if use_freetype {"
        if ((-not $inFreetypeBlock) -and ($line -match 'if\s+use_freetype')) {
            $inFreetypeBlock = $true
            $braceCount = 0
            foreach ($ch in $line.ToCharArray()) {
                if ($ch -eq [char]'{') { $braceCount++ }
                if ($ch -eq [char]'}') { $braceCount-- }
            }
            $newLines.Add($line)
            continue
        }
        
        # Dans le bloc freetype: tracker les accolades
        if ($inFreetypeBlock) {
            foreach ($ch in $line.ToCharArray()) {
                if ($ch -eq [char]'{') { $braceCount++ }
                if ($ch -eq [char]'}') { $braceCount-- }
            }
            if ($braceCount -le 0) { $inFreetypeBlock = $false }
            $newLines.Add($line)
            continue
        }
        
        # HORS du bloc freetype: commenter skia_use_freetype_woff2
        if (($line -match 'skia_use_freetype_woff2') -and ($line -notmatch '^\s*//')) {
            $indent = ""
            if ($line -match '^(\s+)') { $indent = $Matches[1] }
            $newLines.Add("${indent}// PATCHED_WOFF2: arg GN obsolete commente pour fix ninja dirty loop")
            $newLines.Add("${indent}// $($line.TrimStart())")
            $patchCount++
            Write-Host "    Ligne $($i+1) commentee: $($line.Trim())" -ForegroundColor Green
        }
        else {
            $newLines.Add($line)
        }
    }
    
    if ($patchCount -gt 0) {
        $joined = $newLines -join "`r`n"
        [System.IO.File]::WriteAllText($configPath, $joined, [System.Text.UTF8Encoding]::new($false))
        Write-Host "    $patchCount ligne(s) patchee(s) avec succes!" -ForegroundColor Green
    }
    else {
        Write-Host "    Aucune ligne a patcher hors du bloc freetype (deja correct)." -ForegroundColor Yellow
    }
    
    # Mettre a jour le checksum Cargo pour que Cargo accepte le fichier modifie
    $checksumFile = Join-Path $exactSkiaDir ".cargo-checksum.json"
    if (Test-Path $checksumFile) {
        try {
            $csText = [System.IO.File]::ReadAllText($checksumFile)
            # Remplacer le hash SHA256 du fichier config.rs par une chaine vide
            $csText = $csText -replace '"build_support/skia/config\.rs":"[a-f0-9]+"', '"build_support/skia/config.rs":""'
            $csText = $csText -replace '"build_support\\skia\\config\.rs":"[a-f0-9]+"', '"build_support\\skia\\config.rs":""'
            [System.IO.File]::WriteAllText($checksumFile, $csText, [System.Text.UTF8Encoding]::new($false))
            Write-Host "    Checksum Cargo mis a jour." -ForegroundColor Green
        }
        catch {
            Write-Host "    Warning checksum (non-critique): $($_.Exception.Message)" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "    Pas de .cargo-checksum.json trouve (non-critique)." -ForegroundColor Gray
    }
}

# ---------------------------------------------------------------
# ETAPE 2 : Nettoyer le cache de build Skia
# ---------------------------------------------------------------

Write-Host ""
Write-Host "  [2/3] Nettoyage du cache de build Skia..." -ForegroundColor Yellow

# 2a. Supprimer les dossiers out/ des skia-bindings dans le build tree
if (Test-Path $skiaBuildOut) {
    $deletedCount = 0
    
    Get-ChildItem -Path $skiaBuildOut -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue |
        ForEach-Object {
            $outDir = Join-Path $_.FullName "out"
            if (Test-Path $outDir) {
                try {
                    Remove-Item -Path $outDir -Recurse -Force -ErrorAction Stop
                    Write-Host "    Supprime: $($_.Name)\out" -ForegroundColor Green
                    $deletedCount++
                }
                catch {
                    Write-Host "    Impossible de supprimer: $outDir" -ForegroundColor Yellow
                }
            }
        }
    
    Write-Host "    $deletedCount dossier(s) out/ supprime(s)" -ForegroundColor Gray
    
    # 2b. Supprimer les fingerprints de skia-bindings
    $fpDir = Join-Path $skiaBuildOut ".fingerprint"
    if (Test-Path $fpDir) {
        Get-ChildItem -Path $fpDir -Directory -Filter "skia-bindings-*" -ErrorAction SilentlyContinue |
            ForEach-Object {
                try {
                    Remove-Item -Path $_.FullName -Recurse -Force -ErrorAction Stop
                    Write-Host "    Fingerprint supprimee: $($_.Name)" -ForegroundColor Green
                }
                catch { }
            }
    }
}
else {
    Write-Host "    Dossier de build Cargo pas trouve (normal si clean build)." -ForegroundColor Gray
}

# ---------------------------------------------------------------
# ETAPE 3 : Nettoyer les args.gn et ninja corrompus
# ---------------------------------------------------------------

Write-Host ""
Write-Host "  [3/3] Nettoyage des args.gn et fichiers ninja..." -ForegroundColor Yellow

$buildDir = Join-Path $ProjectDir "build"

if (Test-Path $buildDir) {
    # 3a. Nettoyer args.gn contenant skia_use_freetype_woff2
    $cleanedArgs = 0
    Get-ChildItem -Path $buildDir -Recurse -Filter "args.gn" -ErrorAction SilentlyContinue |
        ForEach-Object {
            try {
                $afContent = [System.IO.File]::ReadAllText($_.FullName)
                if ($afContent -match "skia_use_freetype_woff2") {
                    $afLines = $afContent -split "`r?`n"
                    $cleanLines = $afLines | Where-Object { $_ -notmatch "skia_use_freetype_woff2" }
                    [System.IO.File]::WriteAllText($_.FullName, ($cleanLines -join "`n"), [System.Text.UTF8Encoding]::new($false))
                    Write-Host "    args.gn nettoye: $($_.FullName)" -ForegroundColor Green
                    $cleanedArgs++
                }
            }
            catch { }
        }
    Write-Host "    $cleanedArgs fichier(s) args.gn nettoye(s)" -ForegroundColor Gray
    
    # 3b. Supprimer les build.ninja / .ninja_deps / .ninja_log dans les dossiers skia
    $ninjaDeleted = 0
    Get-ChildItem -Path $buildDir -Recurse -ErrorAction SilentlyContinue |
        Where-Object { ($_.Name -eq "build.ninja" -or $_.Name -eq ".ninja_deps" -or $_.Name -eq ".ninja_log") -and $_.FullName -match "skia" } |
        ForEach-Object {
            try {
                Remove-Item $_.FullName -Force -ErrorAction Stop
                Write-Host "    Supprime: $($_.Name)" -ForegroundColor Green
                $ninjaDeleted++
            }
            catch { }
        }
    Write-Host "    $ninjaDeleted fichier(s) ninja supprime(s)" -ForegroundColor Gray
}
else {
    Write-Host "    Dossier build introuvable: $buildDir" -ForegroundColor Yellow
}

# ---------------------------------------------------------------
# VERIFICATION FINALE
# ---------------------------------------------------------------

Write-Host ""
Write-Host "  Verification finale..." -ForegroundColor Cyan

$verifyContent = [System.IO.File]::ReadAllText($configPath)
if ($verifyContent.Contains("PATCHED_WOFF2")) {
    Write-Host "    config.rs est PATCHE correctement." -ForegroundColor Green
}
else {
    Write-Host "    ATTENTION: config.rs n'est PAS patche." -ForegroundColor Red
}

# ---------------------------------------------------------------
# RESUME
# ---------------------------------------------------------------

Write-Host ""
Write-Host "  ======================================" -ForegroundColor Cyan
Write-Host "   FIX TERMINE" -ForegroundColor Green
Write-Host "  ======================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Maintenant lance:" -ForegroundColor White
Write-Host "    cd C:\KonamiV2\build" -ForegroundColor Yellow
Write-Host "    cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Si Cargo refuse le fichier modifie (source verification):" -ForegroundColor Gray
Write-Host '    $env:CARGO_NET_OFFLINE="true"' -ForegroundColor Yellow
Write-Host "    cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Si ca echoue encore, clean rebuild complet:" -ForegroundColor Gray
Write-Host "    Remove-Item -Recurse -Force C:\KonamiV2\build" -ForegroundColor Yellow
Write-Host "    cd C:\KonamiV2" -ForegroundColor Yellow
Write-Host "    cmake -B build -S ." -ForegroundColor Yellow
Write-Host "    .\fix-konamiv2-skia-build-manual.ps1" -ForegroundColor Yellow
Write-Host "    cd build" -ForegroundColor Yellow
Write-Host "    cmake --build . --config Release" -ForegroundColor Yellow
Write-Host ""
