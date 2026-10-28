"""Offline baking of original PvZ art; never writes the original asset tree.

Only a daylight lawn, pea shooter, normal zombie walk/eat, sun and pea survive
in the runtime pack. Skeleton transforms run here, NOT on the host during play.
"""
import json
import math
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

game_dir = Path(__file__).resolve().parents[1]
source = game_dir/'assets'
out = game_dir/'generated'
# Fail explicitly if an input disappears; never quietly drop an alpha mask.
for name in json.loads((source/'source-manifest.json').read_text())['original_files_used']:
    if not (source/name).is_file():
        raise FileNotFoundError(f'Missing source asset: {source/name}')
out.mkdir(parents=True, exist_ok=True)
filemap = {p.stem.casefold(): p for p in sorted((source/'reanim').iterdir())
           if p.suffix.lower() in ('.jpg','.png') and not p.stem.endswith('_')}
used = set()

def load(path):
    used.add(str(path.relative_to(source)))
    im=Image.open(path).convert('RGBA')
    mask=path.with_name(path.stem+'_.png')
    if path.suffix.lower()=='.jpg' and mask.exists():
        used.add(str(mask.relative_to(source)));im.putalpha(Image.open(mask).convert('L'))
    return im

def skeleton(filename, animation, count, hidden=(),canvas_size=(300,280),origin=(70,45)):
    path=source/'reanim'/filename
    used.add(str(path.relative_to(source)))
    root=ET.fromstring('<root>'+path.read_text(encoding='utf-8-sig')+'</root>')
    tracks=[]
    for track in root.findall('track'):
        name=track.findtext('name')
        state=dict(x=0.,y=0.,kx=0.,ky=0.,sx=1.,sy=1.,f=0.,a=1.,i='')
        frames=[]
        for t in track.findall('t'):
            for item in t:
                if item.tag in state:
                    state[item.tag]=item.text if item.tag=='i' else float(item.text)
            frames.append(dict(state))
        tracks.append((name,frames))
    marker=next(frames for name,frames in tracks if name==animation)
    active=[i for i,s in enumerate(marker) if s['f']>=0]
    if not active: raise RuntimeError(f'No frames for {filename}/{animation}')
    result=[]
    for fi in [active[i*len(active)//count] for i in range(count)]:
        canvas=Image.new('RGBA',canvas_size)
        for name,frames in tracks:
            if name.casefold().startswith(tuple(h.casefold() for h in hidden)): continue
            s=frames[min(fi,len(frames)-1)]
            if not s['i'] or s['f']<0 or s['a']<=0: continue
            key=s['i'].removeprefix('IMAGE_REANIM_').casefold()
            if key not in filemap: raise RuntimeError(f'Unresolved art: {s["i"]}')
            im=load(filemap[key])
            if s['a']<1: im.putalpha(im.getchannel('A').point(lambda a:int(a*s['a'])))
            kx,ky=math.radians(s['kx']),math.radians(s['ky'])
            a,b=math.cos(kx)*s['sx'],-math.sin(ky)*s['sy']
            c,d=math.sin(kx)*s['sx'],math.cos(ky)*s['sy']
            det=a*d-b*c
            if abs(det)<1e-8: continue
            tx,ty=s['x']+origin[0],s['y']+origin[1]
            inverse=(d/det,-b/det,(b*ty-d*tx)/det,
                     -c/det,a/det,(c*tx-a*ty)/det)
            canvas.alpha_composite(im.transform(canvas.size,Image.Transform.AFFINE,inverse,
                                               Image.Resampling.BICUBIC))
        result.append(canvas)
    return result

def normalize(frames,size):
    boxes=[im.getbbox() for im in frames]
    box=(min(b[0] for b in boxes),min(b[1] for b in boxes),
         max(b[2] for b in boxes),max(b[3] for b in boxes))
    return [im.crop(box).resize(size,Image.Resampling.LANCZOS) for im in frames]

# Match Board::DrawBackdrop's tutorial layout: unsodded yard, sod at (239,265)
# in the original image, then crop BOARD_OFFSET=220. Originals remain untouched.
yard=load(source/'images/background1unsodded.jpg')
yard.alpha_composite(load(source/'images/sod1row.jpg'),(239,265))
background=yard.crop((220,0,1020,600)).resize((320,240),Image.Resampling.LANCZOS)
draw=ImageDraw.Draw(background)
draw.rectangle((0,0,319,41),fill=(42,55,25,255))
font_path=str(source/'fonts/DejaVuSans.ttf')
font=ImageFont.truetype(font_path,10)
background.alpha_composite(load(source/'images/SeedBank.png').resize((196,40),Image.Resampling.LANCZOS),(2,0))
draw.text((204,3),'DAY 1 - ONE LANE',font=ImageFont.truetype(font_path,8),fill='#ffed90')
draw.rounded_rectangle((267,15,316,25),radius=2,fill='#76572d',outline='#dac28a')
draw.text((272,14),'MENU',font=ImageFont.truetype(font_path,9),fill='white')
draw.text((204,28),'WAVE',font=ImageFont.truetype(font_path,8),fill='white')
draw.text((248,28),'/',font=font,fill='white')
draw.text((243,228),'LEVEL 1-1',font=ImageFont.truetype(font_path,9),fill='#fff4b4',stroke_width=1,stroke_fill='#302814')

title=load(source/'images/titlescreen.jpg').resize((320,240),Image.Resampling.LANCZOS)
title.alpha_composite(load(source/'images/PvZ_Logo.jpg').resize((280,46),Image.Resampling.LANCZOS),(20,6))
title.alpha_composite(load(source/'images/LoadBar_dirt.png').resize((188,22),Image.Resampling.LANCZOS),(66,211))
title.alpha_composite(load(source/'images/LoadBar_grass.png').resize((188,22),Image.Resampling.LANCZOS),(66,208))
td=ImageDraw.Draw(title);td.text((91,215),'CLICK TO START',font=ImageFont.truetype(font_path,14),fill='#ffffa0',stroke_width=1,stroke_fill='#243316')
# Static final-open selector pose. In the original engine these layers are
# separately held/render-grouped; sampling anim_idle alone hides the backdrop.
selector=load(source/'reanim/SelectorScreen_BG.jpg').resize((800,600),Image.Resampling.BICUBIC)
for filename,pos in [('SelectorScreen_BG_Center.jpg',(80,250)),
                     ('SelectorScreen_BG_Left.jpg',(0,-80)),
                     ('SelectorScreen_BG_Right.jpg',(71,41)),
                     ('SelectorScreen_StartAdventure_Button1.png',(405,65)),
                     ('SelectorScreen_Survival_button.png',(406,173)),
                     ('SelectorScreen_Challenges_button.png',(410,258)),
                     ('SelectorScreen_Vasebreaker_button.png',(410,342))]:
    selector.alpha_composite(load(source/'reanim'/filename),pos)
selector=selector.resize((320,240),Image.Resampling.LANCZOS)
locked=Image.new('RGBA',(320,240));ld=ImageDraw.Draw(locked)
ld.rectangle((163,78,302,174),fill=(0,0,0,65));selector.alpha_composite(locked)
selector.alpha_composite(load(source/'images/PvZ_Logo.jpg').resize((136,23),Image.Resampling.LANCZOS),(5,4))
sd=ImageDraw.Draw(selector)
for y in [89,122,157]:sd.text((277,y),'LOCK',font=ImageFont.truetype(font_path,7),fill='#ded0ad')
sd.text((8,216),'RV32I / ADVENTURE 1-1 / 1-2',font=ImageFont.truetype(font_path,8),fill='#fff5c4',stroke_width=1,stroke_fill='#172718')
for label,x in [('HELP',204),('ABOUT',253)]:
    sd.rounded_rectangle((x,217,x+43,236),radius=3,fill='#5a503d',outline='#cdb889')
    sd.text((x+4,222),label,font=ImageFont.truetype(font_path,9),fill='#fff3bd')

plant=normalize(skeleton('PeaShooter.reanim','anim_full_idle',8,('anim_blink',)),(30,36))
card=load(source/'images/SeedPacket_Larger.png').resize((28,38),Image.Resampling.LANCZOS)
card.alpha_composite(plant[0].resize((19,22),Image.Resampling.LANCZOS),(4,7))
ImageDraw.Draw(card).text((7,27),'100',font=ImageFont.truetype(font_path,8),fill='#152712')
background.alpha_composite(card,(40,2))
yard3=load(source/'images/background1unsodded.jpg')
yard3.alpha_composite(load(source/'images/sod3row.jpg'),(235,149))
level2=yard3.crop((220,0,1020,600)).resize((320,240),Image.Resampling.LANCZOS)
level2.alpha_composite(background.crop((0,0,320,42)),(0,0))
l2d=ImageDraw.Draw(level2);l2d.rectangle((200,0,319,13),fill=(42,55,25,255))
l2d.text((204,3),'DAY 2 - THREE LANES',font=ImageFont.truetype(font_path,8),fill='#ffed90')
l2d.text((243,228),'LEVEL 1-2',font=ImageFont.truetype(font_path,9),fill='#fff4b4',stroke_width=1,stroke_fill='#302814')
hidden=('anim_cone','anim_bucket','anim_screendoor','Zombie_flaghand','Zombie_duckytube',
        'Zombie_whitewater','anim_tongue','Zombie_mustache','Zombie_futureglasses',
        'Zombie_outerarm_screendoor','Zombie_innerarm_screendoor','anim_head2')
walk=skeleton('Zombie.reanim','anim_walk',8,hidden)
eat=skeleton('Zombie.reanim','anim_eat',8,hidden)
zombies=normalize(walk+eat,(32,56))
sun=load(source/'reanim/Sun2.png').resize((18,18),Image.Resampling.LANCZOS)
sun.alpha_composite(load(source/'reanim/Sun1.png').resize((10,10),Image.Resampling.LANCZOS),(4,4))
pea=load(source/'images/ProjectilePea.png').resize((8,8),Image.Resampling.LANCZOS)
cursor=Image.new('RGBA',(1,1)) # retired keyboard cursor; retain enum numbering
def text_image(text,color):
    im=Image.new('RGBA',(180,30),(22,30,12,240))
    d=ImageDraw.Draw(im);d.rectangle((0,0,179,29),outline=color,width=2)
    d.text((10,6),text,font=ImageFont.truetype(font_path,14),fill=color)
    return im
sprites=plant+zombies+[sun,pea,cursor,text_image('LEVEL COMPLETE!','#fff455'),
                                   text_image('ZOMBIES ATE YOU!','#ffaaaa'),text_image('GAME PAUSED','white')]
for n in range(10):
    im=Image.new('RGBA',(7,12));ImageDraw.Draw(im).text((0,-1),str(n),font=ImageFont.truetype(font_path,11),fill='#fff5cc');sprites.append(im)
selected=Image.new('RGBA',(28,38));ImageDraw.Draw(selected).rectangle((0,0,27,37),outline='#fff455',width=2)
dimmed=card.copy();dimmed.alpha_composite(Image.new('RGBA',card.size,(0,0,0,150)))
sprites += [selected,dimmed]
# Shared original tombstone panel, with independently composited text/buttons.
panel=load(source/'images/options_menuback.jpg').resize((180,166),Image.Resampling.LANCZOS)
sprites.append(panel)
for label in ['BACK TO GAME','RESTART LEVEL','MAIN MENU','PLAY AGAIN','BACK']:
    button=Image.new('RGBA',(140,20),(77,69,49,255));bd=ImageDraw.Draw(button)
    bd.rounded_rectangle((0,0,139,19),radius=3,outline='#b8af7c',width=2)
    f=ImageFont.truetype(font_path,11);box=bd.textbbox((0,0),label,font=f)
    bd.text(((140-box[2])/2,3),label,font=f,fill='#f6e6b4');sprites.append(button)
help_text=Image.new('RGBA',(160,58));hd=ImageDraw.Draw(help_text)
for i,line in enumerate(['Click card, then lawn.','Click suns to collect.','Right click cancels.','Adventure 1-1 and 1-2.']):
    hd.text((1,i*14),line,font=ImageFont.truetype(font_path,10),fill='#fff0c6')
sprites.append(help_text)
dave=normalize(skeleton('CrazyDave.reanim','anim_blahblah',4,canvas_size=(700,700),origin=(200,100)),(80,116))
sprites+=dave
for lines in [('Hey, neighbor!','Three rows this time.','Defend every lane!'),
              ('Plant in all three rows.','Click suns for more.','Good luck, neighbor!')]:
    speech=Image.new('RGBA',(180,64),(250,242,211,255));sp=ImageDraw.Draw(speech)
    sp.rectangle((0,0,179,63),outline='#705a32',width=2)
    for i,line in enumerate(lines):sp.text((7,5+14*i),line,font=ImageFont.truetype(font_path,11),fill='#30271c')
    sp.text((8,49),'CLICK: NEXT / RIGHT: SKIP',font=ImageFont.truetype(font_path,8),fill='#6b5531')
    sprites.append(speech)
for filename in ['StartReady.png','StartSet.png','StartPlant.png']:
    sprites.append(load(source/'reanim'/filename).resize((100,36),Image.Resampling.LANCZOS))
next_button=Image.new('RGBA',(140,20),(77,69,49,255));nd=ImageDraw.Draw(next_button)
nd.rounded_rectangle((0,0,139,19),radius=3,outline='#b8af7c',width=2)
nd.text((24,3),'NEXT LEVEL',font=ImageFont.truetype(font_path,11),fill='#f6e6b4');sprites.append(next_button)

# Global 255-color palette; index 0 is strictly transparent, never opaque black.
samples=Image.new('RGB',(1280,600),(40,60,20))
for i,im in enumerate([background,title,selector,level2]):samples.paste(im.convert('RGB'),(i*320,0))
for i,im in enumerate(sprites):
    thumb=im.copy();thumb.thumbnail((60,60));samples.paste(thumb,((i%10)*64,240+(i//10)*60),thumb)
pal=samples.quantize(colors=255,method=Image.Quantize.MEDIANCUT)
palette=[(0,0,0)]+[tuple(pal.getpalette()[i:i+3]) for i in range(0,255*3,3)]
# Separate dark HUD digits: keep the existing palette and light wave-counter
# glyphs unchanged. Palette index 0 remains transparency, not opaque black.
for n in range(10):
    im=Image.new('RGBA',(7,12))
    ImageDraw.Draw(im).text((0,-1),str(n),font=ImageFont.truetype(font_path,11),fill='#000000')
    sprites.append(im)
def encode(im,transparent=False):
    indices=im.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).tobytes()
    alpha=im.getchannel('A').tobytes()
    return bytes(0 if transparent and a<80 else v+1 for v,a in zip(indices,alpha))
bg=encode(background);atlas=bytearray();descriptors=[]
for im in sprites:
    descriptors.append((len(atlas),im.width,im.height));atlas.extend(encode(im,True))
if len(atlas)>262144: raise RuntimeError('Atlas exceeds hardware capacity')
def words(data):
    data=bytes(data)+bytes((-len(data))%4)
    return struct.unpack('<'+'I'*(len(data)//4),data)
def array(name,values):
    rows=[','.join(f'0x{v:08x}u' for v in values[i:i+8]) for i in range(0,len(values),8)]
    return 'static const uint32_t '+name+'[] = {\n'+',\n'.join(rows)+'\n};\n'
header='#pragma once\n#include <stdint.h>\n'+array('pvz_background',words(bg))+array('pvz_title',words(encode(title)))+array('pvz_selector',words(encode(selector)))+array('pvz_level2',words(encode(level2)))+array('pvz_atlas',words(atlas))
header+=array('pvz_palette',[(r<<16)|(g<<8)|b for r,g,b in palette])
header+='struct Asset { uint32_t offset; uint16_t w,h; };\nstatic const Asset pvz_assets[] = {\n'
header+=',\n'.join('{%du,%d,%d}'%d for d in descriptors)+'\n};\n'
header+='enum { PLANT=0, WALK=8, EAT=16, SUN=24, PEA=25, CURSOR=26, WIN=27, LOSE=28, PAUSE=29, DIGIT=30, CARD_SELECT=40, CARD_DIM=41, MENU_PANEL=42, RESUME_BUTTON=43, RESTART_BUTTON=44, MENU_BUTTON=45, AGAIN_BUTTON=46, BACK_BUTTON=47, HELP_TEXT=48, DAVE=49, DAVE_TEXT=53, READY_WORD=55, NEXT_BUTTON=58, SUN_DIGIT=59 };\n'
(out/'assets.h').write_text(header)
(out/'background.bin').write_bytes(bg);(out/'atlas.bin').write_bytes(atlas)
(out/'palette.bin').write_bytes(bytes(c for rgb in palette for c in rgb))
preview=Image.new('RGBA',(640,360),(90,110,85));preview.paste(background,(0,0))
for i,im in enumerate(sprites[:30]): preview.alpha_composite(im,(330+(i%8)*38,(i//8)*70))
preview.save(out/'preview.png')
title.save(out/'title-preview.png');selector.save(out/'selector-preview.png');background.save(out/'yard-preview.png')
level2.save(out/'level2-preview.png');dave[0].save(out/'dave-preview.png')
manifest=dict(source='https://github.com/FregD156/PvZ_Assets',commit='8ba94e95cfe7d0de5682f8258d3b932dbdac885c',
              original_files_used=sorted(used),background_bytes=len(bg),atlas_bytes=len(atlas),palette_bytes=768,
              runtime_art_bytes=4*len(bg)+len(atlas)+768,background_banks=4,dave_frames=4,descriptors=descriptors,
              note='Original proprietary extracted art, NOT an open-art license; offline resized/palette-quantized animation samples.')
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(f'PVZ runtime art: {manifest["runtime_art_bytes"]} bytes; {len(sprites)} sprites; originals unchanged')
