#!/usr/bin/env python3
"""Pack the current CHR for upstream's fixed-offset loader; preserve user config."""
from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--out', type=Path, default=Path('build/mario-native'))
args = p.parse_args()
data = (Path(__file__).resolve().parent / 'generated/chr.bin').read_bytes()
if len(data) != 8192:
    p.error('CHR must be 8192 bytes; run make mario-assets first')
args.out.mkdir(parents=True, exist_ok=True)
# Graphics container only. No original PRG/game ROM is copied.
(args.out / 'graphics.bin').write_bytes(bytes(16 + 32768) + data)
config = args.out / 'smbc.conf'
if not config.exists():
    config.write_text('[game]\nrom_file=graphics.bin\nframe_rate=60\n'
                      '[video]\nscale=3\nscanlines=0\nvsync=0\n'
                      '[audio]\nenabled=0\n')
print('Native graphics updated; existing smbc.conf preserved:', args.out)
