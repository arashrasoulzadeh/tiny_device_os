#!/usr/bin/env bash
# Install host build, test, and USB-flash dependencies on a new machine.
#
# macOS and Debian/Ubuntu/Fedora. Windows is not installed here; CI uses
# vcpkg (see .github/workflows/sim.yml).
#
# Proxy: pass --proxy, or export http_proxy / https_proxy / all_proxy
# before running. The value is handed to apt, dnf, Homebrew, pip, and the
# littlefs submodule fetch. Git config is not modified.
#
#   ./scripts/install_deps.sh
#   ./scripts/install_deps.sh --proxy http://proxy.example:8080
#   ./scripts/install_deps.sh --proxy http://user:pass@proxy.example:8080
#   make install-deps PROXY=http://proxy.example:8080

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv-pio"
PROXY="${https_proxy:-${HTTPS_PROXY:-${http_proxy:-${HTTP_PROXY:-${all_proxy:-${ALL_PROXY:-}}}}}}"
NO_PROXY_VAL="${no_proxy:-${NO_PROXY:-}}"
SKIP_SUBMODULES=0

usage() {
    cat <<'EOF'
Usage: scripts/install_deps.sh [--proxy URL] [--no-proxy LIST] [--skip-submodules]

  --proxy URL         HTTP(S) or socks proxy for apt/dnf, Homebrew, pip, and git
  --no-proxy LIST     Comma-separated hosts that bypass the proxy
  --skip-submodules   Do not fetch third_party/littlefs
  -h, --help          Show this help

Environment variables http_proxy, https_proxy, and all_proxy are used when
--proxy is omitted. Add .venv-pio/bin to PATH after a successful run so
platformio, pyserial, cmake-format, and pre-commit are found.
EOF
}

mask_proxy() {
    # Hide userinfo so a proxy password is not printed.
    printf '%s\n' "$1" | sed -E 's#(://)[^/@]*@#\1***@#'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --proxy)
            PROXY="${2:-}"
            if [[ -z "$PROXY" ]]; then
                echo "error: --proxy needs a URL" >&2
                exit 2
            fi
            shift 2
            ;;
        --no-proxy)
            NO_PROXY_VAL="${2:-}"
            shift 2
            ;;
        --skip-submodules)
            SKIP_SUBMODULES=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

apply_proxy() {
    if [[ -z "$PROXY" ]]; then
        return 0
    fi
    export http_proxy="$PROXY"
    export https_proxy="$PROXY"
    export HTTP_PROXY="$PROXY"
    export HTTPS_PROXY="$PROXY"
    export all_proxy="$PROXY"
    export ALL_PROXY="$PROXY"
    if [[ -n "$NO_PROXY_VAL" ]]; then
        export no_proxy="$NO_PROXY_VAL"
        export NO_PROXY="$NO_PROXY_VAL"
    fi
    echo "Using proxy $(mask_proxy "$PROXY")"
}

need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required command not found: $1" >&2
        exit 1
    fi
}

run_privileged() {
    if [[ "$(id -u)" -eq 0 ]]; then
        env "$@"
        return
    fi
    if ! command -v sudo >/dev/null 2>&1; then
        echo "error: sudo is required to install system packages" >&2
        exit 1
    fi
    # sudo drops the proxy unless it is passed on the command line.
    sudo env \
        http_proxy="${http_proxy:-}" \
        https_proxy="${https_proxy:-}" \
        HTTP_PROXY="${HTTP_PROXY:-}" \
        HTTPS_PROXY="${HTTPS_PROXY:-}" \
        all_proxy="${all_proxy:-}" \
        ALL_PROXY="${ALL_PROXY:-}" \
        no_proxy="${no_proxy:-}" \
        NO_PROXY="${NO_PROXY:-}" \
        DEBIAN_FRONTEND=noninteractive \
        "$@"
}

install_debian() {
    local packages=(
        build-essential
        ca-certificates
        cmake
        ninja-build
        pkg-config
        git
        python3
        python3-venv
        python3-pip
        libsdl2-dev
        portaudio19-dev
        lcov
        clang-format
    )
    if [[ -n "$PROXY" ]]; then
        run_privileged apt-get update \
            -o "Acquire::http::Proxy=${PROXY}" \
            -o "Acquire::https::Proxy=${PROXY}"
        run_privileged apt-get install -y \
            -o "Acquire::http::Proxy=${PROXY}" \
            -o "Acquire::https::Proxy=${PROXY}" \
            "${packages[@]}"
    else
        run_privileged apt-get update
        run_privileged apt-get install -y "${packages[@]}"
    fi
}

