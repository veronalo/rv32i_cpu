"""Real X11/XTEST Q-hold comparison: legacy text input versus UART-focus-only.

Opens its own NVBoard windows. Do not switch windows or type during this test.
Unlike SDL_PushEvent fixtures, this exercises X11, SDL's event pump, and the IME.
Requires xdotool, a real desktop, and no other running NVBoard simulation.
"""
import csv
import json
import math
import os
from pathlib import Path
import re
import signal
import statistics
import subprocess
import time


def xdo(*args):
    return subprocess.check_output(['xdotool', *map(str, args)], text=True).strip()


def stats(values):
    if not values:
        return None
    values = sorted(values)
    return dict(count=len(values), median_ms=round(statistics.median(values), 3),
                p95_ms=values[math.ceil(len(values)*.95)-1], max_ms=values[-1],
                total_ms=round(sum(values), 3))


def main():
    existing = subprocess.run(['pgrep', '-x', 'Vnvboard_top'], capture_output=True, text=True)
    if existing.returncode == 0:
        raise RuntimeError('Close existing NVBoard simulations before this controlled test')
    folder = Path('build/real-keyboard')
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    executable = str(Path('build/nvboard/Vnvboard_top').resolve())
    baseline = None
    for mode in ('legacy', 'focused'):
        prefix = folder / mode
        env = os.environ.copy()
        env.setdefault('NVBOARD_HOME', '/home/vero/ysyx2407/ysyx-workbench/nvboard')
        env['NVBOARD_RENDERER'] = 'software'
        env['NVBOARD_TEXT_INPUT'] = mode
        env['NVBOARD_INPUT_TRACE'] = str(prefix) + '-input.csv'
        command = [executable, '+rom=build/mario-rv32i/mario.hex', '--ps2-demo', '--verify',
                   '--frames', '600', '--frame-times', str(prefix)+'-frames.csv',
                   '--log', str(prefix)+'.state', '--screenshot', str(prefix)+'.ppm']
        print('START', mode, flush=True)
        with Path(str(prefix)+'.stdout').open('w') as out, Path(str(prefix)+'.stderr').open('w') as err:
            process = subprocess.Popen(command, env=env, stdout=out, stderr=err, start_new_session=True)
            q_down = False
            try:
                expiry = time.monotonic()+120
                window = None
                while time.monotonic() < expiry:
                    if process.poll() is not None:
                        raise RuntimeError('NVBoard exited before test: '+str(prefix))
                    found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(process.pid)],
                                           capture_output=True, text=True)
                    if found.returncode == 0:
                        window = found.stdout.splitlines()[0]
                        break
                    time.sleep(.1)
                if not window:
                    raise RuntimeError('No visible owned NVBoard window')
                xdo('windowactivate', '--sync', window)
                while time.monotonic() < expiry:
                    text = Path(str(prefix)+'.stdout').read_text()
                    if any(int(f) >= 210 for f in re.findall(r'game=(\d+)', text)):
                        break
                    if process.poll() is not None:
                        raise RuntimeError('NVBoard exited during startup')
                    time.sleep(.1)
                else:
                    raise RuntimeError('Startup timeout')
                phases = []
                for phase, duration in (('quiet-before', 2), ('Q-held', 4), ('quiet-after', 2)):
                    if xdo('getwindowfocus') != window:
                        raise RuntimeError('Focus changed; aborting rather than typing into another application')
                    if phase == 'Q-held':
                        xdo('keydown', 'q'); q_down = True
                    elif q_down:
                        xdo('keyup', 'q'); q_down = False
                    start = time.monotonic()*1000
                    until = time.monotonic()+duration
                    while time.monotonic() < until:
                        if process.poll() is not None:
                            raise RuntimeError('NVBoard exited during input phase')
                        if xdo('getwindowfocus') != window:
                            raise RuntimeError('Focus changed during input test')
                        time.sleep(.1)
                    end = time.monotonic()*1000
                    phases.append(dict(name=phase, start_ms=start, end_ms=end))
                    print(mode, phase, 'done', flush=True)
                if process.wait(timeout=120):
                    raise RuntimeError('NVBoard verification failed')
            finally:
                if q_down:
                    subprocess.run(['xdotool', 'keyup', 'q'], check=False)
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL); process.wait()
        with Path(str(prefix)+'-frames.csv').open() as fp:
            frames = list(csv.DictReader(fp))
        with Path(str(prefix)+'-input.csv').open() as fp:
            polls = list(csv.DictReader(fp))
        for phase in phases:
            lo, hi = phase['start_ms']+100, phase['end_ms']-100
            ff = [r for r in frames if lo <= float(r['monotonic_ms']) <= hi]
            pp = [r for r in polls if lo <= float(r['monotonic_ms']) <= hi]
            phase['frame_times'] = stats([float(r['frame_ms']) for r in ff])
            phase['poll_times'] = stats([float(r['poll_ms']) for r in pp])
            phase['repeat_events'] = sum(int(r['event_type']) == 768 and int(r['repeat']) != 0 for r in pp)
            phase['text_events'] = sum(int(r['event_type']) in (770, 771) for r in pp)
            phase['text_active_calls'] = sum(int(r['text_active']) for r in pp)
        state = Path(str(prefix)+'.state').read_bytes()
        image = Path(str(prefix)+'.ppm').read_bytes()
        if baseline is None:
            baseline = state, image
        if (state, image) != baseline:
            raise RuntimeError('Game state or pixels differ between text-input modes')
        result = dict(mode=mode, matched=True, phases=phases)
        results.append(result)
        (folder/'results.json').write_text(json.dumps(results, indent=2)+'\n')
        print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
