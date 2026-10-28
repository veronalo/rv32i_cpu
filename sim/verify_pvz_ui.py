"""Headless UI snapshots and real RTL state checks; never moves the desktop mouse."""
import os
from pathlib import Path
import subprocess

folder=Path('build/pvz-ui-tests')
folder.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
env.update(SDL_VIDEODRIVER='dummy',NVBOARD_RENDERER='software')
cases=[('title',2,[],[],{4}),
       ('selector',1,['--mouse-demo'],['--mouse-demo'],{5}),
       ('menu',120,['--ui-demo'],['--ui-demo'],{0,3,5,6}),
       ('menu-sdl',120,['--ui-demo'],['--mouse-sdl-demo','--ui-demo'],{0,3,5,6})]
for name,frames,native_args,rtl_args,modes in cases:
    ref=folder/(name+'-reference')
    rtl=folder/(name+'-rtl')
    with (folder/(name+'.log')).open('w') as log:
        def run(command):
            subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT,env=env,timeout=90)
        run(['./build/pvz-simplized-native/pvz','--headless',*native_args,'--frames',str(frames),
             '--log',str(ref)+'.state','--screenshot',str(ref)+'.ppm'])
        args=rtl_args if name=='menu-sdl' else ['--headless',*rtl_args]
        run(['./build/pvz-nvboard/Vnvboard_top','+rom=build/pvz-rv32i/pvz.hex',*args,'--fps','0',
             '--frames',str(frames),'--log',str(rtl)+'.state','--screenshot',str(rtl)+'.ppm'])
        run(['python3','sim/verify_pvz.py',str(ref)+'.state',str(rtl)+'.state','--allow-incomplete'])
    actual_modes={(int(row.split()[2],16)>>16)&15 for row in Path(str(ref)+'.state').read_text().splitlines()}
    assert modes<=actual_modes,(name,actual_modes)
    assert Path(str(ref)+'.ppm').read_bytes()==Path(str(rtl)+'.ppm').read_bytes(),name+' RGB mismatch'
    print('PASS:',name,'state/mode transitions and final RGB match',flush=True)
