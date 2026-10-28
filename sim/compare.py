#!/usr/bin/env python3
"""先显示两边完整输出，再比较同一镜像的32个GPR和最终PC；不是在线差分。"""
import argparse
import pathlib
import re
import subprocess
import sys


def run(command, log_path):
    try:
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=60)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"Timed out: {command}") from error
    log_path.write_text(result.stdout)
    print(result.stdout, end="", flush=True)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log_path}")
    return result.stdout


def snapshot(output):
    regs = {int(i): int(value, 16) for i, value in
            re.findall(r"^x(\d+)\s*=\s*0x([0-9a-fA-F]{8})", output, re.M)}
    pc = re.findall(r"^PC\s*=\s*0x([0-9a-fA-F]{8})", output, re.M)
    if set(regs) != set(range(32)) or not pc:
        raise RuntimeError("Missing complete register snapshot")
    return regs, int(pc[-1], 16)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True)
    parser.add_argument("--expected", type=int, required=True)
    parser.add_argument("--nemu", required=True)
    args = parser.parse_args()
    output_dir = pathlib.Path("build/results")
    output_dir.mkdir(parents=True, exist_ok=True)
    prefix = output_dir / args.program
    print(f"=== RTL: {args.program} (expected {args.expected}) ===", flush=True)
    rtl = run(["vvp", "build/soc_tb", f"+rom=build/mario-rv32i/{args.program}.hex",
               f"+expected={args.expected}"], prefix.with_suffix(".rtl.txt"))
    print(f"=== NEMU: {args.program} ===", flush=True)
    nemu = run([args.nemu, "-b", f"build/mario-rv32i/{args.program}.bin"],
               prefix.with_suffix(".nemu.txt"))
    if "HIT GOOD TRAP" not in nemu:
        raise RuntimeError("NEMU did not reach a successful EBREAK (possibly timed out)")
    rregs, rpc = snapshot(rtl)
    nregs, npc = snapshot(nemu)
    errors = []
    for i in range(32):
        if rregs[i] != nregs[i]:
            errors.append(f"x{i:02}: RTL=0x{rregs[i]:08x}, NEMU=0x{nregs[i]:08x}")
    if rpc != npc:
        errors.append(f"PC: RTL=0x{rpc:08x}, NEMU=0x{npc:08x}")
    if nregs[11] != args.expected & 0xffffffff:
        errors.append(f"NEMU result a1 differs from expected {args.expected}")
    if errors:
        raise RuntimeError("\n".join(errors))
    print(f"PASS: {args.program}: 32/32 GPR + PC match, result={args.expected}")
    print(f"Logs: {prefix}.rtl.txt and {prefix}.nemu.txt")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
