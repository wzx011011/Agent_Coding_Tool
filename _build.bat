@echo off
set "VCPKG_ROOT=E:\vcpkg"
set "HTTP_PROXY=http://127.0.0.1:7897"
set "HTTPS_PROXY=http://127.0.0.1:7897"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set "VCPKG_ROOT=E:\vcpkg"
set "HTTP_PROXY=http://127.0.0.1:7897"
set "HTTPS_PROXY=http://127.0.0.1:7897"
set "PATH=E:\Qt6.10\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;E:\ai\Agent_Coding_Tool\build\vcpkg_installed\x64-windows\bin;%PATH%"
cd /d E:\ai\Agent_Coding_Tool
echo === VCPKG_ROOT=%VCPKG_ROOT% ===
echo === PROXY=%HTTPS_PROXY% ===
echo === NINJA ===
ninja --version
echo === CMAKE ===
cmake --version
echo === CONFIGURE ===
cmake --preset default
if %ERRORLEVEL% neq 0 (
    echo CONFIGURE FAILED
    exit /b %ERRORLEVEL%
)
echo === BUILD ===
cmake --build build --config RelWithDebInfo
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    exit /b %ERRORLEVEL%
)
echo === TEST ===
ctest --test-dir build --output-on-failure
