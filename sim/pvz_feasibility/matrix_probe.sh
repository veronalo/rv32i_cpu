#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_dir"
archive_dir="${PVZ_ARCHIVE_DIR:-$project_dir/../../pvz-native}"
source_dir="$archive_dir/PvZ-Portable/src/SexyAppFramework/misc"
[[ -f "$source_dir/SexyMatrix.cpp" ]] || { printf 'Full reference source missing: %s\n' "$source_dir" >&2; exit 2; }
mkdir -p build/pvz-feasibility
compiler="${CROSS_COMPILE:-riscv64-linux-gnu-}"
flags=(-march=rv32i -mabi=ilp32 -std=c++20 -O2 -ffreestanding -nostdinc++
       -fno-exceptions -fno-rtti -fno-pic -fno-pie -mno-relax -msmall-data-limit=0
       -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
       -Isim/pvz_feasibility/include "-I$source_dir")
"${compiler}g++" "${flags[@]}" -c "$source_dir/SexyMatrix.cpp" \
    -o build/pvz-feasibility/matrix.o
"${compiler}g++" "${flags[@]}" -c sim/pvz_feasibility/matrix_probe.cpp \
    -o build/pvz-feasibility/matrix-main.o
"${compiler}nm" -u build/pvz-feasibility/matrix.o > build/pvz-feasibility/matrix-undefined.txt
# Link only against the same minimal bare-metal environment; failure identifies
# missing runtime support, NOT a claim that RV32I cannot execute software float.
if "${compiler}g++" "${flags[@]}" -nostdlib -nostartfiles -static -no-pie \
    -Wl,--gc-sections,--build-id=none,--no-relax,-T,sw/link.ld \
    sw/start.S build/pvz-feasibility/matrix-main.o build/pvz-feasibility/matrix.o \
    -o build/pvz-feasibility/matrix.elf > build/pvz-feasibility/matrix-link.log 2>&1; then
    printf 'LINK_OK\n'
else
    printf 'LINK_BLOCKED: see build/pvz-feasibility/matrix-link.log\n'
fi
