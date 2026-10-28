"""Sequential, state-checked 1/2/4-thread benchmarks. No production edits."""
import argparse
import json
import os
from pathlib import Path
import re
import statistics
import signal
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--frames',type=int,default=330)
p.add_argument('--rounds',type=int,default=3)
p.add_argument('--mode',choices=['headless','dummy','desktop'],default='headless')
p.add_argument('--timeout',type=int,default=180)
p.add_argument('--coarse',action='store_true')
p.add_argument('--no-affinity',action='store_true')
args=p.parse_args()
out=Path('build/thread-results')/f'{args.mode}-{args.frames}{"-coarse" if args.coarse else ""}{"-os" if args.no_affinity else ""}'
out.mkdir(parents=True,exist_ok=True)
rows=[]
env=os.environ.copy()
if not args.no_affinity:env['VERILATOR_NUMA_STRATEGY']='none'
env['NVBOARD_HOME']='/home/vero/ysyx2407/ysyx-workbench/nvboard'
if args.mode=='dummy':env['SDL_VIDEODRIVER']='dummy'
baseline=None
for run in range(args.rounds):
    order=[[1,2,4],[4,1,2],[2,4,1]][run%3]
    for threads in order:
        prefix=out/f't{threads}-r{run+1}'
        variant='coarse' if args.coarse and threads>1 else 'bench'
        executable=f'./build/thread-{variant}-{threads}/Vnvboard_top'
        command=['/usr/bin/time','-f','%e %U %S %P','-o',str(prefix)+'.time']
        if not args.no_affinity:command+=['taskset','-c','0,2,4,6']
        command+=[executable,'+rom=build/mario-rv32i/mario.hex','--demo',
                 '--frames',str(args.frames),'--log',str(prefix)+'.state','--screenshot',str(prefix)+'.ppm']
        if args.mode=='headless':command+=['--headless']
        if args.frames>=300:command+=['--verify']
        print('START',args.mode,'threads',threads,'round',run+1,flush=True)
        with open(str(prefix)+'.stdout','w') as stdout,open(str(prefix)+'.stderr','w') as stderr:
            process=subprocess.Popen(command,env=env,stdout=stdout,stderr=stderr,start_new_session=True)
            try:code=process.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGTERM)
                try:process.wait(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
                raise SystemExit(f'TIMEOUT: {prefix}')
        if code:
            raise SystemExit(f'FAIL: {prefix}: exit {code}; see stderr')
        stdout=Path(str(prefix)+'.stdout').read_text()
        match=re.search(r'DONE: (\d+) game frames, (\d+) VGA frames, (\d+) cycles, ([\d.]+)s, ([\d.]+) game fps',stdout)
        if not match:raise SystemExit('Missing completion: '+str(prefix))
        state=Path(str(prefix)+'.state').read_bytes()
        screenshot=Path(str(prefix)+'.ppm').read_bytes()
        if baseline is None:baseline=(state,screenshot,match[3])
        if (state,screenshot,match[3])!=baseline:raise SystemExit('State/pixels/cycle mismatch: '+str(prefix))
        row=dict(threads=threads,run=run+1,frames=int(match[1]),vga_frames=int(match[2]),
                 cycles=int(match[3]),seconds=float(match[4]),fps=float(match[5]),
                 process_time=Path(str(prefix)+'.time').read_text().strip())
        rows.append(row)
        (out/'results.json').write_text(json.dumps(rows,indent=2))
        print('DONE',row,flush=True)
    baseline_seconds=statistics.median(r['seconds'] for r in rows if r['threads']==1)
    for threads in (1,2,4):
        seconds=statistics.median(r['seconds'] for r in rows if r['threads']==threads)
        print(f'MEDIAN t={threads}: {seconds:.2f}s {args.frames/seconds:.2f}fps speedup={baseline_seconds/seconds:.3f}x',flush=True)
