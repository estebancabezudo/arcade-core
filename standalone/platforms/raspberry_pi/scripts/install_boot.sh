#!/usr/bin/env bash
set -euo pipefail
platform="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
root="$(cd "$platform/../../.." && pwd)"
model="${2:-4}"
toolchain="${3:-gcc}"
if [[ $# -lt 1 || $# -gt 3 || ! "$model" =~ ^[34]$ || ! "$toolchain" =~ ^(gcc|clang)$ ]]; then
    echo "usage: $0 /path/to/mounted-boot-partition [3|4] [gcc|clang]" >&2
    exit 2
fi
build="$root/build/standalone/raspberry_pi/model$model/$toolchain"
if [[ ! -f "$build/kernel8.img" ]]; then
    echo "Missing $build/kernel8.img; build MODEL=$model TOOLCHAIN=$toolchain first." >&2
    exit 1
fi
# Stage both files inside build/; the shared installer validates and backs up.
stage="$(mktemp -d "$build/install.XXXXXXXX")"
trap 'rm -rf "$stage"' EXIT
cp "$build/kernel8.img" "$stage/kernel8.img"
cp "$platform/boot/config.txt" "$stage/config.txt"
bash "$platform/scripts/install_image.sh" "$stage" "$1"
