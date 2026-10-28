#!/usr/bin/env python3
"""Visual-content checks, independent of RTL-vs-software equivalence."""
from pathlib import Path
import re
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
chr_data = (root/'sw/mario/generated/chr.bin').read_bytes()
assert len(chr_data) == 8192, 'CHR size'


def decode(tile):
    b = chr_data[tile*16:tile*16+16]
    return [((b[y] >> (7-x)) & 1) | (((b[y+8] >> (7-x)) & 1) << 1)
            for y in range(8) for x in range(8)]


glyphs = [decode(256+i) for i in range(36)]
assert all(any(g) for g in glyphs), 'Missing digit/letter'
assert all(set(g) <= {0, 1} for g in glyphs), 'Font has mixed palette indices (old antialiasing bug)'
assert not any(decode(0x124)) and not any(decode(0xfc)), 'Reserved blank tiles are not blank'

source = (root/'sw/mario/vendor/SuperMarioBros-C/source/SMB/SMBData.cpp').read_text()
body = re.search(r'PlayerGraphicsTable_data\[\] = \{(.*?)\};', source, re.S)[1]
indices = [int(v,16) for v in re.findall(r'0x([0-9a-f]+)', body)]
poses = [indices[i:i+8] for i in range(0,len(indices),8)]
assert len(poses) == 26
for frame, pose in enumerate(poses):
    pixels = sum(sum(p != 0 for p in decode(t)) for t in pose)
    assert pixels >= 48, f'Player pose {frame} is empty/incomplete ({pixels} pixels)'
for t in (0x3a, 0x37, 0x4f):
    assert any(decode(t)), f'Small standing tile {t:x} is blank (old overwrite bug)'

preview = Image.new('RGB', (640,300), (40,40,40))
draw = ImageDraw.Draw(preview)
draw.text((8,4), 'SMB CHR: 36 glyphs / 26 player poses (including standing)', fill='white')
for i, glyph in enumerate(glyphs):
    im = Image.new('RGB',(8,8));im.putdata([(255,255,255) if v else (0,0,0) for v in glyph])
    preview.paste(im.resize((16,16),Image.Resampling.NEAREST),(8+(i%18)*24,26+(i//18)*24))
palette = [(92,148,252),(181,49,32),(107,109,0),(234,158,34)]
for i,pose in enumerate(poses):
    im = Image.new('RGB',(16,32),palette[0])
    for j,t in enumerate(pose):
        tile = Image.new('RGB',(8,8));tile.putdata([palette[v] for v in decode(t)])
        im.paste(tile,((j%2)*8,(j//2)*8))
    x=8+(i%13)*48;y=90+(i//13)*96
    preview.paste(im.resize((32,64),Image.Resampling.NEAREST),(x,y))
    draw.text((x,y+66),str(i),fill='white')
(root/'build').mkdir(exist_ok=True)
preview.save(root/'build/mario-art-preview.png')
print('PASS: 36 single-index font glyphs, 26 nonempty player poses, standing tiles, reserved blanks')
