#!/usr/bin/env bash
# Builds just the Loom Browser plugin (LoomBrowserPlugin) - the single
# instance that can switch between every registered algorithm - in every
# format available on this platform, and copies the results into one
# directory. Skips the ~40 individual single-algorithm plugin targets
# plugin/CMakeLists.txt also declares; use scripts/build_au_plugins.sh if
# you actually want those built too.
#
# Usage:
#   scripts/build_browser_plugin.sh [output-dir]
#
# output-dir defaults to build-plugin/LoomBrowserPlugin_dist under the
# repo root.
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
OUTPUT_DIR="${1:-$BUILD_DIR/LoomBrowserPlugin_dist}"

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
echo "  VST3:       drop the .vst3 into your DAW's VST3 search path"
echo "  AU:         cp -R \"$OUTPUT_DIR\"/*.component ~/Library/Audio/Plug-Ins/Components/"
echo "  CLAP:       drop the .clap into your DAW's CLAP search path"
echo "  Standalone: run it directly"

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo
    echo "warning: the following targets failed to build and were skipped:" >&2
    printf '  %s\n' "${FAILED[@]}" >&2
    exit 1
fi
