#!/bin/bash
set -e

CNEXT_VERSION="${CNEXT_VERSION:-9.0.0}"
CNEXT_REPO="Melton122/cnext"
INSTALL_DIR="${CNEXT_INSTALL_DIR:-/usr/local}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}>>> $1${NC}"; }
ok()    { echo -e "${GREEN}>>> $1${NC}"; }
warn()  { echo -e "${YELLOW}>>> $1${NC}"; }
err()   { echo -e "${RED}>>> $1${NC}" >&2; }

detect_platform() {
    local os arch

    case "$(uname -s)" in
        Linux*)     os="linux" ;;
        Darwin*)    os="macos" ;;
        MINGW*|MSYS*|CYGWIN*) os="windows" ;;
        *)
            err "Unsupported OS: $(uname -s)"
            err "Supported: Linux, macOS, Windows"
            exit 1
            ;;
    esac

    case "$(uname -m)" in
        x86_64|amd64)   arch="x64" ;;
        aarch64|arm64)   arch="arm64" ;;
        armv7l|armhf)    arch="arm64"; warn "ARM32 detected, using ARM64 build" ;;
        *)
            err "Unsupported architecture: $(uname -m)"
            err "Supported: x86_64 (x64), aarch64/arm64"
            exit 1
            ;;
    esac

    echo "${os}-${arch}"
}

download_file() {
    local url="$1"
    local dest="$2"

    if command -v curl &>/dev/null; then
        curl -fsSL -o "$dest" "$url"
    elif command -v wget &>/dev/null; then
        wget -q -O "$dest" "$url"
    else
        err "Neither curl nor wget found. Install one and try again."
        exit 1
    fi
}

check_gcc() {
    if ! command -v gcc &>/dev/null; then
        warn "GCC not found. Cnext needs GCC to compile your code."
        echo ""
        echo "  Install it with:"
        echo "    Ubuntu/Debian:  sudo apt install gcc"
        echo "    Fedora/RHEL:    sudo dnf install gcc"
        echo "    Arch:           sudo pacman -S gcc"
        echo "    macOS:          xcode-select --install"
        echo ""
        warn "Cnext is installed but won't compile without GCC."
        return 1
    fi
    return 0
}

install_cnext() {
    local platform
    platform=$(detect_platform)

    local os="${platform%-*}"
    local arch="${platform#*-}"

    info "Detected: $os ($arch)"
    echo ""

    local ext=""
    local archive_ext="tar.gz"
    if [ "$os" = "windows" ]; then
        ext=".exe"
        archive_ext="zip"
    fi

    local filename="cnext-${os}-${arch}-${CNEXT_VERSION}.${archive_ext}"
    local url="https://github.com/${CNEXT_REPO}/releases/download/v${CNEXT_VERSION}/${filename}"

    info "Downloading Cnext v${CNEXT_VERSION}..."
    local tmpdir
    tmpdir=$(mktemp -d)
    trap "rm -rf $tmpdir" EXIT

    local archive_path="${tmpdir}/${filename}"
    download_file "$url" "$archive_path" || {
        err "Download failed."
        err "URL: $url"
        err ""
        err "If this is a new release, the binaries may still be building."
        err "Check: https://github.com/${CNEXT_REPO}/releases"
        exit 1
    }
    ok "Downloaded ${filename}"

    info "Extracting..."
    mkdir -p "${tmpdir}/extract"
    if [ "$archive_ext" = "zip" ]; then
        unzip -q -o "$archive_path" -d "${tmpdir}/extract"
    else
        tar -xzf "$archive_path" -C "${tmpdir}/extract"
    fi

    local bin_dir="${INSTALL_DIR}/bin"
    local include_dir="${INSTALL_DIR}/include"

    info "Installing to ${INSTALL_DIR}..."
    sudo mkdir -p "$bin_dir" 2>/dev/null || mkdir -p "$bin_dir"
    sudo mkdir -p "$include_dir" 2>/dev/null || mkdir -p "$include_dir"

    sudo cp "${tmpdir}/extract/cnext${ext}" "${bin_dir}/cnext${ext}"
    sudo chmod +x "${bin_dir}/cnext${ext}"
    sudo cp -r "${tmpdir}/extract/include/"* "${include_dir}/" 2>/dev/null || true

    if [ -d "${tmpdir}/extract/examples" ]; then
        sudo mkdir -p "${INSTALL_DIR}/share/cnext/examples" 2>/dev/null || true
        sudo cp -r "${tmpdir}/extract/examples/"* "${INSTALL_DIR}/share/cnext/examples/" 2>/dev/null || true
    fi

    ok "Installed cnext to ${bin_dir}/cnext${ext}"

    check_gcc

    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  Cnext v${CNEXT_VERSION} installed!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "  Quick start:"
    echo "    cnext version"
    echo "    cnext new my_app"
    echo "    cd my_app"
    echo "    cnext run src/main.cn"
    echo ""
    echo "  Docs: https://github.com/${CNEXT_REPO}"
    echo ""

    local path_warn=false
    case ":$PATH:" in
        *":${bin_dir}:"*) ;;
        *) path_warn=true ;;
    esac

    if [ "$path_warn" = true ]; then
        warn "${bin_dir} is not in your PATH."
        echo ""
        echo "  Add it with:"
        echo "    export PATH=\"${bin_dir}:\$PATH\""
        echo ""
        echo "  Or add to ~/.bashrc for persistence:"
        echo "    echo 'export PATH=\"${bin_dir}:\$PATH\"' >> ~/.bashrc"
        echo ""
    fi
}

uninstall_cnext() {
    info "Removing Cnext..."
    sudo rm -f "${INSTALL_DIR}/bin/cnext" "${INSTALL_DIR}/bin/cnext.exe" 2>/dev/null || true
    sudo rm -rf "${INSTALL_DIR}/include/runtime.h" 2>/dev/null || true
    sudo rm -rf "${INSTALL_DIR}/share/cnext" 2>/dev/null || true
    ok "Cnext removed."
}

show_help() {
    echo "Cnext Installer v${CNEXT_VERSION}"
    echo ""
    echo "Usage:"
    echo "  curl -fsSL https://raw.githubusercontent.com/${CNEXT_REPO}/main/install.sh | bash"
    echo ""
    echo "Or clone and run locally:"
    echo "  git clone https://github.com/${CNEXT_REPO}.git"
    echo "  cd cnext"
    echo "  bash install.sh"
    echo ""
    echo "Options:"
    echo "  install       Install Cnext (default)"
    echo "  uninstall     Remove Cnext"
    echo "  --help        Show this help"
}

case "${1:-install}" in
    install)    install_cnext ;;
    uninstall)  uninstall_cnext ;;
    --help|-h)  show_help ;;
    *)          err "Unknown command: $1"; show_help; exit 1 ;;
esac
