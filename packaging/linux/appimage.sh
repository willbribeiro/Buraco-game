#!/usr/bin/env bash
# Builds a single-file AppImage for the SDL2 port.
#
#   ./packaging/linux/appimage.sh                  # build + dist/buraco-x86_64.AppImage
#   ./packaging/linux/appimage.sh --skip-build     # reuse an existing build tree
#   ./packaging/linux/appimage.sh --appimagetool /tmp/appimagetool
#
# The AppDir mirrors the FHS layout of `cmake --install` and adds the runtime
# libraries the game links against:
#
#   Buraco.AppDir/
#     AppRun
#     buraco.desktop, buraco.png, .DirIcon
#     usr/bin/buraco                     (RPATH $ORIGIN/../lib)
#     usr/lib/libSDL2-2.0.so.0
#     usr/lib/libstdc++.so.6, libgcc_s.so.1
#     usr/share/buraco/assets/font8x16.bin
#     usr/share/applications/buraco.desktop
#     usr/share/icons/hicolor/{scalable,256x256}/apps/buraco.*
#
# SDL2 is bundled because the game is built against whatever SDL the build host
# has (2.0.18+); libstdc++/libgcc are bundled so an AppImage built on a newer
# distro still starts on one with an older C++ runtime.  glibc and the X11/
# Wayland/audio stack stay on the host, which is what SDL expects.  The
# official CI artifact is built on Ubuntu 22.04 (glibc 2.35) for the widest
# compatibility; a local build can only run on distributions at least as new
# as the build host.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: appimage.sh [options]

Options:
  --build-dir DIR     build tree to package (default: <repo>/port/build)
  --out DIR           where the AppImage is written (default: <repo>/dist)
  --skip-build        do not run port/build.sh, only package the build tree
  --appimagetool FILE use this appimagetool instead of the one in PATH
  -h, --help          show this help

Environment:
  BURACO_BUILD_DIR    default build tree (same as --build-dir)
  APPIMAGETOOL        default appimagetool (same as --appimagetool)

appimagetool is a separate download; the script accepts both the AppImage
build (run with --appimage-extract-and-run, so no FUSE is needed) and a plain
binary from a distribution package.
EOF
}

die() {
    echo "appimage.sh: $*" >&2
    exit 1
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
build_dir="${BURACO_BUILD_DIR:-$repo_root/port/build}"
out_dir="$repo_root/dist"
skip_build=0
appimagetool="${APPIMAGETOOL:-}"

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
        --appimagetool)
            [[ $# -ge 2 ]] || die "--appimagetool needs a value"
            appimagetool="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "appimage.sh: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

absolute() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$PWD" "$1" ;;
    esac
}
build_dir="$(absolute "$build_dir")"
out_dir="$(absolute "$out_dir")"

case "$(uname -m)" in
    x86_64|amd64) arch="x86_64" ;;
    aarch64|arm64) arch="aarch64" ;;
    armv7l|armhf) arch="armhf" ;;
    i?86) arch="i686" ;;
    *) die "unsupported architecture: $(uname -m)" ;;
esac

command -v cmake >/dev/null 2>&1 || die "cmake not found (install CMake 3.20 or newer)"
command -v patchelf >/dev/null 2>&1 ||
    die "patchelf not found (install patchelf so the AppImage finds its bundled SDL2)"
command -v ldd >/dev/null 2>&1 || die "ldd not found (it ships with glibc)"

if [[ -z "$appimagetool" ]]; then
    appimagetool="$(command -v appimagetool || true)"
fi
[[ -n "$appimagetool" ]] ||
    die "appimagetool not found; download it from https://github.com/AppImage/appimagetool/releases and pass --appimagetool FILE"
[[ -x "$appimagetool" ]] || die "appimagetool is not executable: $appimagetool"

if [[ $skip_build -eq 1 ]]; then
    [[ -f "$build_dir/CMakeCache.txt" ]] ||
        die "no CMake cache in $build_dir (drop --skip-build to build first)"
else
    "$repo_root/port/build.sh" --build-dir "$build_dir"
fi

[[ -x "$build_dir/buraco" ]] || die "missing $build_dir/buraco (build failed?)"

stage="$(mktemp -d "${TMPDIR:-/tmp}/buraco-appimage.XXXXXX")"
trap 'chmod -R u+w "$stage" 2>/dev/null; rm -rf "$stage"' EXIT

appdir="$stage/Buraco.AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/share/buraco/assets"

