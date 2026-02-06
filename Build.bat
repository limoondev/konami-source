@echo off
REM ============================================================================
REM  Konami Client - Windows Build Script
REM ============================================================================
REM
REM  Usage:
REM    Build.bat                     -- Release build with auto-detected generator
REM    Build.bat Debug               -- Debug build
REM    Build.bat Release clean       -- Clean Release build
REM    Build.bat Release install     -- Build and install
REM    Build.bat Release package     -- Build and package
REM    Build.bat Debug tests         -- Debug build with tests
REM
REM ============================================================================

setlocal enabledelayedexpansion

REM -- Configuration --
set "PROJECT_NAME=KonamiClient"
set "SOURCE_DIR=%~dp0"
set "BUILD_TYPE=Release"
set "DO_CLEAN=0"
set "DO_INSTALL=0"
set "DO_PACKAGE=0"
set "DO_TESTS=0"
set "GENERATOR="
set "JOBS=%NUMBER_OF_PROCESSORS%"

if "%JOBS%"=="" set "JOBS=4"

REM -- Parse arguments --
:parse_args
if "%~1"=="" goto :done_args

if /I "%~1"=="Debug"          ( set "BUILD_TYPE=Debug"          & shift & goto :parse_args )
if /I "%~1"=="Release"        ( set "BUILD_TYPE=Release"        & shift & goto :parse_args )
if /I "%~1"=="RelWithDebInfo" ( set "BUILD_TYPE=RelWithDebInfo" & shift & goto :parse_args )
if /I "%~1"=="MinSizeRel"     ( set "BUILD_TYPE=MinSizeRel"     & shift & goto :parse_args )
if /I "%~1"=="clean"          ( set "DO_CLEAN=1"                & shift & goto :parse_args )
if /I "%~1"=="install"        ( set "DO_INSTALL=1"              & shift & goto :parse_args )
if /I "%~1"=="package"        ( set "DO_PACKAGE=1"              & shift & goto :parse_args )
if /I "%~1"=="tests"          ( set "DO_TESTS=1"                & shift & goto :parse_args )

echo [!] Unknown argument: %~1
shift
goto :parse_args

:done_args

set "BUILD_DIR=%SOURCE_DIR%build\%BUILD_TYPE%"
set "INSTALL_DIR=%SOURCE_DIR%install"

REM -- Header --
echo.
echo ============================================================
echo   %PROJECT_NAME% Build System
echo ============================================================
echo   Build Type:   %BUILD_TYPE%
echo   Source Dir:   %SOURCE_DIR%
echo   Build Dir:    %BUILD_DIR%
echo   Parallel:     %JOBS% jobs
echo ============================================================
echo.

REM ============================================================================
REM  Check prerequisites
REM ============================================================================
echo [*] Checking prerequisites...

where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [X] CMake not found. Install from https://cmake.org/download/
    exit /b 1
)
for /f "tokens=3" %%v in ('cmake --version ^| findstr /R "cmake version"') do (
    echo [*] CMake %%v
)

where git >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [X] Git not found. Install from https://git-scm.com/
    exit /b 1
)
echo [*] Git found

REM ============================================================================
REM  Detect generator
REM ============================================================================
if not "%GENERATOR%"=="" goto :generator_ready

REM Prefer Ninja
where ninja >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "GENERATOR=Ninja"
    echo [*] Generator: Ninja ^(detected^)
    goto :generator_ready
)

