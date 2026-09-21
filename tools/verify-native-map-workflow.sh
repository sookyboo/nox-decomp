#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
app_dir="${repo_dir}/build-deps/gamefiles/app"

die() {
    printf 'workflow check failed: %s\n' "$1" >&2
    exit 1
}

require_file() {
    [ -f "$1" ] || die "missing $1"
}

check_elf() {
    local binary=$1 expected=$2 description=$3
    require_file "$binary"
    local kind
    kind=$(file -b "$binary")
    case "$kind" in
        *"ELF"*"${expected}"*) ;;
        *) die "$description is not the expected native ELF ($kind)" ;;
    esac
    case "$kind" in
        *"PE32"*|*"Windows"*|*"Wine"*) die "$description is a Windows/Wine binary ($kind)" ;;
    esac
    printf '%s: %s\n' "$description" "$kind"
}

check_hash() {
    local path=$1 expected=$2
    require_file "$path"
    local actual
    actual=$(sha256sum "$path" | awk '{print $1}')
    [ "$actual" = "$expected" ] || die "stock fixture changed: $path ($actual)"
    printf 'stock fixture: %s (%s)\n' "$path" "$actual"
}

check_elf "${repo_dir}/build-amd64/src/out" 'x86-64' 'amd64 target'
check_elf "${repo_dir}/build-i386/src/out" 'Intel i386' 'i386 target'

for target in "${repo_dir}/build-amd64/src/out" "${repo_dir}/build-i386/src/out"; do
    if pgrep -f -- "$target" >/dev/null 2>&1; then
        die "target is already running: $target (stop stale game/GDB processes first)"
    fi
done

# CapFlag is a built-in stock fixture.  Keep this workflow independent of
# reloaded EUD maps and fail loudly if a probe left the game data modified.
check_hash "${app_dir}/maps/CapFlag/CapFlag.map" \
    6e666124ee36ad60a472f8be03bf1df7a347e7df0deba44cbecfecd82972ac50
check_hash "${app_dir}/maps/CapFlag/CapFlag.nxz" \
    b00f7ee5488e2a853a0806fa947e14af48758f4377b2916ce8d72bee87bced0c

ctest --test-dir "${repo_dir}/build-amd64" -R map_download_dispatch_test --output-on-failure
ctest --test-dir "${repo_dir}/build-i386" -R map_download_dispatch_test --output-on-failure

printf 'native map workflow checks passed\n'
