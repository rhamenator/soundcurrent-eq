#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$repo_root/build-windows"
version=$(sed -n 's/^project(soundcurrent-eq VERSION \([0-9.]*\).*/\1/p' "$repo_root/CMakeLists.txt")
mkdir -p "$repo_root/dist"

# Preserve the complete, unmodified standard vendor package. Pin the reviewed
# HTTPS download; a changed download must be reviewed before updating the pin.
cable_zip="$repo_root/.cache/VBCABLE_Driver_Pack45.zip"
cable_sha=b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb
mkdir -p "$(dirname "$cable_zip")"
if [[ ! -f "$cable_zip" ]]; then
    curl --fail --location --proto '=https' --proto-redir '=https' \
        --output "$cable_zip.part" \
        https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip
    mv "$cable_zip.part" "$cable_zip"
fi
printf '%s  %s\n' "$cable_sha" "$cable_zip" | sha256sum --check

cmake -S "$repo_root" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$repo_root/cmake/mingw64.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel

installer="$repo_root/dist/SoundCurrent-EQ-${version}-windows-x64-setup.exe"
(cd "$repo_root" && makensis \
    "-DAPP_EXE=$build_dir/soundcurrent-eq.exe" \
    "-DOUTPUT=$installer" \
    "-DCABLE_ZIP=$cable_zip" \
    "-DSOURCE_ROOT=$repo_root" \
    packaging/windows/soundcurrent-eq.nsi)
(cd "$repo_root/dist" && sha256sum "$(basename "$installer")" > "$(basename "$installer").sha256")
printf '%s\n' "$installer"
