#!/usr/bin/env python3
"""Compare every frame's state from native C++ and the exact RV32I RTL port."""
import sys
from pathlib import Path
expected=Path(sys.argv[1]).read_text().splitlines()
actual=Path(sys.argv[2]).read_text().splitlines()
if len(actual)<len(expected):
    raise SystemExit(f'FAIL: RTL only produced {len(actual)} of {len(expected)} frames')
for i,(a,b) in enumerate(zip(expected,actual)):
    if a!=b:raise SystemExit(f'FAIL frame {i+1}: native={a} RTL={b}')
print(f'PASS: {len(expected)} frames match native C++ (mode, player position, scrolling, level)')
