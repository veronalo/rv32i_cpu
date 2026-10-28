#!/usr/bin/env python3
"""Import exact SMB CHR indices, without lossy sprite-sheet remapping.

--rom-c reads the user's existing litenes C array and caches ONLY its 8 KiB CHR.
--rom imports a user-supplied iNES file. With neither, use the cached CHR.
No ROM is downloaded; the original ysyx workspace is never modified.
"""
import argparse
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CACHE = ROOT / 'assets/smb-litenes.chr'


def extract_chr(rom):
    if len(rom) < 16 or rom[:4] != b'NES\x1a':
        raise ValueError('Expected an iNES header')
    if rom[5] != 1:
        raise ValueError('SMB expects exactly one 8 KiB CHR bank')
    offset = 16 + (512 if rom[6] & 4 else 0) + rom[4] * 16384
    data = rom[offset:offset + 8192]
    if len(data) != 8192:
        raise ValueError('Truncated CHR bank')
    return data


def read_rom_c(path):
    source = Path(path).read_text()
    match = re.search(r'\brom_mario_nes\s*\[\s*\]\s*=\s*\{(.*?)\}', source, re.S)
    if not match:
        raise ValueError('No rom_mario_nes[] initializer found')
    body = re.sub(r'/\*.*?\*/|//[^\n]*', '', match[1], flags=re.S)
    return bytes(int(v, 16) for v in re.findall(r'\b0x([0-9a-fA-F]{1,2})\b', body))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group()
    source.add_argument('--rom', type=Path)
    source.add_argument('--rom-c', type=Path)
    args = p.parse_args()
    try:
        if args.rom_c:
            data = extract_chr(read_rom_c(args.rom_c))
            CACHE.write_bytes(data)
            origin = f'local litenes C array: {args.rom_c}'
        elif args.rom:
            data = extract_chr(args.rom.read_bytes())
            CACHE.write_bytes(data)
            origin = f'user-provided iNES: {args.rom}'
        else:
            if not CACHE.exists():
                p.error('No cached CHR. Import once with --rom-c /path/to/litenes/src/mario-rom.c or --rom /path/to/smb.nes')
            data = CACHE.read_bytes()
            origin = 'local litenes CHR cache'
        if len(data) != 8192:
            raise ValueError('CHR must be exactly 8192 bytes')
    except (OSError, ValueError) as error:
        p.error(str(error))

    out = ROOT / 'generated'
    out.mkdir(exist_ok=True)
    (out / 'chr.bin').write_bytes(data)
    (out / 'chr_data.h').write_text(
        '#pragma once\n#include <stdint.h>\nstatic const uint8_t mario_chr[8192] = {\n' +
        '\n'.join(','.join(str(v) for v in data[i:i+32]) + ',' for i in range(0,8192,32)) + '\n};\n')
    (out / 'chr-source.txt').write_text(f'{origin}\nSHA256 {hashlib.sha256(data).hexdigest()}\n')
    print(f'CHR: {len(data)} bytes; {origin}; SHA256 {hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    main()
