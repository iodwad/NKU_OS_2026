#!/bin/sh
# All downloads, extracted dependencies and build products stay in this project.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
debug_dir="$project_dir/.debug"
source_dir=${QEMU_SRC:-"$debug_dir/qemu-4.1.1"}
build_dir=${QEMU_BUILD:-"$debug_dir/qemu-build"}
mkdir -p "$debug_dir/downloads" "$debug_dir/deps" "$debug_dir/evidence" "$build_dir"
if [ ! -f "$source_dir/configure" ]; then
    archive="$debug_dir/downloads/qemu-4.1.1.tar.xz"
    if [ ! -f "$archive" ]; then
        curl -fL --retry 2 https://download.qemu.org/qemu-4.1.1.tar.xz -o "$archive.part"
        mv "$archive.part" "$archive"
    fi
    tar -xf "$archive" -C "$debug_dir"
fi
if [ "$(cat "$source_dir/VERSION")" != "4.1.1" ]; then
    echo 'Expected QEMU source VERSION=4.1.1' >&2
    exit 1
fi
if ! pkg-config --exists pixman-1; then
    if [ ! -d "$debug_dir/deps/usr/include/pixman-1" ]; then
        (cd "$debug_dir/downloads" && apt-get download libpixman-1-dev)
        for package in "$debug_dir"/downloads/libpixman-1-dev_*.deb; do
            dpkg-deb -x "$package" "$debug_dir/deps"
        done
    fi
    for pc in "$debug_dir"/deps/usr/lib/*/pkgconfig/pixman-1.pc; do
        python3 - "$pc" "$debug_dir/deps/usr" <<'PY'
from pathlib import Path
import re
import sys
path = Path(sys.argv[1])
path.write_text(re.sub(r'^prefix=.*$', 'prefix=' + sys.argv[2], path.read_text(), flags=re.M))
PY
        PKG_CONFIG_PATH="$(dirname "$pc")${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
        export PKG_CONFIG_PATH
    done
fi
cd "$build_dir"
if [ ! -f config-host.mak ]; then
    "$source_dir/configure" --target-list=riscv32-softmmu,riscv64-softmmu --enable-debug \
        > "$debug_dir/evidence/qemu-configure.log" 2>&1
fi
make -j"${JOBS:-$(nproc)}" > "$debug_dir/evidence/qemu-build.log" 2>&1
./riscv64-softmmu/qemu-system-riscv64 --version
file riscv64-softmmu/qemu-system-riscv64
echo "Build ready at $build_dir; no system installation performed."
