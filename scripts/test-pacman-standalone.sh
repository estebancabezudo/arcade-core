#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
build="build/tests/standalone/native"
mkdir -p "$build"
"${CC:-cc}" -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror \
    -Istandalone/include \
    build/standalone/native/pacman.c \
    build/standalone/raspberry_pi/generated/roms.c \
    tests/standalone/native/pacman_standalone_test.c \
    -o "$build/pacman_standalone_test"
"$build/pacman_standalone_test" > "$build/trace.txt"
diff -u tests/standalone/native/original-trace.txt "$build/trace.txt"
cat "$build/trace.txt"
