"""Summarize synthetic RTL memory loops; numbers are NOT actual PvZ FPS."""
import csv
import json
from pathlib import Path
from statistics import median

root=Path('build/pvz-feasibility')
runs=[]
for repeat in range(1,4):
    with (root/f'blit-{repeat}.csv').open() as f:
        runs.append({int(row['phase']):row for row in csv.DictReader(f)})
rows=[]
for phase,name,pixels in [(3,'copy_800x600',480000),(4,'half_blend_800x600',480000),
                          (5,'copy_400x300',120000),(6,'half_blend_400x300',120000),
                          (7,'copy_320x240',76800),(8,'half_blend_320x240',76800)]:
    cycles=[int(run[phase]['cycles']) for run in runs]
    if len(set(cycles))!=1:
        raise RuntimeError(f'Non-deterministic phase cycles: {phase}')
    times=[float(run[phase]['host_ms']) for run in runs]
    ms=median(times)
    rows.append(dict(test=name,pixels=pixels,cycles=cycles[0],
                     cycles_per_pixel=cycles[0]/pixels,host_ms=times,median_ms=ms,
                     primitive_only_passes_per_second=1000/ms))
report=dict(note='Synthetic RAM loops with CPU+SoC+blank VGA RTL, no NVBoard UI, no PvZ logic. '
                 'Primitive-only rate is NOT a measured game frame rate.',results=rows)
(root/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
for row in rows:
    print(f"{row['test']}: {row['cycles']} cycles, median {row['median_ms']:.3f} ms, "
          f"{row['primitive_only_passes_per_second']:.2f} passes/s (NOT game FPS)")
