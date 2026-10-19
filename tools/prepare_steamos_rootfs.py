#!/usr/bin/env python3
"""Fetch the fixed Valve ARM64 image using SteamOS-ARM-Port's pinned extractor.

This stage downloads and verifies a filesystem image, not a disk installer.
It never modifies a device, bootloader, partition table, or running rootfs.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def download(url, path, expected):
    if not path.exists():
        partial = path.with_suffix(path.suffix + '.partial')
        with urllib.request.urlopen(url, timeout=60) as source, partial.open('wb') as target:
            shutil.copyfileobj(source, target)
        if digest(partial) != expected:
            raise ValueError('Download checksum mismatch: ' + url)
        partial.replace(path)
    if digest(path) != expected:
        raise ValueError('Cached input checksum mismatch: ' + str(path))


def prepare(output, workers=8):
    output = output.resolve()
    if not output.is_relative_to(ROOT / 'build'):
        raise ValueError('Use an output directory under build/')
    cfg = json.loads((ROOT / 'config/steamos.json').read_text())
    base, ref = cfg['base'], cfg['reference']
    output.mkdir(parents=True, exist_ok=True)
    bundle = output / 'base.raucb'
    download(base['bundle_url'], bundle, base['bundle_sha256'])
    extractor = output / 'extract_rootfs.py'
    url = (ref['repository'].replace('github.com', 'raw.githubusercontent.com') +
           '/' + ref['commit'] + '/' + ref['extractor'])
    download(url, extractor, ref['extractor_sha256'])
    # Only these two known files are needed from the RAUC squashfs payload.
    unpacked = output / 'bundle'
    if not unpacked.exists():
        subprocess.run(['unsquashfs', '-no-progress', '-d', str(unpacked),
                        str(bundle), 'manifest.raucm', 'rootfs.img.caibx'], check=True)
    manifest = configparser.ConfigParser()
    manifest.read(unpacked / 'manifest.raucm')
    if (manifest['update']['compatible'] != 'steamos-aarch64' or
            manifest['image.rootfs']['sha256'] != base['rootfs_sha256'] or
            int(manifest['image.rootfs']['size']) != base['rootfs_bytes']):
        raise ValueError('Valve rootfs manifest differs from the selected source')
    image = output / 'rootfs.img'
    if not image.exists():
        partial = output / 'rootfs.img.partial'
        command = [sys.executable, str(extractor), '--caibx',
                   str(unpacked / 'rootfs.img.caibx'), '--output', str(partial),
                   '--workers', str(workers), '--expected-sha256', base['rootfs_sha256']]
        for store in base['stores']:
            command += ['--store', store]
        subprocess.run(command, check=True)
        partial.replace(image)
    if image.stat().st_size != base['rootfs_bytes'] or digest(image) != base['rootfs_sha256']:
        raise ValueError('Assembled Valve image differs from its manifest')
    record = {'status': 'VERIFIED_VALVE_ARM64_BASE_NOT_PIANO_IMAGE',
              'base': base, 'reference': ref, 'image': str(image),
              'device_modified': False}
    (output / 'SOURCE.json').write_text(json.dumps(record, indent=2) + '\n')
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/steamos-base')
    parser.add_argument('--workers', type=int, default=8)
    args = parser.parse_args()
    if not 1 <= args.workers <= 32:
        parser.error('workers must be 1..32')
    print(json.dumps(prepare(args.output, args.workers), indent=2))
