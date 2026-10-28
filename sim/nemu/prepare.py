#!/usr/bin/env python3
"""对独立NEMU副本做三个有明确上下文的修改；不更改文件权限。"""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])


def replace(relative, old, new):
    path = root / relative
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"NEMU template differs: expected one matching block in {path}")
    path.write_text(text.replace(old, new))


replace("src/monitor/monitor.c",
        '  Log("Exercise: Please remove me in the source code and compile NEMU again.");\n'
        '  assert(0);\n', '')
replace("src/monitor/monitor.c",
        '  long size = ftell(fp);\n',
        '  long size = ftell(fp);\n'
        '  Assert(size > 0 && size <= CONFIG_MSIZE, "Image must fit in the course ROM");\n')
replace("src/cpu/cpu-exec.c", '  execute(n);\n',
        '  execute(n);\n'
        '  isa_reg_display(); // course adapter: print final/stepped registers\n')
