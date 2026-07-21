# Installation

## Quick Install (One Command)

### Linux / macOS

```bash
curl -fsSL https://raw.githubusercontent.com/Melton122/cnext/main/install.sh | bash
```

### Windows (PowerShell)

```powershell
iwr -useb https://raw.githubusercontent.com/Melton122/Cnext/main/install.ps1 | iex
```

Or download `install.bat` from the [Releases page](https://github.com/Melton122/cnext/releases) and double-click it.

## Manual Install

### Pre-built Release (Recommended)

Download the latest release for your platform from the [Releases page](https://github.com/Melton122/cnext/releases):

| Platform | Architecture | File |
|----------|-------------|------|
| Linux | x86_64 | `cnext-linux-x64-v9.0.0.tar.gz` |
| Linux | ARM64 | `cnext-linux-arm64-v9.0.0.tar.gz` |
| macOS | x86_64 | `cnext-macos-x64-v9.0.0.tar.gz` |
| macOS | ARM64 (Apple Silicon) | `cnext-macos-arm64-v9.0.0.tar.gz` |
| Windows | x86_64 | `cnext-windows-x64-v9.0.0.zip` |

Then extract and install:

**Linux/macOS:**
```bash
tar -xzf cnext-linux-x64-v9.0.0.tar.gz
sudo cp cnext /usr/local/bin/
sudo cp -r include/* /usr/local/include/
```

**Windows:**
1. Extract the `.zip` file
2. Run `install.bat` as Administrator
3. Open a new terminal

## Building from Source

### Prerequisites

- GCC or Clang (C11 support)
- Make
- libcurl (Linux/macOS, for networking)

### Linux

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
sudo apt-get install libcurl4-openssl-dev  # for networking
make
sudo make install
```

### macOS

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
brew install curl  # for networking
make
make install
```

### Windows (MSYS2)

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
make
```

## Build Options

```bash
make              # Release build (optimized)
make DEBUG=1      # Debug build (with symbols)
make clean        # Clean build artifacts
make install      # Install to /usr/local/bin
make release      # Package for release
make uninstall    # Remove from /usr/local/bin
make help         # Show all available targets
```

## Verifying Installation

After installing, verify it works:

```bash
cnext version
```

Output should show:
```
Cnext version 9.0.0
```

## VS Code Extension

Install the Cnext extension for syntax highlighting, snippets, and build tasks:

```bash
cd vscode-extension
npm install -g @vscode/vsce
vsce package
code --install-extension cnext-3.1.0.vsix
```

Or search for "Cnext" in the VS Code Extensions panel (Ctrl+Shift+X).

Features:
- Syntax highlighting for `.cn` files
- Code snippets for common patterns
- Run, build, format, and lint commands
- Keyboard shortcuts (Ctrl+Shift+R to run, Ctrl+Shift+B to build)
- Auto-run and auto-format on save

See [vscode-extension/README.md](../vscode-extension/README.md) for the full command reference.

## Adding to PATH

### Linux/macOS

The installer and `make install` put Cnext in `/usr/local/bin` by default.

To install to a custom location:
```bash
make install INSTALL_DIR=/opt/cnext
export PATH=$PATH:/opt/cnext/bin
```

Or add to your `~/.bashrc` or `~/.zshrc`:
```bash
export PATH=$PATH:/usr/local/bin
```

Then reload:
```bash
source ~/.bashrc
```

### Windows

The installer adds Cnext to your PATH automatically. If it doesn't work:

1. Right-click "This PC" -> Properties -> Advanced system settings
2. Click "Environment Variables"
3. Under "System variables", find "Path" and click "Edit"
4. Add `C:\Cnext\bin`
5. Click OK

## Running Your First Program

Create a file `hello.cn`:

```cnext
main {
    printin("Hello, World!")
}
```

Run it:

```bash
cnext run hello.cn
```

## Troubleshooting

### "runtime.h: No such file or directory"

The `include/` directory wasn't installed. Reinstall:
```bash
# From release
sudo cp -r include/* /usr/local/include/

# From source
sudo make install
```

### "gcc: command not found"

Install GCC:
- **Linux**: `sudo apt install gcc` or `sudo dnf install gcc`
- **macOS**: `xcode-select --install`
- **Windows**: Install [MSYS2](https://www.msys2.org/) or [MinGW](https://mingw-w64.org/)

### "make: command not found"

Install Make:
- **Linux**: `sudo apt install make`
- **macOS**: `xcode-select --install`
- **Windows**: Install [MSYS2](https://www.msys2.org/)

### Permission Denied

Make the executable executable:
```bash
chmod +x cnext
```

### Windows Defender Warning

If Windows Defender blocks the executable:
1. Click "More info"
2. Click "Run anyway"

### curl/curl.h not found (Linux/macOS)

Install libcurl development headers:
- **Linux**: `sudo apt install libcurl4-openssl-dev`
- **macOS**: `brew install curl`
