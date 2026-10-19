#!/usr/bin/env python3
"""Build the native ARM64 Steam bootstrap from pinned public CDN packages."""
import concurrent.futures
import json
from pathlib import Path
import shutil
import stat
import subprocess
import zipfile

from prepare_steamos_rootfs import download

ROOT = Path(__file__).resolve().parents[1]


def prepare(root, output):
    cfg = json.loads((ROOT / 'config/steam-client.json').read_text())
    cache = output / 'downloads'
    tree = output / 'client'
    cache.mkdir(parents=True, exist_ok=True)
    if tree.exists():
        shutil.rmtree(tree)
    tree.mkdir()
    def fetch(item):
        name, row = item
        path = cache / row['file']
        download('https://client-update.akamai.steamstatic.com/' + row['file'],
                 path, row['sha2'])
        if path.stat().st_size != int(row['size']):
            raise ValueError('Steam package size mismatch: ' + name)
        return path
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        archives = list(pool.map(fetch, cfg['packages'].items()))
    for archive in archives:
        with zipfile.ZipFile(archive) as source:
            for entry in source.infolist():
                # Valve's cross-platform ZIPs use backslashes even for Linux.
                name = Path(entry.filename.replace('\\', '/'))
                if name.is_absolute() or '..' in name.parts:
                    raise ValueError('Invalid Steam archive path')
                target = tree / name
                if entry.is_dir() or entry.filename.endswith('\\'):
                    target.mkdir(parents=True, exist_ok=True)
                    continue
                # Do not let an earlier archive's symlink redirect extraction.
                if target.is_symlink() or any(p.is_symlink() for p in target.parents if p != tree):
                    raise ValueError('Steam archive writes through a symlink')
                target.parent.mkdir(parents=True, exist_ok=True)
                mode = entry.external_attr >> 16
                if stat.S_ISLNK(mode):
                    link = source.read(entry).decode()
                    resolved = (target.parent / link).resolve()
                    if Path(link).is_absolute() or not resolved.is_relative_to(tree.resolve()):
                        raise ValueError('Steam archive link escapes its root')
                    target.symlink_to(link)
                    continue
                with source.open(entry) as incoming, target.open('wb') as outgoing:
                    shutil.copyfileobj(incoming, outgoing)
                with target.open('rb') as stream:
                    elf = stream.read(4) == b'\x7fELF'
                target.chmod(0o755 if elf or name.suffix == '.sh' or mode & 0o111 else 0o644)
    for required in ('steam.sh', 'steamrtarm64/steam', 'steamrtarm64/steamui.so'):
        if not (tree / required).is_file():
            raise ValueError('Native client is incomplete: ' + required)
    (tree / 'package').mkdir(exist_ok=True)
    (tree / 'package/beta').write_text(cfg['channel'] + '\n')
    destination = root / 'usr/lib/steam/bootstraplinux_steamrtarm64.tar.xz'
    subprocess.run(['tar', '--sort=name', '--mtime=@0', '--owner=0', '--group=0',
                    '-I', 'xz -T4 -1', '-cf', str(destination), '-C', str(tree), '.'], check=True)
    # The Frame tarball is superseded by this complete handheld bootstrap.
    (root / 'usr/lib/steam/steam.tar.zst').unlink(missing_ok=True)
    shutil.rmtree(tree)
    (output / 'SOURCE.json').write_text(json.dumps(cfg, indent=2) + '\n')
    return cfg
