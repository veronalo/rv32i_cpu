"""Real CPU vs native Game hashes and complete RGB; no desktop mouse/windows."""
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--frames',type=int,default=600)
p.add_argument('--ps2',action='store_true')
args=p.parse_args()
if args.frames<=0:p.error('--frames must be positive')
folder=Path('build/tetris-tests');folder.mkdir(parents=True,exist_ok=True)
stem='ps2' if args.ps2 else 'direct'
reference=folder/(stem+'-reference');actual=folder/stem
env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',NVBOARD_RENDERER='software')
native=['./build/tetris-native/tetris','--headless','--demo','--frames',str(args.frames)]
rtl=['./build/tetris-nvboard/Vnvboard_top','+rom=build/tetris-rv32i/tetris.hex',
     *(['--ps2-demo'] if args.ps2 else ['--headless','--demo']),'--fps','0','--frames',str(args.frames)]
for command,base in ((native,reference),(rtl,actual)):
    subprocess.run([*command,'--log',str(base)+'.state','--screenshot',str(base)+'.ppm'],
                   env=env,check=True,timeout=max(90,args.frames))
expected=Path(str(reference)+'.state').read_text().splitlines()
observed=Path(str(actual)+'.state').read_text().splitlines()
assert len(expected)==len(observed)==args.frames,(len(expected),len(observed))
for i,(a,b) in enumerate(zip(expected,observed)):
    assert a==b,f'frame {i+1}: native={a}, RTL={b}'
assert Path(str(reference)+'.ppm').read_bytes()==Path(str(actual)+'.ppm').read_bytes(),'VGA RGB mismatch'
if args.frames>=300:
    modes={int(row.split()[2],16)>>24 for row in expected}
    assert {1,2}<=modes,'demo missed play/pause'
    assert any((int(row.split()[2],16)>>12)&4095 for row in expected),'no piece locked'
if args.frames>=600:
    assert any((int(row.split()[2],16)&4095)>=2 for row in expected),'demo missed line clears'
print(f'PASS: Tetris {stem}: {args.frames} whole-game hashes/metrics and complete VGA RGB match')
