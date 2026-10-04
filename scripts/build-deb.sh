#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
version=$(sed -n 's/^project(soundcurrent-eq VERSION \([0-9.]*\).*/\1/p' "$project_dir/CMakeLists.txt")
architecture=$(dpkg --print-architecture)
build_dir="$project_dir/build"
package_dir=$(mktemp -d)
trap 'rm -rf "$package_dir"' EXIT

cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel
DESTDIR="$package_dir" cmake --install "$build_dir" --prefix /usr

mkdir -p "$package_dir/DEBIAN" "$project_dir/dist"
cat > "$package_dir/DEBIAN/control" <<EOF
Package: soundcurrent-eq
Version: $version
Section: sound
Priority: optional
Architecture: $architecture
Maintainer: rhamenator <rhamenator@gmail.com>
Depends: libqt6widgets6 (>= 6.4), pipewire, pipewire-pulse, pipewire-bin, wireplumber, pulseaudio-utils
Homepage: https://github.com/rhamenator/soundcurrent-eq
Description: Simple nine-band desktop equalizer for PipeWire
 SoundCurrent EQ provides listening presets, custom profiles, and automatic
 output-device selection in a native C++ desktop application.
EOF

find "$package_dir" -type d -exec chmod 755 {} +

deb="$project_dir/dist/soundcurrent-eq_${version}_${architecture}.deb"
dpkg-deb --build --root-owner-group "$package_dir" "$deb"
sha256sum "$deb" > "$deb.sha256"
printf 'Built %s\n' "$deb"
