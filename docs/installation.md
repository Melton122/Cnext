# Installation

## Prerequisites

- **GCC** or **Clang** compiler (with C11 support)
- **Make** (Linux/macOS) or **MinGW** (Windows)
- **Git** (for cloning the repository)
- **libcurl** (Linux/macOS, for networking features)

## Building from Source

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

### Windows (MinGW)

```bash
git clone https://github.com/Melton122/cnext.git
cd cnext
mingw32-make
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
make uninstall    # Remove from /usr/local/bin
make format       # Format source code
make help         # Show all available targets
```

## Verifying Installation

After building, verify the installation:

```bash
./cnext version
```

Output should show:
```
Cnext version 9.0
```

## Adding to PATH

### Linux/macOS

The `make install` command installs to `/usr/local/bin` by default.

To install to a custom location:
```bash
make install DESTDIR=/opt/cnext
export PATH=$PATH:/opt/cnext/bin
```

Or add to your `~/.bashrc` or `~/.zshrc`:
```bash
export PATH=$PATH:/path/to/cnext
```

Then reload:
```bash
source ~/.bashrc
```

### Windows

1. Right-click "This PC" -> Properties -> Advanced system settings
2. Click "Environment Variables"
3. Under "System variables", find "Path" and click "Edit"
4. Add the Cnext directory
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

### "gcc: command not found"

Install GCC:
- **Linux**: `sudo apt install gcc` or `sudo yum install gcc`
- **macOS**: `xcode-select --install`
- **Windows**: Install [MinGW](https://mingw-w64.org/) or [MSYS2](https://www.msys2.org/)

### "make: command not found"

Install Make:
- **Linux**: `sudo apt install make` or `sudo yum install make`
- **macOS**: `xcode-select --install`
- **Windows**: Install [MinGW](https://mingw-w64.org/) or [MSYS2](https://www.msys2.org/)

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
