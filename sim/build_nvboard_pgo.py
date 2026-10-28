"""Build an isolated GCC-PGO simulator from the current generated model.

Production RTL, libraries, generated sources, and executable are not modified.
Run `make nvboard-build` first. Training uses deterministic MMIO input with SDL
dummy; its result and the final optimized result must match the reference.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import tempfile


def run(command, log, *, env=None, timeout=300):
    log.write('$ ' + shlex.join(map(str, command)) + '\n')
    log.flush()
    process = subprocess.Popen(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT,
                               env=env, start_new_session=True)
    try:
        code = process.wait(timeout=timeout)
    except (subprocess.TimeoutExpired, KeyboardInterrupt):
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL); process.wait()
        raise
    if code:
        raise RuntimeError(f'Exit {code}: {shlex.join(map(str, command))}')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--if-needed', action='store_true', help='reuse a verified, matching build')
    args = parser.parse_args()
    source = Path('build/nvboard')
    out = Path('build/nvboard-pgo')
    production = source/'Vnvboard_top'
    if not production.is_file():
        raise SystemExit('Run make nvboard-build first')
    out.mkdir(parents=True, exist_ok=True)
    files = [p for pattern in ('Vnvboard_top*.h', 'Vnvboard_top*.cpp', 'Vnvboard_top*.mk')
             for p in source.glob(pattern)]
    inputs = {'production_sha256': digest(production),
              'model': {p.name: digest(p) for p in files},
              'driver_sha256': digest(Path('sim/nvboard_main.cpp')),
              'rom_sha256': digest(Path('build/mario-rv32i/mario.hex'))}
    if args.if_needed and (out/'manifest.json').is_file() and (out/'Vnvboard_top').is_file():
        try:
            previous = json.loads((out/'manifest.json').read_text())
        except (ValueError, OSError):
            previous = {}
        if (all(previous.get(k) == v for k, v in inputs.items())
                and previous.get('pgo_sha256') == digest(out/'Vnvboard_top')):
            print('PGO: verified build is up to date', flush=True)
            return
    for path in files:
        shutil.copy2(path, out/path.name)
    # Same object paths for profile generation and use are essential for GCC.
    # A fresh data directory avoids merging profiles from an older model.
    profiles = Path(tempfile.mkdtemp(prefix='profiles-', dir=out)).resolve()
    nvboard = Path(os.environ.get('NVBOARD_HOME', '/home/vero/ysyx2407/ysyx-workbench/nvboard'))
    sdl_cflags = subprocess.check_output(['sdl2-config', '--cflags'], text=True).strip()
    sdl_libs = subprocess.check_output(['sdl2-config', '--libs'], text=True).strip()
    base_cflags = f'-std=c++17 -O3 -flto {sdl_cflags} -I{nvboard}/usr/include'
    base_libs = f'-flto {Path("build/nvboard-display/renderer.a").resolve()} {nvboard}/build/nvboard.a {sdl_libs} -lSDL2_image -lSDL2_ttf'
    manifest = dict(inputs, profiles=str(profiles))
    env = os.environ.copy()
    env.update(NVBOARD_HOME=str(nvboard), NVBOARD_RENDERER='software',
               SDL_VIDEODRIVER='dummy', NVBOARD_TEXT_INPUT='focused')
    env.pop('NVBOARD_INPUT_TRACE', None)
    with (out/'build.log').open('w') as log:
        def compile_model(flags):
            run(['make', '-C', out, '-f', 'Vnvboard_top.mk', '-B', '-j2',
                 'OPT_FAST=-O3', f'VM_USER_CFLAGS={base_cflags} {flags}',
                 f'VM_USER_LDLIBS={base_libs} {flags}', 'Vnvboard_top'], log)

        print('PGO: compiling instrumented single-thread model', flush=True)
        compile_model(f'-fprofile-generate={profiles}')
        print('PGO: training on 600 deterministic game frames', flush=True)
        def execute(label):
            run([out/'Vnvboard_top', '+rom=build/mario-rv32i/mario.hex', '--demo', '--verify',
                 '--frames', '600', '--log', out/f'{label}.state',
                 '--screenshot', out/f'{label}.ppm'], log, env=env, timeout=180)
        execute('training')
        if not list(profiles.glob('*.gcda')):
            raise RuntimeError('No GCC profiles were written')
        print('PGO: recompiling with measured profiles', flush=True)
        compile_model(f'-fprofile-use={profiles} -fprofile-correction -Werror=missing-profile -Werror=coverage-mismatch')
        print('PGO: verifying optimized executable', flush=True)
        execute('optimized')
        for suffix in ('state', 'ppm'):
            if (out/f'training.{suffix}').read_bytes() != (out/f'optimized.{suffix}').read_bytes():
                raise RuntimeError(f'PGO changed {suffix}')
        run(['./build/mario_reference', 'sw/mario/generated/chr.bin', '600', out/'reference.state'], log)
        run(['python3', 'sim/compare_mario.py', out/'reference.state', out/'optimized.state'], log)
    if digest(production) != manifest['production_sha256']:
        raise RuntimeError('Production executable changed during the build')
    if any(digest(p) != manifest['model'][p.name] for p in files):
        raise RuntimeError('Production model changed during the build')
    manifest['pgo_sha256'] = digest(out/'Vnvboard_top')
    (out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('PASS: PGO profile use, 600-frame reference/state/pixels; production executable unchanged', flush=True)


if __name__ == '__main__':
    main()
