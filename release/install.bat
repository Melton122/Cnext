@echo off
setlocal enabledelayedexpansion

set "CNEXT_VERSION=9.0.0"
set "CNEXT_REPO=Melton122/cnext"
set "INSTALL_DIR=C:\Cnext"

echo ========================================
echo   Cnext v9.0 Installer
echo ========================================
echo.

REM Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

REM Detect architecture
set "ARCH=x64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH=arm64"

echo Detected: Windows (%ARCH%)
echo.

REM Check if this is a local install (files in same dir) or remote download
set "LOCAL_INSTALL=0"
if exist "%~dp0cnext.exe" set "LOCAL_INSTALL=1"
if exist "%~dp0include\runtime.h" set "LOCAL_INSTALL=1"

if "%LOCAL_INSTALL%"=="1" (
    echo Installing from local files...
    goto :install_files
)

REM Download from GitHub
echo Downloading Cnext v%CNEXT_VERSION%...
set "FILENAME=cnext-windows-%ARCH%-%CNEXT_VERSION%.zip"
set "URL=https://github.com/%CNEXT_REPO%/releases/download/v%CNEXT_VERSION%/%FILENAME%"

set "TMPDIR=%TEMP%\cnext_install"
if not exist "%TMPDIR%" mkdir "%TMPDIR%"

echo Fetching: %URL%
curl -fsSL -o "%TMPDIR%\%FILENAME%" "%URL%" 2>nul
if %errorlevel% neq 0 (
    echo Download failed. Trying PowerShell...
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%URL%' -OutFile '%TMPDIR%\%FILENAME%'" 2>nul
    if !errorlevel! neq 0 (
        echo ERROR: Download failed.
        echo.
        echo Please download manually from:
        echo   https://github.com/%CNEXT_REPO%/releases
        echo.
        pause
        exit /b 1
    )
)

echo Extracting...
powershell -Command "Expand-Archive -Path '%TMPDIR%\%FILENAME%' -DestinationPath '%TMPDIR%\extract' -Force"
if %errorlevel% neq 0 (
    echo ERROR: Extraction failed.
    pause
    exit /b 1
)

set "SRC_DIR=%TMPDIR%\extract"

:install_files
if "%LOCAL_INSTALL%"=="1" (
    set "SRC_DIR=%~dp0"
)

REM Create directories
echo Installing to %INSTALL_DIR%...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%INSTALL_DIR%\bin" mkdir "%INSTALL_DIR%\bin"
if not exist "%INSTALL_DIR%\include" mkdir "%INSTALL_DIR%\include"

REM Copy compiler
echo Copying compiler...
if "%LOCAL_INSTALL%"=="1" (
    copy /Y "%SRC_DIR%cnext.exe" "%INSTALL_DIR%\bin\" >nul
) else (
    copy /Y "%SRC_DIR%\cnext.exe" "%INSTALL_DIR%\bin\" >nul
)
if errorlevel 1 (
    echo ERROR: Failed to copy compiler.
    pause
    exit /b 1
)

REM Copy headers
echo Copying runtime headers...
xcopy /E /Y /Q "%SRC_DIR%\include\*" "%INSTALL_DIR%\include\" >nul

REM Add to PATH
echo.
echo Adding to PATH...

REM Check if already in PATH
echo %PATH% | findstr /I /C:"%INSTALL_DIR%\bin" >nul
if %errorLevel% neq 0 (
    REM Get current user PATH
    for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USER_PATH=%%b"
    if defined USER_PATH (
        setx PATH "!USER_PATH!;%INSTALL_DIR%\bin" >nul 2>&1
    ) else (
        setx PATH "%INSTALL_DIR%\bin" >nul 2>&1
    )
    if !errorlevel! neq 0 (
        echo Warning: Could not add to PATH automatically.
        echo Please add %INSTALL_DIR%\bin to your PATH manually.
    ) else (
        echo Added to PATH.
    )
) else (
    echo Already in PATH.
)

REM Cleanup
if defined TMPDIR (
    if exist "%TMPDIR%" rmdir /S /Q "%TMPDIR%" 2>nul
)

echo.
echo ========================================
echo   Installation Complete!
echo ========================================
echo.
echo Cnext v9.0 has been installed to: %INSTALL_DIR%
echo.
echo To get started:
echo   1. Open a NEW command prompt
echo   2. Type: cnext version
echo   3. Create hello.cn:
echo      main { printin("Hello, World!") }
echo   4. Run it: cnext run hello.cn
echo.
echo   Install GCC if you don't have it:
echo     winget install GnuWin32.Make
echo     https://gcc.gnu.org/
echo.

pause
