#!/usr/bin/env bash
# Builds the native port with CMake on Linux and other POSIX systems.
#
#   ./port/build.sh                     # Release build in port/build
#   ./port/build.sh --debug             # Debug build
#   ./port/build.sh --build-dir build-linux
#   ./port/build.sh --sdl-dir /opt/SDL2 # custom SDL2 CMake package
#   ./port/build.sh -- -DBURACO_BUILD_TESTS=OFF
#
# SDL2 development files are required.  On Debian/Ubuntu:
#
#   sudo apt install build-essential cmake ninja-build libsdl2-dev
#
# The resulting binary is port/build/buraco (assets/ is copied next to it by
# CMake), so it can be run directly from the build tree.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: build.sh [options] [-- extra cmake args]

Options:
  --build-dir DIR     build directory (default: <port>/build)
  --sdl-dir DIR       value for SDL2_DIR (or set the SDL2_DIR environment)
  --debug             Debug build (default: Release)
  --release           Release build
  -G, --generator G   CMake generator (default: Ninja if available)
  -h, --help          show this help

Environment:
  SDL2_DIR            SDL2 CMake package directory
  BURACO_BUILD_DIR    overrides the default build directory
  BURACO_BUILD_TYPE   overrides Release/Debug
EOF
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BURACO_BUILD_DIR:-$script_dir/build}"
build_type="${BURACO_BUILD_TYPE:-Release}"
sdl_dir="${SDL2_DIR:-}"
generator=""
extra_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            if [[ $# -lt 2 ]]; then
                echo "build.sh: --build-dir needs a value" >&2
                exit 2
            fi
            build_dir="$2"
            shift 2
            ;;
        --sdl-dir)
            if [[ $# -lt 2 ]]; then
                echo "build.sh: --sdl-dir needs a value" >&2
                exit 2
            fi
            sdl_dir="$2"
            shift 2
            ;;
        --debug)
            build_type="Debug"
            shift
            ;;
        --release)
            build_type="Release"
            shift
            ;;
        -G|--generator)
            if [[ $# -lt 2 ]]; then
                echo "build.sh: $1 needs a value" >&2
                exit 2
            fi
            generator="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            extra_args=("$@")
            break
            ;;
        *)
            echo "build.sh: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "build.sh: cmake not found. Install CMake 3.20 or newer." >&2
    exit 1
fi

if [[ -z "$generator" ]]; then
    if command -v ninja >/dev/null 2>&1; then
        generator="Ninja"
    else
        generator="Unix Makefiles"
    fi
fi

configure_args=(
    -S "$script_dir"
    -B "$build_dir"
    -G "$generator"
    "-DCMAKE_BUILD_TYPE=$build_type"
)
if [[ -n "$sdl_dir" ]]; then
    configure_args+=("-DSDL2_DIR=$sdl_dir")
fi
if [[ ${#extra_args[@]} -gt 0 ]]; then
    configure_args+=("${extra_args[@]}")
fi

if ! cmake "${configure_args[@]}"; then
    cat >&2 <<'EOF'

build.sh: CMake configuration failed.
If the error mentions SDL2, install the development package (version >= 2.0.18):

  Debian/Ubuntu: sudo apt install build-essential cmake ninja-build libsdl2-dev
  Fedora:        sudo dnf install gcc-c++ cmake ninja-build SDL2-devel
  Arch:          sudo pacman -S base-devel cmake ninja sdl2

Use --sdl-dir DIR (or the SDL2_DIR environment variable) for a custom build.
EOF
    exit 1
fi

cmake --build "$build_dir" --config "$build_type" --parallel

binary="$build_dir/buraco"
if [[ -f "$binary.exe" ]]; then
    binary="$binary.exe"
fi
echo "OK: $binary"
