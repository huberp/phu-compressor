@echo off
:: ---------------------------------------------------------------------------
:: release.bat — Build Release and package VST3 for distribution
::
:: Usage:
::   scripts\release.bat [version]
::
::   version  Optional semver string, e.g. 1.0.0  (defaults to "dev")
::
:: Output:
::   dist\PHU-COMPRESSOR-windows-<version>.zip
::
:: Requirements:
::   - CMake 3.23+ on PATH
::   - PowerShell 5+ (included with Windows 10/11, used for zipping)
::   - Visual Studio 2022 (or 2026) with Desktop C++ workload installed
::   - JUCE submodule initialised:  git submodule update --init --recursive
:: ---------------------------------------------------------------------------
setlocal EnableExtensions EnableDelayedExpansion

:: ---- Version ---------------------------------------------------------------
set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=dev"

set "ZIP_NAME=PHU-COMPRESSOR-windows-%VERSION%.zip"

echo.
echo ============================================================
echo  phu-compressor ^| Windows Release Package
echo  Version       : %VERSION%
echo  Package       : dist\%ZIP_NAME%
echo ============================================================
echo.

:: ---- Check CMake -----------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not found on PATH.
    echo Please install CMake 3.23+ and add it to your PATH.
    exit /b 1
)

:: ---- Navigate to repo root -------------------------------------------------
pushd "%~dp0\.."

:: ---- Check submodule -------------------------------------------------------
if not exist "JUCE\CMakeLists.txt" (
    echo WARNING: JUCE submodule appears uninitialised. Running:
    echo   git submodule update --init --recursive
    git submodule update --init --recursive
    if errorlevel 1 (
        echo ERROR: Failed to initialise JUCE submodule.
        popd & exit /b 1
    )
)

:: ---- Configure -------------------------------------------------------------
echo [1/3] Configuring (preset: vs2026-x64) ...
cmake --preset vs2026-x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    popd & exit /b 1
)

:: ---- Build Release ---------------------------------------------------------
echo.
echo [2/3] Building Release ...
cmake --build --preset release
if errorlevel 1 (
    echo ERROR: Build failed.
    popd & exit /b 1
)

:: ---- Package ---------------------------------------------------------------
echo.
echo [3/3] Packaging ...

set "ARTEFACTS=build\vs2026-x64\src\phu-compressor_artefacts\Release\VST3"
set "DIST_DIR=dist"

:: Create dist directory
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

:: Remove old package if it exists
if exist "%DIST_DIR%\%ZIP_NAME%" del /f /q "%DIST_DIR%\%ZIP_NAME%"

:: Locate the .vst3 bundle
set "VST3_PATH="
for /d %%f in ("%ARTEFACTS%\*.vst3") do set "VST3_PATH=%%f"

if not defined VST3_PATH (
    echo ERROR: VST3 bundle not found in %ARTEFACTS%
    popd & exit /b 1
)

echo Found VST3: %VST3_PATH%

:: Use PowerShell to create a zip
powershell -NoProfile -Command ^
    "Compress-Archive -Path '%VST3_PATH%' -DestinationPath '%DIST_DIR%\%ZIP_NAME%' -Force"
if errorlevel 1 (
    echo ERROR: Failed to create zip archive.
    popd & exit /b 1
)

:: ---- Summary ---------------------------------------------------------------
echo.
echo ============================================================
echo  RELEASE PACKAGE CREATED
echo  File    : %DIST_DIR%\%ZIP_NAME%
echo  Contains: PHU COMPRESSOR.vst3
echo.
echo  To install, extract and copy PHU COMPRESSOR.vst3 to:
echo    C:\Program Files\Common Files\VST3\
echo ============================================================
echo.

popd
endlocal
exit /b 0