REM Detect Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property catalog_productLineVersion 2^>nul`) do set "VS_VER=%%i"

    if "!VS_VER!"=="2022" (
        set "GENERATOR=Visual Studio 17 2022"
        echo [*] Generator: Visual Studio 17 2022 ^(detected^)
        goto :generator_ready
    )
    if "!VS_VER!"=="2019" (
        set "GENERATOR=Visual Studio 16 2019"
        echo [*] Generator: Visual Studio 16 2019 ^(detected^)
        goto :generator_ready
    )
)

REM Fallback
set "GENERATOR=Ninja"
echo [*] Generator: Ninja ^(fallback^)

:generator_ready

REM Determine if multi-config generator
set "IS_MULTI_CONFIG=0"
echo %GENERATOR% | findstr /I "Visual Studio Xcode" >nul 2>&1
if %ERRORLEVEL% equ 0 set "IS_MULTI_CONFIG=1"
echo %GENERATOR% | findstr /I "Ninja Multi-Config" >nul 2>&1
if %ERRORLEVEL% equ 0 set "IS_MULTI_CONFIG=1"

REM ============================================================================
REM  Clean
REM ============================================================================
if "%DO_CLEAN%"=="1" (
    if exist "%BUILD_DIR%" (
        echo [*] Cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM ============================================================================
REM  Configure
REM ============================================================================
echo.
echo ============================================================
echo   Configuring ^(%BUILD_TYPE%^)
echo ============================================================

set "CONFIG_ARGS=-S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "%GENERATOR%""
set "CONFIG_ARGS=%CONFIG_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%"
set "CONFIG_ARGS=%CONFIG_ARGS% -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%""

REM Add -A x64 for Visual Studio generators
echo %GENERATOR% | findstr /I "Visual Studio" >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "CONFIG_ARGS=%CONFIG_ARGS% -A x64"
)

if "%DO_TESTS%"=="1" set "CONFIG_ARGS=%CONFIG_ARGS% -DKONAMI_BUILD_TESTS=ON"

echo [*] Running: cmake %CONFIG_ARGS%
cmake %CONFIG_ARGS%
if %ERRORLEVEL% neq 0 (
    echo [X] CMake configuration failed ^(exit code %ERRORLEVEL%^)
    exit /b %ERRORLEVEL%
)

REM ============================================================================
REM  Build
REM ============================================================================
echo.
echo ============================================================
echo   Building ^(%BUILD_TYPE%^)
echo ============================================================

set "BUILD_ARGS=--build "%BUILD_DIR%" --parallel %JOBS%"
if "%IS_MULTI_CONFIG%"=="1" set "BUILD_ARGS=%BUILD_ARGS% --config %BUILD_TYPE%"

echo [*] Running: cmake %BUILD_ARGS%
cmake %BUILD_ARGS%
if %ERRORLEVEL% neq 0 (
    echo [X] Build failed ^(exit code %ERRORLEVEL%^)
    exit /b %ERRORLEVEL%
)

echo [*] Build succeeded.

REM ============================================================================
REM  Tests
REM ============================================================================
if "%DO_TESTS%"=="1" (
    echo.
    echo ============================================================
    echo   Running Tests
    echo ============================================================

    set "TEST_ARGS=--test-dir "%BUILD_DIR%" --output-on-failure"
    if "%IS_MULTI_CONFIG%"=="1" set "TEST_ARGS=!TEST_ARGS! -C %BUILD_TYPE%"

    ctest !TEST_ARGS!
    if !ERRORLEVEL! neq 0 (
        echo [!] Some tests failed ^(exit code !ERRORLEVEL!^)
    )
)

REM ============================================================================
REM  Install
REM ============================================================================
if "%DO_INSTALL%"=="1" (
    echo.
    echo ============================================================
    echo   Installing to %INSTALL_DIR%
    echo ============================================================

    set "INSTALL_ARGS=--install "%BUILD_DIR%""
    if "%IS_MULTI_CONFIG%"=="1" set "INSTALL_ARGS=!INSTALL_ARGS! --config %BUILD_TYPE%"

    cmake !INSTALL_ARGS!
    if !ERRORLEVEL! neq 0 (
        echo [X] Install failed ^(exit code !ERRORLEVEL!^)
        exit /b !ERRORLEVEL!
    )
    echo [*] Installed to %INSTALL_DIR%
)

REM ============================================================================
REM  Package
REM ============================================================================
if "%DO_PACKAGE%"=="1" (
    echo.
    echo ============================================================
    echo   Packaging
    echo ============================================================

    pushd "%BUILD_DIR%"
    set "CPACK_ARGS="
    if "%IS_MULTI_CONFIG%"=="1" set "CPACK_ARGS=-C %BUILD_TYPE%"

    cpack !CPACK_ARGS!
    if !ERRORLEVEL! neq 0 (
        echo [X] Packaging failed ^(exit code !ERRORLEVEL!^)
        popd
        exit /b !ERRORLEVEL!
    )
    echo [*] Package created in %BUILD_DIR%
    popd
)

REM ============================================================================
REM  Summary
REM ============================================================================
echo.
echo ============================================================
echo   Build Complete
echo ============================================================
echo   Project:    %PROJECT_NAME%
echo   Type:       %BUILD_TYPE%
echo   Generator:  %GENERATOR%
echo   Build dir:  %BUILD_DIR%

set "BIN_DIR=%BUILD_DIR%\bin"
if "%IS_MULTI_CONFIG%"=="1" set "BIN_DIR=%BUILD_DIR%\bin\%BUILD_TYPE%"
if exist "%BIN_DIR%" echo   Binaries:   %BIN_DIR%

echo ============================================================
echo.
echo Done.

endlocal
