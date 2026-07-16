@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   Cnext v9.0 Uninstaller
echo ========================================
echo.

REM Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set "INSTALL_DIR=C:\Cnext"

echo Uninstalling Cnext from %INSTALL_DIR%...
echo.

REM Remove from PATH
echo Removing from PATH...
for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "CURRENT_PATH=%%b"

REM Remove Cnext from PATH (handle all ordering cases)
set "NEW_PATH=!CURRENT_PATH:;%INSTALL_DIR%\bin=!"
set "NEW_PATH=!NEW_PATH:%INSTALL_DIR%\bin;=!"
set "NEW_PATH=!NEW_PATH:%INSTALL_DIR%\bin=!"

if not "!NEW_PATH!"=="!CURRENT_PATH!" (
    setx PATH "!NEW_PATH!" /M >nul 2>&1
    echo Removed from PATH.
) else (
    echo Cnext was not in PATH.
)

REM Remove installation directory
echo.
echo Removing installation directory...
if exist "%INSTALL_DIR%" (
    rmdir /S /Q "%INSTALL_DIR%"
    echo Removed %INSTALL_DIR%
) else (
    echo Installation directory not found.
)

echo.
echo ========================================
echo   Uninstallation Complete!
echo ========================================
echo.
echo Cnext has been removed from your system.
echo Please restart any open command prompts.
echo.

pause