install_fedora() {
    local packages=(
        gcc
        gcc-c++
        make
        cmake
        ninja-build
        pkgconf-pkg-config
        git
        python3
        python3-pip
        SDL2-devel
        portaudio-devel
        lcov
        clang-tools-extra
        ca-certificates
    )
    if [[ -n "$PROXY" ]]; then
        run_privileged dnf install -y --setopt="proxy=${PROXY}" "${packages[@]}"
    else
        run_privileged dnf install -y "${packages[@]}"
    fi
}

install_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "error: Homebrew is required on macOS (https://brew.sh)" >&2
        exit 1
    fi
    brew install cmake ninja sdl2 portaudio lcov clang-format python@3
}

install_python() {
    need python3
    echo "Creating $VENV"
    python3 -m venv "$VENV"
    local py="$VENV/bin/python"
    # platformio + pyserial: USB flash and the serial monitor.
    # cmakelang: pre-commit cmake-format hook.
    # pre-commit + kconfiglib: the repo hooks.
    if [[ -n "$PROXY" ]]; then
        "$py" -m pip install --upgrade pip --proxy "$PROXY"
        "$py" -m pip install --proxy "$PROXY" \
            platformio pyserial cmakelang pre-commit kconfiglib
    else
        "$py" -m pip install --upgrade pip
        "$py" -m pip install \
            platformio pyserial cmakelang pre-commit kconfiglib
    fi
    if [[ -d "$ROOT/.git" ]]; then
        "$VENV/bin/pre-commit" install
    fi
}

install_submodules() {
    if [[ "$SKIP_SUBMODULES" -eq 1 ]]; then
        echo "Skipping git submodules"
        return 0
    fi
    if [[ ! -d "$ROOT/.git" ]]; then
        echo "Not a git checkout; skipping submodules"
        return 0
    fi
    local git_cmd=(git -C "$ROOT")
    if [[ -n "$PROXY" ]]; then
        git_cmd+=(-c "http.proxy=${PROXY}" -c "https.proxy=${PROXY}")
    fi
    echo "Fetching submodules"
    "${git_cmd[@]}" submodule update --init --recursive
}

main() {
    apply_proxy
    local os
    os="$(uname -s)"
    case "$os" in
        Linux)
            if [[ -r /etc/os-release ]]; then
                # shellcheck disable=SC1091
                . /etc/os-release
            fi
            case "${ID:-}${ID_LIKE:-}" in
                *fedora*|*rhel*|*centos*)
                    install_fedora
                    ;;
                *debian*|*ubuntu*|*linuxmint*)
                    install_debian
                    ;;
                *)
                    if command -v apt-get >/dev/null 2>&1; then
                        install_debian
                    elif command -v dnf >/dev/null 2>&1; then
                        install_fedora
                    else
                        echo "error: unsupported Linux (${ID:-unknown}). Need apt-get or dnf." >&2
                        exit 1
                    fi
                    ;;
            esac
            ;;
        Darwin)
            install_macos
            ;;
        MINGW*|MSYS*|CYGWIN*)
            cat >&2 <<'EOF'
error: this script does not install the Windows toolchain.
CI uses vcpkg (see .github/workflows/sim.yml):

  git clone https://github.com/microsoft/vcpkg.git
  ./vcpkg/bootstrap-vcpkg.bat
  ./vcpkg/vcpkg install sdl2:x64-windows portaudio:x64-windows

Set HTTPS_PROXY before the clone if you are behind a proxy, then configure
with -DCMAKE_TOOLCHAIN_FILE=.../vcpkg/scripts/buildsystems/vcpkg.cmake
EOF
            exit 1
            ;;
        *)
            echo "error: unsupported OS: $os" >&2
            exit 1
            ;;
    esac

    install_python
    install_submodules

    cat <<EOF

Installed.
  Simulator deps: cmake, a C compiler, SDL2, PortAudio, lcov
  Flash tools:    $VENV/bin/platformio
  Python pkgs:    platformio, pyserial, cmakelang, pre-commit, kconfiglib

Add the virtualenv to PATH (platformio and cmake-format):

  export PATH="$VENV/bin:\$PATH"

Then, from $ROOT:

  make test
  make usb DEVICE=nodemcu

The first USB build downloads the board toolchain through the same proxy
variables. Export http_proxy and https_proxy again in that shell if you
passed --proxy only to this script.
EOF
}

main
