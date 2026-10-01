#!/usr/bin/env bash
# Shared installer. It never formats a device or installs Raspberry Pi firmware.
set -euo pipefail
source_dir="${1:-}"
boot_dir="${2:-}"

if [[ -z "$source_dir" || -z "$boot_dir" || ! -d "$boot_dir" || ! -w "$boot_dir" ]]; then
    echo "usage: $0 /path/to/image-directory /path/to/mounted-boot-partition" >&2
    exit 2
fi
if [[ ! -f "$source_dir/kernel8.img" || ! -f "$source_dir/config.txt" ]]; then
    echo "Missing kernel8.img or config.txt under $source_dir" >&2
    exit 1
fi
source_dir="$(cd "$source_dir" && pwd -P)"
boot_dir="$(cd "$boot_dir" && pwd -P)"
if [[ "$source_dir" == "$boot_dir" ]]; then
    echo "Source and destination must be different directories." >&2
    exit 2
fi
if [[ -f "$source_dir/SHA256SUMS" ]]; then
    (cd "$source_dir" && sha256sum --check --strict SHA256SUMS)
fi
for name in kernel8.img config.txt; do
    if [[ -L "$boot_dir/$name" || ( -e "$boot_dir/$name" && ! -f "$boot_dir/$name" ) ]]; then
        echo "Refusing to replace a symlink or non-regular file: $boot_dir/$name" >&2
        exit 1
    fi
done
backup_root="$boot_dir/arcade-core-backup"
if [[ -L "$backup_root" || ( -e "$backup_root" && ! -d "$backup_root" ) ]]; then
    echo "Invalid backup directory: $backup_root" >&2
    exit 1
fi
if [[ -f "$boot_dir/kernel8.img" || -f "$boot_dir/config.txt" ]]; then
    mkdir -p "$backup_root"
    backup="$(mktemp -d "$backup_root/install.XXXXXXXX")"
    for name in kernel8.img config.txt; do
        if [[ -f "$boot_dir/$name" ]]; then
            cp -p "$boot_dir/$name" "$backup/$name"
        fi
    done
    echo "Previous files saved in $backup"
fi
cp "$source_dir/kernel8.img" "$boot_dir/kernel8.img"
cp "$source_dir/config.txt" "$boot_dir/config.txt"
sync
echo "Installed kernel8.img and config.txt into $boot_dir"
echo "Other boot files were left untouched; model-appropriate Raspberry Pi firmware must already be present."
