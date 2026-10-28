#!/usr/bin/env bash
set -euo pipefail
source_dir=${1:?Usage: build.sh /path/to/original/nemu}
project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
adapter_dir="$project_dir/sim/nemu"
course_dir="$project_dir/build/nemu"

if [[ ! -f "$source_dir/src/nemu-main.c" ]]; then
    echo "NEMU source not found: $source_dir" >&2
    exit 1
fi
if [[ "$source_dir" == "$course_dir" ]]; then
    echo 'NEMU_SRC must point to the original source, not the generated copy.' >&2
    exit 1
fi

# 快速重复运行；适配文件有变化时重新覆盖独立副本并构建。
stamp="$course_dir/.course-ready"
binary="$course_dir/build/riscv32-nemu-interpreter"
if [[ -f "$stamp" && -x "$binary" ]] &&
   ! find "$adapter_dir" -type f -newer "$stamp" -print -quit | grep -q . &&
   [[ "$(<"$stamp")" == "$source_dir" ]]; then
    echo "Course NEMU ready: $binary"
    exit 0
fi

mkdir -p "$course_dir/tools"
# 只复制源码和离线构建工具；不复制原项目.git、build和Capstone仓库。
cp -R "$source_dir/src" "$source_dir/include" "$source_dir/scripts" "$source_dir/configs" "$course_dir/"
cp "$source_dir/Makefile" "$source_dir/Kconfig" "$source_dir/LICENSE" "$course_dir/"
cp -R "$source_dir/tools/kconfig" "$source_dir/tools/fixdep" "$course_dir/tools/"
cp "$source_dir/tools/difftest.mk" "$course_dir/tools/"
cp "$adapter_dir/inst.c" "$course_dir/src/isa/riscv32/inst.c"
cp "$adapter_dir/reg.c" "$course_dir/src/isa/riscv32/reg.c"
cp "$adapter_dir/paddr.c" "$course_dir/src/memory/paddr.c"
cp "$adapter_dir/sdb.c" "$course_dir/src/monitor/sdb/sdb.c"
cp "$adapter_dir/course_defconfig" "$course_dir/configs/course_defconfig"
python3 "$adapter_dir/prepare.py" "$course_dir"
make --no-print-directory -C "$course_dir" NEMU_HOME="$course_dir" course_defconfig
make --no-print-directory -C "$course_dir" NEMU_HOME="$course_dir" -j2
# stamp只在构建成功后写入。
printf '%s\n' "$source_dir" > "$stamp"
echo "Course NEMU ready: $binary"
