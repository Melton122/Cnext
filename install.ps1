# Cnext Installer for Windows (PowerShell)
# Usage: iwr -useb https://raw.githubusercontent.com/Melton122/Cnext/main/install.ps1 | iex

$ErrorActionPreference = "Stop"

$CNEXT_VERSION = "10.0.5"
$CNEXT_REPO = "Melton122/Cnext"
$INSTALL_DIR = "$env:LOCALAPPDATA\Cnext"
$INSTALL_BIN = Join-Path $INSTALL_DIR "bin"
$INSTALL_INCLUDE = Join-Path $INSTALL_DIR "include"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Cnext v$CNEXT_VERSION Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ARCH = "x64"
if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { $ARCH = "arm64" }

Write-Host "Detected: Windows ($ARCH)" -ForegroundColor Green
Write-Host ""

New-Item -ItemType Directory -Force -Path $INSTALL_DIR | Out-Null
New-Item -ItemType Directory -Force -Path $INSTALL_BIN | Out-Null
New-Item -ItemType Directory -Force -Path $INSTALL_INCLUDE | Out-Null

$FILENAME = "cnext-windows-$ARCH-v$CNEXT_VERSION.zip"
$URL = "https://github.com/$CNEXT_REPO/releases/download/v$CNEXT_VERSION/$FILENAME"
$TMPDIR = Join-Path $env:TEMP ("cnext_install_" + [guid]::NewGuid().ToString("N"))
$ARCHIVE = Join-Path $TMPDIR $FILENAME
$EXTRACT_DIR = Join-Path $TMPDIR "extract"

Write-Host "Downloading Cnext v$CNEXT_VERSION..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $TMPDIR | Out-Null

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    try {
        Invoke-WebRequest -Uri $URL -OutFile $ARCHIVE -UseBasicParsing
    } catch {
        Write-Host "ERROR: Download failed." -ForegroundColor Red
        Write-Host "URL: $URL" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please download manually from:" -ForegroundColor Yellow
        Write-Host "  https://github.com/$CNEXT_REPO/releases" -ForegroundColor Yellow
        throw
    }

    Write-Host "Download complete." -ForegroundColor Green

    Write-Host "Extracting..." -ForegroundColor Yellow
    Expand-Archive -LiteralPath $ARCHIVE -DestinationPath $EXTRACT_DIR -Force

    Write-Host "Installing..." -ForegroundColor Yellow

    $cnextExe = Get-ChildItem -LiteralPath $EXTRACT_DIR -Filter "cnext.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cnextExe) {
        Write-Host "ERROR: cnext.exe not found in downloaded archive." -ForegroundColor Red
        Write-Host "Contents of archive:" -ForegroundColor Red
        Get-ChildItem -LiteralPath $EXTRACT_DIR -Recurse | ForEach-Object { Write-Host "  $($_.FullName.Replace("$EXTRACT_DIR\", ''))" -ForegroundColor Red }
        throw "cnext.exe not found in downloaded archive."
    }
    Write-Host "Found: $($cnextExe.FullName)" -ForegroundColor Green
    Copy-Item -LiteralPath $cnextExe.FullName -Destination (Join-Path $INSTALL_BIN "cnext.exe") -Force

    $includeDir = Get-ChildItem -LiteralPath $EXTRACT_DIR -Filter "include" -Directory -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($includeDir) {
        Get-ChildItem -LiteralPath $includeDir.FullName -Force | Copy-Item -Destination $INSTALL_INCLUDE -Recurse -Force
    }

    $CurrentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($CurrentPath -notlike "*$INSTALL_BIN*") {
        [Environment]::SetEnvironmentVariable("Path", "$CurrentPath;$INSTALL_BIN", "User")
        $env:Path = "$env:Path;$INSTALL_BIN"
        Write-Host "Added to PATH." -ForegroundColor Green
    } else {
        Write-Host "Already in PATH." -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  Installation Complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Cnext v$CNEXT_VERSION installed to: $INSTALL_BIN" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Get started:" -ForegroundColor Yellow
    Write-Host "  1. Open a NEW PowerShell or Command Prompt window"
    Write-Host "  2. cnext version"
    Write-Host "  3. cnext new my_app"
    Write-Host "  4. cd my_app && cnext run src/main.cn"
    Write-Host ""
    Write-Host "VS Code Extension:" -ForegroundColor Yellow
    Write-Host "  code --install-extension cnext"
    Write-Host "  Or search 'Cnext' in VS Code extensions"
    Write-Host ""
    Write-Host "Prerequisites:" -ForegroundColor Yellow
    Write-Host "  - GCC: https://gcc.gnu.org/ or winget install GCC.QT"
    Write-Host "  - Make: winget install GnuWin32.Make"
    Write-Host ""
}
finally {
    if (Test-Path -LiteralPath $TMPDIR) {
        Remove-Item -LiteralPath $TMPDIR -Recurse -Force -ErrorAction SilentlyContinue
    }
}
