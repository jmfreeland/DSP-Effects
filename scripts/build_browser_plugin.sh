#!/usr/bin/env bash
# Builds just the Loom Browser plugin (LoomBrowserPlugin) - the single
# instance that can switch between every registered algorithm - in every
# format available on this platform, and copies the results into one
# directory. Skips the ~40 individual single-algorithm plugin targets
# plugin/CMakeLists.txt also declares; use scripts/build_au_plugins.sh if
# you actually want those built too.
#
# Usage:
#   scripts/build_browser_plugin.sh [--install] [output-dir]
#
# output-dir defaults to build-plugin/LoomBrowserPlugin_dist under the
# repo root. --install additionally copies each built format into this
# platform's standard plugin search path (see INSTALL below) so a DAW
# picks it up on its next rescan - no manual copying needed.
#
# Formats built:
#   VST3       - all platforms
#   Standalone - all platforms
#   AU         - macOS only (skipped elsewhere, JUCE/AudioUnit limitation)
#   CLAP       - all platforms, via the free-audio/clap-juce-extensions
#                wrapper; first configure fetches that dependency (and its
#                own clap/clap-helpers submodules) over git, so expect a
#                one-time delay.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-plugin"

INSTALL=0
OUTPUT_DIR=""
for arg in "$@"; do
    if [[ "$arg" == "--install" ]]; then
        INSTALL=1
    else
        OUTPUT_DIR="$arg"
    fi
done
OUTPUT_DIR="${OUTPUT_DIR:-$BUILD_DIR/LoomBrowserPlugin_dist}"

echo "Configuring $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DDSP_EFFECTS_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release

TARGETS=(LoomBrowserPlugin_VST3 LoomBrowserPlugin_Standalone LoomBrowserPlugin_CLAP)
if [[ "$(uname -s)" == "Darwin" ]]; then
    TARGETS+=(LoomBrowserPlugin_AU)
else
    echo "note: skipping AU (AudioUnit is macOS-only)."
fi

FAILED=()
for target in "${TARGETS[@]}"; do
    echo "=== Building ${target} ==="
    if ! cmake --build "$BUILD_DIR" -j -t "$target"; then
        FAILED+=("$target")
    fi
done

mkdir -p "$OUTPUT_DIR"
echo "Collecting artefacts into $OUTPUT_DIR ..."
ARTEFACTS_DIR="$BUILD_DIR/plugin/LoomBrowserPlugin_artefacts"
for pattern in "*.vst3" "*.component" "*.clap"; do
    find "$ARTEFACTS_DIR" -mindepth 1 -maxdepth 1 \( -iname "$pattern" \) -print0 2>/dev/null |
        while IFS= read -r -d '' bundle; do
            echo "  copying $(basename "$bundle")"
            rm -rf "${OUTPUT_DIR:?}/$(basename "$bundle")"
            cp -R "$bundle" "$OUTPUT_DIR/"
        done
    find "$ARTEFACTS_DIR" -mindepth 2 \( -iname "$pattern" \) -print0 2>/dev/null |
        while IFS= read -r -d '' bundle; do
            echo "  copying $(basename "$bundle")"
            rm -rf "${OUTPUT_DIR:?}/$(basename "$bundle")"
            cp -R "$bundle" "$OUTPUT_DIR/"
        done
done
# Standalone is a bare executable (Loom - Browser[.exe]/.app), not a
# pattern-matched bundle extension - copy it by known artefact subdir.
if [[ -d "$ARTEFACTS_DIR/Standalone" ]]; then
    find "$ARTEFACTS_DIR/Standalone" -mindepth 1 -maxdepth 1 -print0 |
        while IFS= read -r -d '' item; do
            echo "  copying $(basename "$item")"
            rm -rf "${OUTPUT_DIR:?}/$(basename "$item")"
            cp -R "$item" "$OUTPUT_DIR/"
        done
fi

echo
echo "Done. LoomBrowserPlugin artefacts are in: $OUTPUT_DIR"

install_matching() {
    # install_matching <glob-pattern> <destination-dir>
    local pattern="$1" dest="$2"
    shopt -s nullglob nocaseglob
    local matches=("$OUTPUT_DIR"/$pattern)
    shopt -u nullglob nocaseglob
    if [[ ${#matches[@]} -eq 0 ]]; then
        return
    fi
    mkdir -p "$dest"
    for item in "${matches[@]}"; do
        echo "  installing $(basename "$item") -> $dest/"
        rm -rf "${dest:?}/$(basename "$item")"
        cp -R "$item" "$dest/"
    done
}

if [[ "$INSTALL" -eq 1 ]]; then
    echo
    echo "Installing into this platform's plugin search paths ..."
    case "$(uname -s)" in
        Darwin)
            install_matching "*.vst3" "$HOME/Library/Audio/Plug-Ins/VST3"
            install_matching "*.component" "$HOME/Library/Audio/Plug-Ins/Components"
            install_matching "*.clap" "$HOME/Library/Audio/Plug-Ins/CLAP"
            ;;
        Linux)
            install_matching "*.vst3" "$HOME/.vst3"
            install_matching "*.clap" "$HOME/.clap"
            ;;
        *)
            echo "warning: don't know the standard plugin install paths for $(uname -s) - skipping --install, use the paths below manually." >&2
            ;;
    esac
    echo "Done installing. Rescan plugins in your DAW to pick them up."
else
    echo "  VST3:       drop the .vst3 into your DAW's VST3 search path"
    echo "  AU:         cp -R \"$OUTPUT_DIR\"/*.component ~/Library/Audio/Plug-Ins/Components/"
    echo "  CLAP:       drop the .clap into your DAW's CLAP search path"
    echo "  Standalone: run it directly"
    echo "  (or re-run with --install to copy these into place automatically)"
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo
    echo "warning: the following targets failed to build and were skipped:" >&2
    printf '  %s\n' "${FAILED[@]}" >&2
    exit 1
fi
