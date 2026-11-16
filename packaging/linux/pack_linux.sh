#!/usr/bin/env bash
# Builds the self-contained Linux tarball for the SDL2 port.
#
#   ./packaging/linux/pack_linux.sh                    # build + dist/buraco-linux-x86_64.tar.gz
#   ./packaging/linux/pack_linux.sh --skip-build       # reuse an existing build tree
#   ./packaging/linux/pack_linux.sh --build-dir port/build-linux --out /tmp/out
#
# The tarball mirrors the FHS layout written by `cmake --install`:
#
#   buraco-linux-x86_64/
#     bin/buraco
#     share/buraco/assets/font8x16.bin
#     share/applications/buraco.desktop
#     share/icons/hicolor/scalable/apps/buraco.svg
#     share/icons/hicolor/256x256/apps/buraco.png
#     README.md
#     INSTALL.txt
#
# Extracting it is enough to play: `./buraco-linux-x86_64/bin/buraco` finds the
# font in ../share/buraco/assets and keeps saves/config inside the folder
# (which is writable).  `sudo cp -r bin share /` installs it system-wide.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: pack_linux.sh [options]

Options:
  --build-dir DIR   build tree to install from (default: <repo>/port/build)
  --out DIR         where the tarball is written (default: <repo>/dist)
  --skip-build      do not run port/build.sh, only package the build tree
  -h, --help        show this help

Environment:
  BURACO_BUILD_DIR  default build tree (same as --build-dir)
  SOURCE_DATE_EPOCH timestamps stored in the tarball, for reproducible builds
EOF
}

die() {
    echo "pack_linux.sh: $*" >&2
    exit 1
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
build_dir="${BURACO_BUILD_DIR:-$repo_root/port/build}"
out_dir="$repo_root/dist"
skip_build=0
package_name="buraco-linux-x86_64"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            [[ $# -ge 2 ]] || die "--build-dir needs a value"
            build_dir="$2"
            shift 2
            ;;
        --out)
            [[ $# -ge 2 ]] || die "--out needs a value"
            out_dir="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "pack_linux.sh: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

# cmake resolves relative paths against the caller's directory; make both
# directories absolute so the rest of the script can use them from anywhere.
absolute() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PWD" "$1" ;;
    esac
}
build_dir="$(absolute "$build_dir")"
out_dir="$(absolute "$out_dir")"

command -v cmake >/dev/null 2>&1 || die "cmake not found (install CMake 3.20 or newer)"

if [[ $skip_build -eq 1 ]]; then
    [[ -f "$build_dir/CMakeCache.txt" ]] ||
        die "no CMake cache in $build_dir (drop --skip-build to build first)"
else
    "$repo_root/port/build.sh" --build-dir "$build_dir"
fi

[[ -x "$build_dir/buraco" ]] || die "missing $build_dir/buraco (build failed?)"

stage="$(mktemp -d "${TMPDIR:-/tmp}/buraco-pack.XXXXXX")"
trap 'rm -rf "$stage"' EXIT

package="$stage/$package_name"
cmake --install "$build_dir" --prefix "$package" >/dev/null

# Symbol names are only useful in the build tree; the tarball ships a stripped
# copy.  strip is optional so unusual toolchains still produce a package.
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$package/bin/buraco"
fi

cp "$repo_root/README.md" "$package/README.md"
cp "$script_dir/INSTALL.txt" "$package/INSTALL.txt"

for required in \
    bin/buraco \
    share/buraco/assets/font8x16.bin \
    share/applications/buraco.desktop \
    share/icons/hicolor/scalable/apps/buraco.svg \
    share/icons/hicolor/256x256/apps/buraco.png; do
    [[ -e "$package/$required" ]] || die "the install step did not produce $required"
done

# Smoke test: the packaged binary must find the font through its own prefix and
# keep saves next to itself, without any environment override.
info="$(SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$package/bin/buraco" --display-info 2>&1)" || {
    printf '%s\n' "$info" >&2
    die "the packaged binary failed to start"
}
grep -q 'share/buraco/assets/font8x16\.bin$' <<<"$info" ||
    die "the packaged binary did not find its font: $info"
grep -q "^saves: $package/bin/$" <<<"$info" ||
    die "the packaged binary did not use its own folder for saves: $info"
rm -f "$package/bin/buraco.cfg"

mkdir -p "$out_dir"
tarball="$out_dir/$package_name.tar.gz"
rm -f "$tarball"

tar_args=(--sort=name --owner=0 --group=0 --numeric-owner)
if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
    tar_args+=("--mtime=@$SOURCE_DATE_EPOCH")
fi
tar -C "$stage" "${tar_args[@]}" -czf "$tarball" "$package_name"

printf 'package: %s (%s)\n' "$tarball" "$(du -h "$tarball" | cut -f1)"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$tarball"
fi
tar -tzf "$tarball" | sed 's/^/  /'
