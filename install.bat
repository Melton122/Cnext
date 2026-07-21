@echo off
REM Cnext Installer - Downloads and installs the latest Cnext compiler
REM Usage: install.bat
REM Or:    iwr -useb https://raw.githubusercontent.com/Melton122/cnext/main/install.bat | iex

setlocal enabledelayedexpansion

set "CNEXT_VERSION=9.0.0"
set "CNEXT_REPO=Melton122/cnext"
set "INSTALL_DIR=%LOCALAPPDATA%\Cnext"

echo ========================================
echo   Cnext v%CNEXT_VERSION% Installer
echo ========================================
echo.

REM Detect architecture
set "ARCH=x64"
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "ARCH=arm64"

echo Detected: Windows (%ARCH%)
echo.

REM Create install directories
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%INSTALL_DIR%\bin" mkdir "%INSTALL_DIR%\bin"
if not exist "%INSTALL_DIR%\include" mkdir "%INSTALL_DIR%\include"

REM Download
set "FILENAME=cnext-windows-%ARCH%-%CNEXT_VERSION%.zip"
set "URL=https://github.com/%CNEXT_REPO%/releases/download/v%CNEXT_VERSION%/%FILENAME%"
set "TMPDIR=%TEMP%\cnext_install_%RANDOM%"

echo Downloading Cnext v%CNEXT_VERSION%...
if not exist "%TMPDIR%" mkdir "%TMPDIR%"

curl -fsSL -o "%TMPDIR%\%FILENAME%" "%URL%" 2>nul
if %errorlevel% neq 0 (
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%URL%' -OutFile '%TMPDIR%\%FILENAME%'" 2>nul
    if !errorlevel! neq 0 (
        echo ERROR: Download failed.
        echo.
        echo Please download manually from:
        echo   https://github.com/%CNEXT_REPO%/releases
        pause
        exit /b 1
    )
)
echo Download complete.

echo Extracting...
powershell -Command "Expand-Archive -Path '%TMPDIR%\%FILENAME%' -DestinationPath '%TMPDIR%\extract' -Force" 2>nul
if %errorlevel% neq 0 (
    echo ERROR: Extraction failed.
    pause
    exit /b 1
)

echo Installing...
copy /Y "%TMPDIR%\extract\cnext.exe" "%INSTALL_DIR%\bin\" >nul
xcopy /E /Y /Q "%TMPDIR%\extract\include\*" "%INSTALL_DIR%\include\" >nul 2>nul

REM Add to PATH
echo %PATH% | findstr /I /C:"%INSTALL_DIR%\bin" >nul
if %errorLevel% neq 0 (
    for /f "tokens=2*" %%a in ('reg query "HKCU\Environment" /v Path 2^>nul') do set "USER_PATH=%%b"
    if defined USER_PATH (
        setx PATH "!USER_PATH!;%INSTALL_DIR%\bin" >nul 2>&1
    ) else (
        setx PATH "%INSTALL_DIR%\bin" >nul 2>&1
    )
    echo Added to PATH.
) else (
    echo Already in PATH.
)

REM Cleanup
if exist "%TMPDIR%" rmdir /S /Q "%TMPDIR%" 2>nul

echo.
echo ========================================
echo   Installation Complete!
echo ========================================
echo.
echo Cnext v%CNEXT_VERSION% installed to: %INSTALL_DIR%\bin
echo.
echo Get started:
echo   1. Open a NEW command prompt
echo   2. cnext version
echo   3. cnext new my_app
echo   4. cd my_app ^&^& cnext run src/main.cn
echo.
echo VS Code Extension:
echo   code --install-extension vscode-extension/cnext-3.0.0.vsix
echo   Or search "Cnext" in VS Code extensions
echo.
echo Prerequisites:
echo   - GCC: winget install GCC.QT | https://gcc.gnu.org/
echo   - Make: winget install GnuWin32.Make | choco install make
echo.

pause
