#!/usr/bin/env bash
# Builds every plugin's AU (AudioUnit) target and copies the resulting
# .component bundles into a single directory for easy copying into
# ~/Library/Audio/Plug-Ins/Components. AU is macOS-only - this script
# only does anything useful when run on macOS with Xcode command line
# tools installed.
#
# Usage:
#   scripts/build_au_plugins.sh [output-dir]
#
# output-dir defaults to build-plugin/AU_plugins under the repo root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-plugin"
OUTPUT_DIR="${1:-$BUILD_DIR/AU_plugins}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "warning: AU (AudioUnit) plugins can only be built on macOS - continuing anyway, but expect nothing to be produced." >&2
fi

echo "Configuring $BUILD_DIR ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DDSP_EFFECTS_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release

# Discover every plugin target name from plugin/CMakeLists.txt rather than
# hardcoding a list, so this script stays correct as algorithms are added.
# (Built with a for-loop rather than `mapfile`/`readarray` since macOS
# ships bash 3.2, which predates both.)
TARGETS=()
for name in $(grep -oE '^juce_add_plugin\(([A-Za-z0-9_]+)' "$REPO_ROOT/plugin/CMakeLists.txt" | sed 's/^juce_add_plugin(//'); do
    TARGETS+=("$name")
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "error: found no juce_add_plugin(...) targets in plugin/CMakeLists.txt" >&2
    exit 1
fi

echo "Found ${#TARGETS[@]} plugin targets."

FAILED=()
for target in "${TARGETS[@]}"; do
    echo "=== Building ${target}_AU ==="
    if ! cmake --build "$BUILD_DIR" -j -t "${target}_AU"; then
        FAILED+=("$target")
    fi
done

mkdir -p "$OUTPUT_DIR"
echo "Collecting .component bundles into $OUTPUT_DIR ..."
find "$BUILD_DIR/plugin" -type d -iname "*.component" -print0 |
    while IFS= read -r -d '' bundle; do
        echo "  copying $(basename "$bundle")"
        rm -rf "${OUTPUT_DIR:?}/$(basename "$bundle")"
        cp -R "$bundle" "$OUTPUT_DIR/"
    done

echo
echo "Done. AU bundles are in: $OUTPUT_DIR"
echo "To install them for your DAW to see:"
echo "  cp -R \"$OUTPUT_DIR\"/*.component ~/Library/Audio/Plug-Ins/Components/"

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo
    echo "warning: the following targets failed to build and were skipped:" >&2
    printf '  %s\n' "${FAILED[@]}" >&2
    exit 1
fi
