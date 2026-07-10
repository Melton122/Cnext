@echo off
setlocal enabledelayedexpansion

set "CNEXT=cnext.exe"
set "OUT=out.exe"
set "PASS=0"
set "FAIL=0"

:: Check if cnext.exe exists
if not exist "%CNEXT%" (
    echo ERROR: cnext.exe not found. Build the compiler first with 'make' or 'mingw32-make'.
    exit /b 1
)

:: Check if gcc is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo WARNING: gcc not found in PATH. Attempting to auto-detect MSYS2/UCRT64...
    set "PATH=C:\msys64\ucrt64\bin;%PATH%"
    gcc --version >nul 2>&1
    if errorlevel 1 (
        echo ERROR: gcc not found. Please install MSYS2/UCRT64 or add gcc to your PATH.
        exit /b 1
    )
    echo INFO: Auto-detected gcc at C:\msys64\ucrt64\bin
)

echo =========================================
echo Cnext Test Suite
echo =========================================
echo.

:: --- Tests ---
for %%f in (tests\*.cn) do (
    set "name=%%f"
    echo --- Testing %%f ---
    %CNEXT% build "%%f" 2>&1
    if errorlevel 1 (
        echo [FAIL] %%f
        set /a FAIL+=1
    ) else (
        echo [PASS] %%f
        set /a PASS+=1
    )
)

:: --- Examples ---
for %%f in (examples\*.cn) do (
    set "name=%%f"
    echo --- Testing %%f ---
    %CNEXT% build "%%f" 2>&1
    if errorlevel 1 (
        echo [FAIL] %%f
        set /a FAIL+=1
    ) else (
        echo [PASS] %%f
        set /a PASS+=1
    )
)

echo.
echo =========================================
echo Results: %PASS% passed, %FAIL% failed
echo =========================================

if %FAIL% gtr 0 (
    echo Some tests failed.
    exit /b 1
) else (
    echo All tests passed!
    exit /b 0
)
