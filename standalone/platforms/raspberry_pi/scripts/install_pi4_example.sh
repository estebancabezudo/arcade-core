#!/usr/bin/env bash
set -euo pipefail
platform="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
root="$(cd "$platform/../../.." && pwd)"
example="$root/build/standalone/raspberry_pi/examples/pi4_usb"
if [[ $# -ne 1 ]]; then
    echo "usage: $0 /path/to/mounted-raspberry-boot-partition" >&2
    exit 2
fi
bash "$platform/scripts/install_image.sh" "$example" "$1"
