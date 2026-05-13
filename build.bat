@echo off
REM disk-layout-scanner: Windows (MSVC) and optional Linux static binary (Docker).
REM Usage: build.bat | build.bat windows | build.bat linux

cd /d "%~dp0"
if not exist BINARIES mkdir BINARIES
set TARGET=%1
if "%TARGET%"=="" set TARGET=all

REM ===== Clean everything =====
echo === Cleaning ===
if exist build rmdir /s /q build
if exist BINARIES\disk-layout-scanner.exe del /q BINARIES\disk-layout-scanner.exe
if exist BINARIES\disk-layout-scanner del /q BINARIES\disk-layout-scanner
if exist BINARIES\report.html del /q BINARIES\report.html
docker rmi disk-layout-scanner-builder >nul 2>&1
docker rm dit_extract >nul 2>&1
echo [OK] Clean

REM ===== Windows Build =====
if "%TARGET%"=="linux" goto :linux

echo.
echo === Building Windows (MSVC) ===
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 >nul 2>&1
cmake --build build --config Release
if errorlevel 1 (
    echo [FAIL] Windows build failed.
    goto :linux
)
copy /Y build\Release\disk-layout-scanner.exe BINARIES\disk-layout-scanner.exe >nul
echo [OK] BINARIES\disk-layout-scanner.exe

if "%TARGET%"=="windows" goto :done

:linux
REM ===== Linux Build (Docker, no cache) =====
echo.
echo === Building Linux (Alpine musl static) ===
docker --version >nul 2>&1
if errorlevel 1 (
    echo [SKIP] Docker not found. Install Docker Desktop to build Linux binary.
    goto :done
)

docker build --no-cache -t disk-layout-scanner-builder . || (
    echo [FAIL] Docker build failed.
    goto :done
)

docker rm dit_extract >nul 2>&1
docker create --name dit_extract disk-layout-scanner-builder >nul 2>&1
docker cp dit_extract:/disk-layout-scanner BINARIES\disk-layout-scanner
docker rm dit_extract >nul 2>&1
echo [OK] BINARIES\disk-layout-scanner

:done
echo.
echo === Cleanup ===
if exist build rmdir /s /q build
docker rmi disk-layout-scanner-builder >nul 2>&1
docker rm dit_extract >nul 2>&1
echo [OK] Cleanup

echo.
echo === Build complete ===
dir /B BINARIES\disk-layout-scanner* 2>nul || echo No binaries found.
