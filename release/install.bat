@echo off
setlocal enabledelayedexpansion

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

REM Set installation directory
set "INSTALL_DIR=C:\Cnext"

echo Installing Cnext to %INSTALL_DIR%...
echo.

REM Create installation directory
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%INSTALL_DIR%\bin" mkdir "%INSTALL_DIR%\bin"
if not exist "%INSTALL_DIR%\include" mkdir "%INSTALL_DIR%\include"
if not exist "%INSTALL_DIR%\examples" mkdir "%INSTALL_DIR%\examples"
if not exist "%INSTALL_DIR%\docs" mkdir "%INSTALL_DIR%\docs"

REM Copy compiler
echo Copying compiler...
copy /Y "%~dp0cnext.exe" "%INSTALL_DIR%\bin\" >nul
if errorlevel 1 (
    echo Error copying compiler!
    pause
    exit /b 1
)

REM Copy runtime headers
echo Copying runtime headers...
xcopy /E /Y /Q "%~dp0include\*" "%INSTALL_DIR%\include\" >nul

REM Copy examples
echo Copying examples...
xcopy /E /Y /Q "%~dp0examples\*" "%INSTALL_DIR%\examples\" >nul

REM Copy documentation
echo Copying documentation...
xcopy /E /Y /Q "%~dp0docs\*" "%INSTALL_DIR%\docs\" >nul

REM Copy README and LICENSE
copy /Y "%~dp0README.md" "%INSTALL_DIR%\" >nul
copy /Y "%~dp0LICENSE" "%INSTALL_DIR%\" >nul

REM Add to PATH
echo.
echo Adding Cnext to PATH...

REM Get current PATH
for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "CURRENT_PATH=%%b"

REM Check if already in PATH
echo !CURRENT_PATH! | findstr /I /C:"%INSTALL_DIR%\bin" >nul
if %errorLevel% neq 0 (
    REM Add to system PATH
    setx PATH "!CURRENT_PATH!;%INSTALL_DIR%\bin" /M >nul 2>&1
    if errorlevel 1 (
        echo Warning: Could not add to system PATH automatically.
        echo Please add %INSTALL_DIR%\bin to your PATH manually.
    ) else (
        echo Added to PATH successfully!
    )
) else (
    echo Already in PATH.
)

echo.
echo ========================================
echo   Installation Complete!
echo ========================================
echo.
echo Cnext has been installed to: %INSTALL_DIR%
echo.
echo To get started:
echo   1. Open a new command prompt
echo   2. Type: cnext version
echo   3. Create a file called hello.cn:
echo      main { printin("Hello, World!") }
echo   4. Run it: cnext run hello.cn
echo.

pause
