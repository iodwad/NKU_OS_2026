#!/usr/bin/env bash
# Run only in a new disposable copy; preserve every existing debug session.
set -euo pipefail
repo=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)
qbuild=${QEMU_BUILD:-/home/andy/os_lab2_qemu_debug/build}
qsource=${QEMU_SRC:-/home/andy/os_lab2_qemu_debug/qemu-4.1.1}
test -x "$qbuild/riscv64-softmmu/qemu-system-riscv64"
test -d "$qsource/pc-bios"
work=$(mktemp -d "${TMPDIR:-/tmp}/lab2-mmu-XXXXXX")
git -C "$repo" archive b639987b1dd80ab93ea5b54e4cc193505ef27f01 lab2/code | tar -x -C "$work"
code="$work/lab2/code"
for file in Makefile tools/qemu_debug.py tools/verify_qemu_debug.py tools/build-qemu-debug.sh tools/gdb/host.gdb tools/gdb/guest.gdb tools/gdb/sv39_probe.py; do
    mkdir -p "$(dirname "$code/$file")"
    cp "$repo/lab2/code/$file" "$code/$file"
done
mkdir -p "$code/.debug"
ln -s "$qbuild" "$code/.debug/qemu-build"
ln -s "$qsource" "$code/.debug/qemu-4.1.1"
printf 'Isolated test workspace: %s\n' "$code"
export PATH="${GUEST_GDB_DIR:-/home/andy/opt/riscv/bin}:$PATH"
export TZ=Asia/Shanghai
make -C "$code" debug-verify
printf 'Evidence remains in %s/.debug/evidence/\n' "$code"
