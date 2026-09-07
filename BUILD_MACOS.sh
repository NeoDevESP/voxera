#!/usr/bin/env bash
# VOXERA 0.8.0 - macOS build (Apple Silicon + Intel universal)
#
# Run this on the Mac itself. A plugin cannot be cross-compiled from Windows:
# JUCE needs Xcode's toolchain and macOS frameworks to produce a bundle.
#
#   chmod +x BUILD_MACOS.sh
#   ./BUILD_MACOS.sh
#
# Add --install to copy the results into the user's plugin folders.

set -euo pipefail
cd "$(dirname "$0")"

INSTALL=0
[[ "${1:-}" == "--install" ]] && INSTALL=1

if [[ "$(uname)" != "Darwin" ]]; then
    echo "This script builds the macOS version and must run on macOS." >&2
    exit 1
fi

for tool in cmake git; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool." >&2
        echo "  Xcode tools: xcode-select --install" >&2
        echo "  CMake:       brew install cmake   (or cmake.org)" >&2
        exit 1
    fi
done

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode command line tools are not installed. Run: xcode-select --install" >&2
    exit 1
fi

echo "VOXERA 0.8.0 - macOS universal (arm64 + x86_64)"
cmake -S . -B build-mac -G Xcode -DVOXERA_BUILD_TESTS=ON
cmake --build build-mac --config Release --parallel 4
ctest --test-dir build-mac -C Release --output-on-failure

ARTEFACTS="build-mac/VOXERA_artefacts/Release"
echo
echo "Built:"
for item in "$ARTEFACTS/VST3/VOXERA+CHOP.vst3" "$ARTEFACTS/AU/VOXERA+CHOP.component" "$ARTEFACTS/Standalone/VOXERA+CHOP.app"; do
    [[ -e "$item" ]] && echo "  $item"
done

# A universal binary carries both slices; if one is missing the other machine
# silently cannot load the plugin, so it is worth confirming here.
if [[ -e "$ARTEFACTS/VST3/VOXERA+CHOP.vst3/Contents/MacOS/VOXERA+CHOP" ]]; then
    echo
    echo "Architectures:"
    lipo -archs "$ARTEFACTS/VST3/VOXERA+CHOP.vst3/Contents/MacOS/VOXERA+CHOP"
fi

if [[ $INSTALL -eq 1 ]]; then
    mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
    rm -rf ~/Library/Audio/Plug-Ins/VST3/VOXERA+CHOP.vst3
    rm -rf ~/Library/Audio/Plug-Ins/Components/VOXERA+CHOP.component
    cp -R "$ARTEFACTS/VST3/VOXERA+CHOP.vst3" ~/Library/Audio/Plug-Ins/VST3/
    [[ -e "$ARTEFACTS/AU/VOXERA+CHOP.component" ]] && \
        cp -R "$ARTEFACTS/AU/VOXERA+CHOP.component" ~/Library/Audio/Plug-Ins/Components/
    echo
    echo "Installed to ~/Library/Audio/Plug-Ins/"
    echo
    # Logic caches AU scan results and will not retry a component it has already
    # seen, so a rebuilt plugin needs the cache cleared before it reappears.
    echo "For Logic / GarageBand, clear the AU cache and rescan:"
    echo "  killall -9 AudioComponentRegistrar 2>/dev/null || true"
    echo "  auval -a | grep -i voxera"
fi

echo
echo "Done."
