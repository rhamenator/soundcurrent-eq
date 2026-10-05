#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$repo_root/build-windows"
version=$(sed -n 's/^project(soundcurrent-eq VERSION \([0-9.]*\).*/\1/p' "$repo_root/CMakeLists.txt")
mkdir -p "$repo_root/dist"

cmake -S "$repo_root" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$repo_root/cmake/mingw64.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel

installer="$repo_root/dist/SoundCurrent-EQ-${version}-windows-x64-setup.exe"
(cd "$repo_root" && makensis \
    "-DAPP_EXE=$build_dir/soundcurrent-eq.exe" \
    "-DOUTPUT=$installer" \
    "-DSOURCE_ROOT=$repo_root" \
    packaging/windows/soundcurrent-eq.nsi)
(cd "$repo_root/dist" && sha256sum "$(basename "$installer")" > "$(basename "$installer").sha256")
printf '%s\n' "$installer"
