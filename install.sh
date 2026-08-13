#!/usr/bin/env bash
# Build and install Sideous into the standard Linux user plugin directories.
#
# Usage: ./install.sh [--vst3] [--lv2] [--clap] [--all]
# No flags = --all. Flags can be combined, e.g. --vst3 --lv2.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PLUGIN_NAMES=(sideous)

DO_VST3=0
DO_LV2=0
DO_CLAP=0

if [[ $# -eq 0 ]]; then
    DO_VST3=1; DO_LV2=1; DO_CLAP=1
else
    for arg in "$@"; do
        case "$arg" in
            --vst3) DO_VST3=1 ;;
            --lv2)  DO_LV2=1 ;;
            --clap) DO_CLAP=1 ;;
            --all)  DO_VST3=1; DO_LV2=1; DO_CLAP=1 ;;
            -h|--help)
                echo "Usage: $0 [--vst3] [--lv2] [--clap] [--all]"
                exit 0
                ;;
            *)
                echo "Unknown option: $arg" >&2
                echo "Usage: $0 [--vst3] [--lv2] [--clap] [--all]" >&2
                exit 1
                ;;
        esac
    done
fi

if [[ ! -f "$SCRIPT_DIR/dpf/distrho/DistrhoPlugin.hpp" ]]; then
    echo "==> dpf/ submodule missing, initializing..."
    git -C "$SCRIPT_DIR" submodule update --init --recursive
fi

if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cached_src="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt")"
    if [[ -n "$cached_src" && "$cached_src" != "$SCRIPT_DIR" ]]; then
        echo "==> build/ was configured for a different path ($cached_src) - wiping and reconfiguring..."
        rm -rf "$BUILD_DIR"
    fi
fi

echo "==> Configuring/building sideous..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

install_one() {
    local src="$1" dest_dir="$2"
    if [[ ! -e "$src" ]]; then
        echo "  !! skipping $(basename "$src") - not found (did the build produce it?)" >&2
        return
    fi
    mkdir -p "$dest_dir"
    rm -rf "${dest_dir:?}/$(basename "$src")"
    cp -r "$src" "$dest_dir/"
    echo "  installed $(basename "$src") -> $dest_dir/"
}

echo "==> Installing..."
for name in "${PLUGIN_NAMES[@]}"; do
    [[ $DO_VST3 -eq 1 ]] && install_one "$BUILD_DIR/bin/$name.vst3" "$HOME/.vst3"
    [[ $DO_LV2  -eq 1 ]] && install_one "$BUILD_DIR/bin/$name.lv2"  "$HOME/.lv2"
    [[ $DO_CLAP -eq 1 ]] && install_one "$BUILD_DIR/bin/$name.clap" "$HOME/.clap"
done

echo "==> Done."
