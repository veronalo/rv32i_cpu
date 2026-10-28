"""Compare real-window NVBoard software/GPU rendering using one RTL executable."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return None
    return dict(median_ms=round(statistics.median(ordered), 3),
                p95_ms=ordered[math.ceil(len(ordered) * .95) - 1],
                max_ms=ordered[-1], frames=len(ordered))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frames', type=int, default=420)
    p.add_argument('--rounds', type=int, default=2)
    p.add_argument('--output', type=Path, default=Path('build/display-bench'))
    p.add_argument('--reference', type=Path)
    p.add_argument('--fps', type=float, default=0, help='Optional host frame cap')
    p.add_argument('--executable', type=Path, default=Path('build/nvboard/Vnvboard_top'))
    p.add_argument('--modes', nargs='+', choices=['software', 'gpu'], default=['software', 'gpu'])
    args = p.parse_args()
    if args.frames < 300 or args.rounds < 1:
        p.error('Use frames>=300 and rounds>=1 for gameplay verification')
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.setdefault('NVBOARD_HOME', '/home/vero/ysyx2407/ysyx-workbench/nvboard')
    if env.get('SDL_VIDEODRIVER') == 'dummy':
        p.error('GPU comparison needs a real desktop, not SDL dummy')
    baseline = None
    results = []
    for repeat in range(args.rounds):
        for mode in (args.modes if repeat % 2 == 0 else list(reversed(args.modes))):
            prefix = args.output / f'{mode}-r{repeat + 1}'
            env['NVBOARD_RENDERER'] = mode
            command = [str(args.executable.resolve()), '+rom=build/mario-rv32i/mario.hex',
                       '--demo', '--verify', '--frames', str(args.frames),
                       '--frame-times', str(prefix) + '.csv', '--log', str(prefix) + '.state',
                       '--screenshot', str(prefix) + '.ppm']
            command += ['--fps', str(args.fps)]
            print(f'START {mode} round {repeat + 1}', flush=True)
            with Path(str(prefix) + '.stdout').open('w') as out, Path(str(prefix) + '.stderr').open('w') as err:
                process = subprocess.Popen(command, env=env, stdout=out, stderr=err, start_new_session=True)
                try:
                    status = process.wait(timeout=180)
                except (subprocess.TimeoutExpired, KeyboardInterrupt):
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                    raise
            if status:
                raise RuntimeError(f'{mode}: exit {status}; see {prefix}.stderr')
            stdout = Path(str(prefix) + '.stdout').read_text()
            stderr = Path(str(prefix) + '.stderr').read_text()
            match = re.search(r'DONE: (\d+) game frames, (\d+) VGA frames, (\d+) cycles, ([\d.]+)s, ([\d.]+) game fps', stdout)
            if not match:
                raise RuntimeError(f'Missing completion: {prefix}')
            actual = (Path(str(prefix) + '.state').read_bytes(),
                      Path(str(prefix) + '.ppm').read_bytes(), int(match[3]))
            if baseline is None:
                baseline = actual
            if actual != baseline:
                raise RuntimeError(f'State, screenshot, or cycles mismatch: {prefix}')
            if args.reference and actual[0] != args.reference.read_bytes():
                raise RuntimeError(f'Native-reference mismatch: {prefix}')
            with Path(str(prefix) + '.csv').open() as fp:
                frames = list(csv.DictReader(fp))
            result = dict(mode=mode, round=repeat + 1, seconds=float(match[4]), fps=float(match[5]),
                          cycles=int(match[3]), matched=True,
                          backend=stderr.strip(),
                          all_frames=distribution([float(r['frame_ms']) for r in frames[3:]]),
                          gameplay=distribution([float(r['frame_ms']) for r in frames if int(r['game_frame']) >= 220]))
            results.append(result)
            (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
