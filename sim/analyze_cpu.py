"""Read gperftools legacy CPU samples and symbolize local ELF addresses."""
from pathlib import Path
from collections import Counter, defaultdict
import subprocess
import struct
import re
import json

blob=Path('build/profiling/mario.cpu').read_bytes()
header=struct.unpack_from('<5Q',blob)
assert header[:3]==(0,3,0), header
position=40
samples=Counter()
while True:
    count,depth=struct.unpack_from('<2Q',blob,position);position+=16
    assert depth<1000
    pcs=struct.unpack_from('<%dQ'%depth,blob,position);position+=depth*8
    if count==0 and pcs==(0,):break
    if depth:samples[pcs[0]]+=count
maps=[]
for line in blob[position:].decode().splitlines():
    fields=line.split(None,5)
    if len(fields)<6:continue
    bounds,perms,offset,_,_,path=fields
    start,end=[int(value,16) for value in bounds.split('-')]
    maps.append((start,end,int(offset,16),path))

groups=defaultdict(dict)
for pc,count in samples.items():
    for start,end,offset,path in maps:
        if start<=pc<end:
            groups[path][pc-start+offset]=count
            break
    else:groups['[unmapped]'][pc]=count

total=sum(samples.values());functions=Counter();locations=Counter();rows=[]
for path,values in groups.items():
    if not Path(path).is_file():
        functions[path]+=sum(values.values());continue
    output=subprocess.check_output(['addr2line','-a','-f','-C','-i','-e',path],
        input='\n'.join(hex(pc) for pc in values)+'\n',text=True)
    chunks=re.split(r'^0x([0-9a-f]+)\n',output,flags=re.M)
    for k in range(1,len(chunks),2):
        pc=int(chunks[k],16);lines=chunks[k+1].splitlines()
        chain=list(zip(lines[0::2],lines[1::2]));count=values[pc]
        fn=chain[0][0] if chain else 'unknown'
        functions[Path(path).name+' :: '+fn]+=count
        for _,location in chain:
            if 'Vnvboard_top___024root__0.cpp:' in location:
                locations[location]+=count;break
        rows.append(dict(module=path,offset=pc,count=count,chain=chain))
print('TOTAL',total,'period_us',header[3])
print('MODULES')
for path,values in sorted(groups.items(),key=lambda item:-sum(item[1].values())):
    n=sum(values.values());print(f'{n/total:7.2%} {n:6d} {path}')
print('LEAF FUNCTIONS (includes inlined functions)')
for fn,count in functions.most_common(20):print(f'{count/total:7.2%} {count:6d} {fn}')
print('GENERATED MODEL HOT LINES')
for loc,count in locations.most_common(18):print(f'{count/total:7.2%} {count:6d} {loc}')
Path('build/profiling/samples.json').write_text(json.dumps(rows,indent=2))