install -m 755 "$build_dir/buraco" "$appdir/usr/bin/buraco"
# Symbol names are only useful in the build tree, as in pack_linux.sh.
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$appdir/usr/bin/buraco"
fi
cp -r "$repo_root/port/assets/." "$appdir/usr/share/buraco/assets/"

install -Dm644 "$script_dir/buraco.desktop" \
    "$appdir/usr/share/applications/buraco.desktop"
install -Dm644 "$script_dir/buraco.svg" \
    "$appdir/usr/share/icons/hicolor/scalable/apps/buraco.svg"
install -Dm644 "$script_dir/buraco-256.png" \
    "$appdir/usr/share/icons/hicolor/256x256/apps/buraco.png"

# appimagetool looks for the desktop entry and the icon at the AppDir root.
install -m 644 "$script_dir/buraco.desktop" "$appdir/buraco.desktop"
install -m 644 "$script_dir/buraco-256.png" "$appdir/buraco.png"
ln -s buraco.png "$appdir/.DirIcon"

# Copy the resolved libraries (ldd prints the real file behind the SONAME
# symlink) into usr/lib.  SDL2 is mandatory; the C++ runtime is optional so
# static builds keep working.
bundle_library() {
    local soname="$1" required="$2" source
    source="$(ldd "$build_dir/buraco" | awk -v n="$soname" '$1 == n && $2 == "=>" { print $3 }')"
    if [[ -z "$source" ]]; then
        [[ "$required" == "required" ]] && die "the game does not link against $soname"
        return 0
    fi
    [[ -f "$source" ]] || die "$soname resolves to a missing file: $source"
    install -m 644 "$source" "$appdir/usr/lib/$soname"
}

bundle_library libSDL2-2.0.so.0 required
bundle_library libstdc++.so.6 optional
bundle_library libgcc_s.so.1 optional

# $ORIGIN is the folder of the executable (usr/bin), so ../lib is usr/lib.
patchelf --set-rpath '$ORIGIN/../lib' "$appdir/usr/bin/buraco"

# Keep the runtime self-contained: no LD_LIBRARY_PATH leak from the host.
cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
HERE="${APPDIR:-$(dirname "$(readlink -f "$0")")}"
exec "$HERE/usr/bin/buraco" "$@"
EOF
chmod 755 "$appdir/AppRun"

# appimagetool is distributed as an AppImage, which needs FUSE to mount; the
# runtime flag extracts it to a temporary folder instead, which also works in
# containers and in WSL.
tool_args=()
if [[ "$(od -An -tx1 -j8 -N3 -- "$appimagetool" 2>/dev/null | tr -d ' \n')" == "414902" ]]; then
    tool_args+=(--appimage-extract-and-run)
fi

mkdir -p "$out_dir"
appimage="$out_dir/buraco-$arch.AppImage"
rm -f "$appimage"
ARCH="$arch" "$appimagetool" "${tool_args[@]}" --no-appstream "$appdir" "$appimage"
chmod 755 "$appimage"

# Smoke test: the AppImage runtime extracts itself, the binary finds its font
# through the bundled usr/share and links against the bundled SDL2.
info="$(SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    "$appimage" --appimage-extract-and-run --display-info 2>&1)" || {
    printf '%s\n' "$info" >&2
    die "the AppImage failed to start"
}
grep -q 'share/buraco/assets/font8x16\.bin$' <<<"$info" ||
    die "the AppImage did not find its font: $info"
grep -qE '^SDL: 2\.' <<<"$info" || die "the AppImage did not load SDL2: $info"

# A mounted AppImage is read-only, so saves must fall back to the per-user
# data folder.  chmod cannot emulate that for root, hence the guard.
if [[ "$(id -u)" -ne 0 ]]; then
    ( cd "$stage" && "$appimage" --appimage-extract >/dev/null )
    root="$stage/squashfs-root"
    chmod -R a-w "$root"
    fake_home="$stage/home"
    mkdir -p "$fake_home"
    info="$(env -i HOME="$fake_home" PATH="$PATH" \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        "$root/AppRun" --display-info 2>&1)" || {
        printf '%s\n' "$info" >&2
        die "the read-only AppDir failed to start"
    }
    grep -qF "saves: $fake_home/.local/share/buraco/buraco/" <<<"$info" ||
        die "the read-only AppDir did not use the user data folder: $info"
    chmod -R u+w "$root"
fi

printf 'appimage: %s (%s)\n' "$appimage" "$(du -h "$appimage" | cut -f1)"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$appimage"
fi
printf '  runtime libs: %s\n' \
    "$(cd "$appdir/usr/lib" && printf '%s ' * | sed 's/ $//')"
