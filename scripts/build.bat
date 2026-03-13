@echo off
:: ---------------------------------------------------------------------------
:: build.bat — Quick Windows build script for phu-compressor
::
:: Usage:
::   scripts\build.bat              (Release, default)
::   scripts\build.bat debug        (Debug)
::
:: Output:
::   build\vs2026-x64\src\phu-compressor_artefacts\<Config>\VST3\PHU COMPRESSOR.vst3
::
:: Requirements:
::   - CMake 3.23+ on PATH
::   - Visual Studio 2022 (or 2026) with Desktop C++ workload installed
::   - JUCE submodule initialised:  git submodule update --init --recursive
:: ---------------------------------------------------------------------------
setlocal EnableExtensions EnableDelayedExpansion

:: ---- Parse optional argument -----------------------------------------------
set "CONFIG=Release"
set "BUILD_PRESET=release"
if /I "%~1"=="debug" (
    set "CONFIG=Debug"
    set "BUILD_PRESET=debug"
)

echo.
echo ============================================================
echo  phu-compressor ^| Windows Build
echo  Configuration : %CONFIG%
echo ============================================================
echo.

:: ---- Check CMake -----------------------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not found on PATH.
    echo Please install CMake 3.23+ and add it to your PATH.
    exit /b 1
)
for /f "tokens=3" %%v in ('cmake --version 2^>^&1 ^| findstr /r "cmake version"') do set "CMAKE_VER=%%v"
echo CMake version: %CMAKE_VER%

:: ---- Navigate to repo root -------------------------------------------------
pushd "%~dp0\.."

:: ---- Check submodule -------------------------------------------------------
if not exist "JUCE\CMakeLists.txt" (
    echo.
    echo WARNING: JUCE submodule appears uninitialised. Running:
    echo   git submodule update --init --recursive
    echo.
    git submodule update --init --recursive
    if errorlevel 1 (
        echo ERROR: Failed to initialise JUCE submodule.
        popd & exit /b 1
    )
)

:: ---- Configure -------------------------------------------------------------
echo.
echo [1/2] Configuring (preset: vs2026-x64) ...
cmake --preset vs2026-x64
if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed.
    popd & exit /b 1
)

:: ---- Build -----------------------------------------------------------------
echo.
echo [2/2] Building (preset: %BUILD_PRESET%) ...
cmake --build --preset %BUILD_PRESET%
if errorlevel 1 (
    echo.
    echo ERROR: Build failed.
    popd & exit /b 1
)

:: ---- Locate output ---------------------------------------------------------
echo.
set "ARTEFACTS=build\vs2026-x64\src\phu-compressor_artefacts\%CONFIG%\VST3"
for /d %%f in ("%ARTEFACTS%\*.vst3") do set "VST3_PATH=%%f"

echo ============================================================
echo  BUILD SUCCEEDED
if defined VST3_PATH (
    echo  Output : %VST3_PATH%
) else (
    echo  Output : %ARTEFACTS%\
)
echo ============================================================
echo.

popd
endlocal
exit /b 0
