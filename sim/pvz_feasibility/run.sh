#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$project_dir"
mkdir -p build/pvz-feasibility
compiler="${CROSS_COMPILE:-riscv64-linux-gnu-}"
"${compiler}gcc" -march=rv32i -mabi=ilp32 -O2 -ffreestanding -fno-builtin \
  -fno-pic -fno-pie -fno-stack-protector -msmall-data-limit=0 -mno-relax \
  -nostdlib -nostartfiles -static -no-pie -Wl,--build-id=none,--no-relax,-T,sw/link.ld \
  sw/start.S sim/pvz_feasibility/blit_probe.c -o build/pvz-feasibility/blit.elf
"${compiler}objcopy" -O verilog --verilog-data-width=1 \
  build/pvz-feasibility/blit.elf build/pvz-feasibility/blit.hex
"${compiler}objdump" -d build/pvz-feasibility/blit.elf > build/pvz-feasibility/blit.dis
"${compiler}size" build/pvz-feasibility/blit.elf
verilator --cc --exe --build -j 2 --threads 1 -O3 --top-module soc_top \
  -GWITH_VIDEO=1 -Wno-fatal -Irtl/soc/core --Mdir build/pvz-feasibility/model \
  -MAKEFLAGS 'OPT_FAST=-O3' -CFLAGS '-std=c++17 -O3 -flto' -LDFLAGS '-flto' \
  rtl/soc/core/*.v rtl/soc/soc_top.v rtl/soc/memory/*.v \
  rtl/soc/periph/*.v rtl/soc/periph/vga/*.v \
  "$project_dir/sim/pvz_feasibility/probe_main.cpp"
for round in 1 2 3; do
  ./build/pvz-feasibility/model/Vsoc_top +rom=build/pvz-feasibility/blit.hex \
    > "build/pvz-feasibility/blit-$round.csv" \
    2> "build/pvz-feasibility/blit-$round.log"
done
