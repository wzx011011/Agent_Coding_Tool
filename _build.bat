@echo off
REM ============================================================
REM  ACT Build Script — Windows MSVC + Qt 6.10.2 + vcpkg
REM
REM  Usage:
REM    _build.bat              Configure + Build + Test (default)
REM    _build.bat --no-test    Configure + Build only
REM    _build.bat --test-only  Run tests only (assumes already built)
REM    _build.bat --configure  Configure only
REM    _build.bat --build      Build only (skip configure)
REM    _build.bat --timeout N  Set per-test timeout in seconds (default 10)
REM ============================================================

set "DO_CONFIGURE=1"
set "DO_BUILD=1"
set "DO_TEST=1"
set "TEST_TIMEOUT=10"

if "%~1"=="--no-test" set "DO_TEST=0"
if "%~1"=="--test-only" (
    set "DO_CONFIGURE=0"
    set "DO_BUILD=0"
)
if "%~1"=="--configure" (
    set "DO_BUILD=0"
    set "DO_TEST=0"
)
if "%~1"=="--build" (
    set "DO_CONFIGURE=0"
    set "DO_TEST=0"
)
if "%~1"=="--timeout" set "TEST_TIMEOUT=%~2"

REM --- MSVC Environment ---
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM --- Toolchain paths ---
set "VCPKG_ROOT=E:\vcpkg"
set "HTTP_PROXY=http://127.0.0.1:7897"
set "HTTPS_PROXY=http://127.0.0.1:7897"
set "PATH=E:\Qt6.10\bin;C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;E:\ai\Agent_Coding_Tool\build\vcpkg_installed\x64-windows\bin;%PATH%"

cd /d E:\ai\Agent_Coding_Tool

echo === ACT Build Script ===
echo === VCPKG_ROOT=%VCPKG_ROOT% ===
echo === PROXY=%HTTPS_PROXY% ===

REM --- Configure ---
if "%DO_CONFIGURE%"=="1" (
    echo === CONFIGURE ===
    cmake --preset default
    if %ERRORLEVEL% neq 0 (
        echo CONFIGURE FAILED
        exit /b %ERRORLEVEL%
    )
)

REM --- Build ---
if "%DO_BUILD%"=="1" (
    echo === BUILD ===
    cmake --build build --config RelWithDebInfo
    if %ERRORLEVEL% neq 0 (
        echo BUILD FAILED
        exit /b %ERRORLEVEL%
    )
)

REM --- Test ---
if "%DO_TEST%"=="1" (
    echo === TEST (timeout=%TEST_TIMEOUT%s) ===
    ctest --test-dir build --output-on-failure --timeout %TEST_TIMEOUT%
)
