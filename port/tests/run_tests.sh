#!/usr/bin/env bash
# Builds the port and runs the unit tests.
#
#   ./port/tests/run_tests.sh              # build + full suite
#   ./port/tests/run_tests.sh canastra     # only cases whose name matches
#   ./port/tests/run_tests.sh --debug      # Debug build
#
# Requirements: the same as ../build.sh (CMake + SDL2 development files).

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: run_tests.sh [options] [filter]

Options:
  --build-dir DIR   build directory (default: <port>/build)
  --debug           Debug build
  --release         Release build (default)
  -h, --help        show this help

The optional filter is matched against test case names.
EOF
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BURACO_BUILD_DIR:-$script_dir/../build}"
build_type="Release"
filter=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            if [[ $# -lt 2 ]]; then
                echo "run_tests.sh: --build-dir needs a value" >&2
                exit 2
            fi
            build_dir="$2"
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
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "run_tests.sh: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            filter="$1"
            shift
            ;;
    esac
done

build_flags=()
if [[ "$build_type" == "Debug" ]]; then
    build_flags+=(--debug)
else
    build_flags+=(--release)
fi

"$script_dir/../build.sh" --build-dir "$build_dir" "${build_flags[@]}"

exe="$build_dir/tests/buraco_tests"
if [[ -f "$exe.exe" ]]; then
    exe="$exe.exe"
fi
if [[ ! -x "$exe" ]]; then
    echo "run_tests.sh: test executable not found: $exe" >&2
    exit 1
fi

if [[ -n "$filter" ]]; then
    "$exe" "$filter"
else
    ctest --test-dir "$build_dir" --output-on-failure
fi
