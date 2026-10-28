#!/usr/bin/env python3
"""Remove only project-owned, reproducible build outputs (never source assets)."""
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
TARGETS = ('build', 'sw/mario/generated', 'sw/PvZ-simplized/generated')
PRESERVE = ('build/mario-native/smbc.conf',)


def clean():
    # Old layouts stored a downloaded binary and player saves in build/. Never
    # silently destroy those if this cleaner is used on an unmigrated checkout.
    for legacy in ('build/pvz-native', 'build/pvz-feasibility/native-userdata'):
        if (ROOT / legacy).exists():
            raise SystemExit(f'Refusing clean: first move {legacy} outside build/ (binary/saves).')
    paths = [ROOT / relative for relative in TARGETS]
    # Do not resolve a symlink into somebody else's directory, even for outputs.
    for path in paths:
        if path.is_symlink():
            raise SystemExit(f'Refusing clean of symlink: {path}')
        for parent in path.parents:
            if parent == ROOT:
                break
            if parent.is_symlink():
                raise SystemExit(f'Refusing clean through symlink: {parent}')
    configs = {}
    for relative in PRESERVE:
        path = ROOT / relative
        if path.is_file() and not path.is_symlink():
            configs[relative] = path.read_bytes()
    for path in paths:
        if path.exists():
            shutil.rmtree(path)
            print(f'Removed {path.relative_to(ROOT)}/')
    for relative, data in configs.items():
        path = ROOT / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(f'Preserved {relative}')
    # Python bytecode is also reproducible; vendor/reference trees are untouched.
    for base in (ROOT / 'sim', ROOT / 'sw/mario', ROOT / 'sw/PvZ-simplized/mini'):
        for path in base.rglob('__pycache__'):
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
    print('Sources, source assets, external NVBoard and ../../pvz-native/ preserved.')


if __name__ == '__main__':
    clean()
