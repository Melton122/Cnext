# Cnext Installer for Windows (PowerShell)
# Usage: iwr -useb https://raw.githubusercontent.com/Melton122/Cnext/main/install.ps1 | iex

$ErrorActionPreference = "Stop"

$CNEXT_VERSION = "9.0.0"
$CNEXT_REPO = "Melton122/Cnext"
$INSTALL_DIR = "$env:LOCALAPPDATA\Cnext"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Cnext v$CNEXT_VERSION Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Detect architecture
$ARCH = "x64"
if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { $ARCH = "arm64" }

Write-Host "Detected: Windows ($ARCH)" -ForegroundColor Green
Write-Host ""

# Create directories
New-Item -ItemType Directory -Force -Path $INSTALL_DIR | Out-Null
New-Item -ItemType Directory -Force -Path "$INSTALL_DIR\bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$INSTALL_DIR\include" | Out-Null

# Download
$FILENAME = "cnext-windows-$ARCH-$CNEXT_VERSION.zip"
$URL = "https://github.com/$CNEXT_REPO/releases/download/v$CNEXT_VERSION/$FILENAME"
$TMPDIR = "$env:TEMP\cnext_install_$([System.IO.Path]::GetRandomFileName())"

Write-Host "Downloading Cnext v$CNEXT_VERSION..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $TMPDIR | Out-Null

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $URL -OutFile "$TMPDIR\$FILENAME" -UseBasicParsing
} catch {
    Write-Host "ERROR: Download failed." -ForegroundColor Red
    Write-Host "URL: $URL" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please download manually from:" -ForegroundColor Yellow
    Write-Host "  https://github.com/$CNEXT_REPO/releases" -ForegroundColor Yellow
    exit 1
}

Write-Host "Download complete." -ForegroundColor Green

# Extract
Write-Host "Extracting..." -ForegroundColor Yellow
Expand-Archive -Path "$TMPDIR\$FILENAME" -DestinationPath "$TMPDIR\extract" -Force

# Install
Write-Host "Installing..." -ForegroundColor Yellow

$cnextExe = Get-ChildItem -Path "$TMPDIR\extract" -Filter "cnext.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $cnextExe) {
    Write-Host "ERROR: cnext.exe not found in downloaded archive." -ForegroundColor Red
    Write-Host "Contents of archive:" -ForegroundColor Red
    Get-ChildItem -Path "$TMPDIR\extract" -Recurse | ForEach-Object { Write-Host "  $($_.FullName.Replace("$TMPDIR\extract\", ''))" -ForegroundColor Red }
    exit 1
}
Write-Host "Found: $($cnextExe.FullName)" -ForegroundColor Green
Copy-Item -Force $cnextExe.FullName "$INSTALL_DIR\bin\"

$includeDir = Get-ChildItem -Path "$TMPDIR\extract" -Filter "include" -Directory -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if ($includeDir) {
    Copy-Item -Recurse -Force "$($includeDir.FullName)\*" "$INSTALL_DIR\include\"
}

# Add to PATH
$CurrentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($CurrentPath -notlike "*$INSTALL_DIR\bin*") {
    [Environment]::SetEnvironmentVariable("Path", "$CurrentPath;$INSTALL_DIR\bin", "User")
    $env:Path = "$env:Path;$INSTALL_DIR\bin"
    Write-Host "Added to PATH." -ForegroundColor Green
} else {
    Write-Host "Already in PATH." -ForegroundColor Green
}

# Cleanup
Remove-Item -Recurse -Force $TMPDIR -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Installation Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Cnext v$CNEXT_VERSION installed to: $INSTALL_DIR\bin" -ForegroundColor Cyan
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
