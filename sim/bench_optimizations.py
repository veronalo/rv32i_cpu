"""Build isolated single-thread models and compare deterministic Mario runs.

Run from the project root. Each build gets its own pin binding and C++ objects.
Results live in build/optimization/<name>; production outputs are not replaced.
"""
import argparse
import json
import os
from pathlib import Path
import re
import resource
import shlex
import signal
import subprocess


def run(command, *, stdout=None, timeout=180):
    process = subprocess.Popen(command, stdout=stdout, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        status = process.wait(timeout=timeout)
    except (subprocess.TimeoutExpired, KeyboardInterrupt):
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        raise
    if status:
        raise RuntimeError(f"Exit {status}: {shlex.join(map(str, command))}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--verilator-flags", default="")
    parser.add_argument("--cxx-flags", default="")
    parser.add_argument("--frames", type=int, default=330)
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--round-offset", type=int, default=0)
    parser.add_argument("--cpu", type=int, help="Optional fixed logical CPU for all compared runs")
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--gui", action="store_true", help="NVBoard with SDL dummy driver")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", args.name):
        parser.error("name must be a simple build label")
    if args.frames < 1 or args.rounds < 1 or args.round_offset < 0:
        parser.error("frames/rounds must be positive and round-offset nonnegative")
    out = Path("build/optimization") / args.name
    out.mkdir(parents=True, exist_ok=True)
    nvboard = Path(os.environ.get("NVBOARD_HOME", "/home/vero/ysyx2407/ysyx-workbench/nvboard"))
    if args.build:
        pins = (out / "pins.cpp").resolve()
        run(["python3", str(nvboard / "scripts/auto_pin_bind.py"),
             "constr/nvboard_top.nxdc", str(pins)])
        sources = sorted(Path("rtl/soc/core").glob("*.v")) + [Path("rtl/soc/soc_top.v")]
        for folder in ("memory", "periph", "periph/vga"):
            sources += sorted((Path("rtl/soc") / folder).glob("*.v"))
        sources += [Path("rtl/nvboard_top.v")]
        sdl_cflags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).strip()
        sdl_libs = subprocess.check_output(["sdl2-config", "--libs"], text=True).strip()
        command = ["verilator", "--cc", "--exe", "--build", "-j", "2",
                   "--top-module", "nvboard_top", "--threads", "1", "-Wall",
                   "-Wno-PINCONNECTEMPTY", "-Wno-BLKSEQ", "-Wno-UNUSEDSIGNAL",
                   "-Irtl/soc/core", "--Mdir", str(out),
                   "-MAKEFLAGS", "OPT_FAST=-O3",
                   "-CFLAGS", f"-std=c++17 -O3 {sdl_cflags} -I{nvboard}/usr/include {args.cxx_flags}",
                   "-LDFLAGS", f"{nvboard}/build/nvboard.a {sdl_libs} -lSDL2_image -lSDL2_ttf {args.cxx_flags}"]
        command += shlex.split(args.verilator_flags)
        command += list(map(str, sources)) + [str(Path("sim/nvboard_main.cpp").resolve()), str(pins)]
        (out / "build-command.txt").write_text(shlex.join(command) + "\n")
        with (out / "build.log").open("w") as log:
            run(command, stdout=log, timeout=300)
        print(f"BUILT {args.name}", flush=True)
    reference = args.reference
    rows = []
    if args.gui:
        os.environ["SDL_VIDEODRIVER"] = "dummy"
        os.environ["NVBOARD_HOME"] = str(nvboard)
    for repeat in range(args.rounds):
        round_number = repeat + 1 + args.round_offset
        mode = 'dummy' if args.gui else 'headless'
        if args.cpu is not None:
            mode += f'-cpu{args.cpu}'
        prefix = out / f"{mode}-{args.frames}-r{round_number}"
        command = [str((out / "Vnvboard_top").resolve()), "+rom=build/mario-rv32i/mario.hex", "--demo",
                   "--frames", str(args.frames), "--fps", "0", "--log", str(prefix) + ".state",
                   "--screenshot", str(prefix) + ".ppm"]
        if not args.gui:
            command += ["--headless"]
        if args.frames >= 300:
            command += ["--verify"]
        if args.cpu is not None:
            command = ["taskset", "-c", str(args.cpu)] + command
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        with Path(str(prefix) + ".stdout").open("w") as log:
            run(command, stdout=log)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        output = Path(str(prefix) + ".stdout").read_text()
        match = re.search(r"DONE: (\d+) game frames, (\d+) VGA frames, (\d+) cycles, ([\d.]+)s, ([\d.]+) game fps", output)
        if not match:
            raise RuntimeError(f"No completion in {prefix}.stdout")
        if reference is None:
            reference = prefix
        for suffix in (".state", ".ppm"):
            if Path(str(prefix) + suffix).read_bytes() != Path(str(reference) + suffix).read_bytes():
                raise RuntimeError(f"Mismatch: {prefix}{suffix} vs {reference}{suffix}")
        reference_output = Path(str(reference) + ".stdout").read_text()
        reference_cycles = re.search(r"(\d+) cycles,", reference_output)
        if not reference_cycles or match[3] != reference_cycles[1]:
            raise RuntimeError("Cycle count mismatch")
        row = dict(name=args.name, repeat=round_number, cpu=args.cpu, frames=int(match[1]),
                   cycles=int(match[3]), seconds=float(match[4]), fps=float(match[5]),
                   user_seconds=round(after.ru_utime-before.ru_utime, 3),
                   sys_seconds=round(after.ru_stime-before.ru_stime, 3),
                   reference=str(reference), matched=True)
        rows.append(row)
        print(json.dumps(row), flush=True)
    (out / f"{mode}-{args.frames}-results-r{args.round_offset + 1}.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main()
