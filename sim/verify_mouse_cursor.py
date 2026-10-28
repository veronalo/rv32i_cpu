"""Headless real-CPU/VGA integration: only RTL arrow pixels may change."""
from pathlib import Path
import subprocess

folder = Path('build/mouse-cursor')
folder.mkdir(parents=True, exist_ok=True)
for name, extra in (('base', []), ('cursor', ['--cursor-demo'])):
    subprocess.run(['./build/pvz-nvboard/Vnvboard_top', '+rom=build/pvz-rv32i/pvz.hex',
                    '--headless', '--frames', '2', '--fps', '0', *extra,
                    '--screenshot', str(folder / (name+'.ppm')),
                    '--log', str(folder / (name+'.state'))], check=True, timeout=60)
assert (folder/'base.state').read_bytes() == (folder/'cursor.state').read_bytes()
header = b'P6\n640 480\n255\n'
base = (folder/'base.ppm').read_bytes()
actual = (folder/'cursor.ppm').read_bytes()
assert base.startswith(header) and actual.startswith(header)
expected = bytearray(base)
rows = ['B', 'BB', 'BWB', 'BWWB', 'BWWWB', 'BWWWWB', 'BWWWWWB',
        'BWWWWWWB', 'BWWWWWWWB', 'BWWWWWWWWB', 'BWWWWWWWWWB',
        'BWWWWWWBBBBB', 'BWWWWBWWB', 'BWWWB BWWB', 'BWWB  BWWB',
        'BWB    BWWB', 'BB     BWWB', 'B       BB']
for y, row in enumerate(rows):
    for x, texel in enumerate(row):
        if texel == ' ':
            continue
        color = b'\x00'*3 if texel == 'B' else bytes.fromhex('ffd65a')
        for sy in range(2):
            for sx in range(2):
                start = len(header) + ((200+y*2+sy)*640+300+x*2+sx)*3
                expected[start:start+3] = color
assert actual == expected, 'RTL cursor shape/position or untouched background mismatch'
assert base != actual, 'Cursor never appeared'
print('PASS: full VGA RGB contains RTL arrow, all other pixels and CPU game state unchanged')
