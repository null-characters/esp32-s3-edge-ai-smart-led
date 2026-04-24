@echo off
setlocal enabledelayedexpansion

echo === Zephyr Unit Test Build ===

:: Set PATH
set "PATH=E:\svn\trunk\software\Code\devtools\msys64\mingw64\bin;%PATH%"

:: Set Zephyr environment
set "ZEPHYR_BASE=E:\zephyrproject\zephyr"
set "ZEPHYR_SDK_INSTALL_DIR=E:\zephyrproject\zephyr-sdk-0.17.0"

:: Verify GCC
echo [1/3] Checking GCC...
gcc --version
if errorlevel 1 (
    echo ERROR: GCC not found!
    exit /b 1
)

:: Enter test directory
echo [2/3] Entering test directory...
cd /d E:\zephyrproject\prj\tests\unit
if errorlevel 1 (
    echo ERROR: Cannot enter test directory!
    exit /b 1
)

:: Clean build
echo [3/3] Building...
if exist build rmdir /s /q build
west build -b native_sim --pristine 2>&1

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    echo Check error messages above.
) else (
    echo.
    echo SUCCESS: Build completed!
    echo Run tests with: west build -t run
)

pause
