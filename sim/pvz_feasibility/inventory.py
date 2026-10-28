"""Asset footprint only; decoded RGBA sum is NOT peak game resident memory."""
import json
import os
from collections import Counter
from pathlib import Path
from PIL import Image

archive = Path(os.environ.get('PVZ_ARCHIVE_DIR', str(Path(__file__).resolve().parents[4] / 'pvz-native')))
root = archive / 'PvZ-Assets-original'
if not root.is_dir():
    raise SystemExit(f'Full reference assets not found: {root}; set PVZ_ARCHIVE_DIR.')
files = [p for p in root.rglob('*') if p.is_file() and '.git' not in p.parts]
images, failed = [], []
for path in files:
    if path.suffix.lower() not in {'.png', '.jpg', '.jpeg', '.gif', '.bmp'}:
        continue
    try:
        with Image.open(path) as im:
            w, h = im.size
            images.append(dict(path=str(path.relative_to(root)), width=w, height=h,
                               rgba_bytes=w*h*4, stored_bytes=path.stat().st_size))
    except Exception as error:
        failed.append(dict(path=str(path), error=str(error)))
report = dict(files=len(files), stored_bytes=sum(p.stat().st_size for p in files),
              image_count=len(images), rgba_all_images_bytes=sum(p['rgba_bytes'] for p in images),
              extensions=dict(Counter(p.suffix.lower() for p in files)),
              largest_images=sorted(images,key=lambda p:p['rgba_bytes'],reverse=True)[:12],
              unreadable_images=failed,
              note='All-images sum, not simultaneous residency, not measured game RAM usage.')
out=Path('build/pvz-feasibility');out.mkdir(parents=True,exist_ok=True)
(out/'assets.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
