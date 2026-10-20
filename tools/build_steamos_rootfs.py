#!/usr/bin/env python3
"""Assemble Valve's ARM64 userspace with the shared Piano BSP and boot files."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from build_nyarch_apps import guest_environment
from build_release_rootfs import build_resolver, digest
from prepare_steamos_rootfs import prepare, download
import build_arch_package
import build_arch_ffmpeg
import stage_arch_piano
import prepare_steam_client

ROOT = Path(__file__).resolve().parents[1]


def run(*args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def sensor_sources(output, cfg):
    source = ROOT / cfg['source']
    commit = subprocess.check_output(['git', '-C', source, 'rev-parse', 'HEAD'], text=True).strip()
    if commit != cfg['commit'] or subprocess.check_output(
            ['git', '-C', source, 'status', '--porcelain'], text=True):
        raise ValueError('Run ./build.sh sources to obtain the fixed sensor source')
    shutil.copytree(source, output, ignore=shutil.ignore_patterns('.git'))
    rows = []
    for name in cfg['patches']:
        run('patch', '--batch', '--forward', '-p1', '-i', ROOT / name, cwd=output)
        rows.append({'path': name, 'sha256': digest(ROOT / name)})
    (output / 'SOURCE').write_text(json.dumps(
        {'source_url': cfg['source_url'], 'source_commit': commit, 'patches': rows}, indent=2) + '\n')


def sensor_recipes(output, source):
    shutil.copytree(ROOT / 'linux/bsp/steamos/protobuf-c', output / 'protobuf-c')
    for name in ('fastrpc', 'libssc', 'iio-sensor-proxy'):
        recipe = output / name
        shutil.copytree(ROOT / 'linux/bsp/arch' / name, recipe)
        if name == 'fastrpc':
            continue
        if name == 'libssc':
            filename, pool, checksum = (
                'libssc_0.4.4.orig.tar.gz', 'libs/libssc',
                '716d6bd6b34d2d753060c6b54c9a87e34fae75b724c763bf9ef487efa3621587')
        else:
            filename, pool, checksum = (
                'iio-sensor-proxy_3.9.orig.tar.bz2', 'i/iio-sensor-proxy',
                '19a4ae99b1b8af603fc789841025f9747bf445418d69ec7389e75adb6c8186bf')
        download('https://deb.debian.org/debian/pool/main/' + pool + '/' + filename,
                 recipe / filename, checksum)
        for patch in (source / 'patches' / name).glob('*.patch'):
            shutil.copy2(patch, recipe / patch.name)


def build(args):
    output = args.output.resolve()
    if os.geteuid() or not output.is_relative_to(ROOT / 'build/distros'):
        raise ValueError('Run as root with output below build/distros')
    if output.exists() and not args.resume:
        raise ValueError('Output exists; use --resume only for this same build')
    output.mkdir(parents=True, exist_ok=True)
    cfg = json.loads((ROOT / 'config/release.json').read_text())
    base = ROOT / 'build/steamos-base'
    base_record = prepare(base, workers=32)
    root = output / 'rootfs'
    if not root.exists():
        mount = output / 'base-mount'
        mount.mkdir()
        run('mount', '-o', 'loop,ro', base / 'rootfs.img', mount)
        try:
            root.mkdir()
            run('rsync', '-aHAX', '--numeric-ids', '--filter=-x btrfs.*', str(mount) + '/', root)
        finally:
            run('umount', mount)
        (output / 'BASE.json').write_text(json.dumps(base_record, indent=2) + '\n')
    elif not (output / 'BASE.json').is_file():
        raise ValueError('Existing root has no verified base record')
    if json.loads((output / 'BASE.json').read_text())['base'] != base_record['base']:
        raise ValueError('Existing root belongs to another Valve base image')

    # Preserve the complete Valve Mesa/FEX runtime rather than mixing Debian
    # shared libraries with Arch packages. Hardware support still needs a boot.
    with build_resolver(root), guest_environment(root) as guest:
        guest(['pacman', '-S', '--needed', '--noconfirm', 'base-devel',
               'sddm', 'openssh', 'bluez', 'bluez-utils', 'networkmanager',
               'alsa-utils', 'pipewire', 'pipewire-pulse', 'wireplumber',
               'python', 'python-gobject', 'polkit', 'erofs-utils', 'rsync', 'chromium'])
        if subprocess.run(['chroot', root, 'id', 'piano'], capture_output=True).returncode:
            guest(['usermod', '-l', 'piano', '-d', '/home/piano', '-m', 'steamos'])
            guest(['groupmod', '-n', 'piano', 'steamos'])
        # Keep Arch's standard account policy; the password is documented in
        # README and shared with the other images.
        guest(['usermod', '-aG', 'wheel', 'piano'])
        locale = root / 'etc/locale.gen'
        locale.write_text('en_US.UTF-8 UTF-8\nzh_CN.UTF-8 UTF-8\n')
        guest(['locale-gen'])
    source = output / 'sensor-source'
    if not source.exists():
        sensor_sources(source, cfg['sensors'])
    recipes = output / 'sensor-recipes'
    if not recipes.exists():
        sensor_recipes(recipes, source)
    for name in ('protobuf-c', 'fastrpc', 'libssc', 'iio-sensor-proxy'):
        packages = output / 'packages' / name
        if not (packages / 'manifest.json').is_file():
            build_arch_package.build(root, recipes / name, packages)
    ffmpeg = output / 'packages/ffmpeg'
    if not (ffmpeg / 'SOURCE.json').is_file():
        build_arch_ffmpeg.build(root, ffmpeg, resume=ffmpeg.exists())
    client = output / 'steam-client'
    if not (client / 'SOURCE.json').is_file():
        prepare_steam_client.prepare(root, client)
    if json.loads((client / 'SOURCE.json').read_text()) != json.loads(
            (ROOT / 'config/steam-client.json').read_text()):
        raise ValueError('Cached Steam client differs from config/steam-client.json')

    kernel = ROOT / 'artifacts/kernels/release'
    kernel_source = Path(json.loads((ROOT / 'build/release/source-manifest.json').read_text())['worktree'])
    runtime = output / 'runtime'
    if not (runtime / 'manifest.json').exists():
        run(sys.executable, ROOT / 'tools/build_release_helpers.py',
            '--kernel', kernel, '--source', kernel_source,
            '--kernel-build', ROOT / 'build/kernels/release',
            '--output', runtime, '--sysroot', args.bootstrap_sysroot.resolve())
    # Shared stager installs firmware, matching modules, standard interfaces,
    # input/audio/camera helpers, suspend policy and the LABEL-based initramfs.
    stage_arch_piano.stage(argparse.Namespace(
        rootfs=root, kernel=kernel, runtime=runtime, sensor_source=source,
        bootstrap_sysroot=args.bootstrap_sysroot, desktop='steamos'))
    manifest = output / 'manifest.json'
    record = json.loads(manifest.read_text())
    record.update(base_source=base_record, ffmpeg=json.loads((ffmpeg / 'SOURCE.json').read_text()),
                  steam_client=json.loads((client / 'SOURCE.json').read_text()))
    manifest.write_text(json.dumps(record, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/distros/steamos-piano')
    parser.add_argument('--bootstrap-sysroot', type=Path, default=Path('/'),
                        help='Native ARM64 Debian build environment containing busybox-static')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.worker:
        raise SystemExit(run('unshare', '--mount', '--propagation', 'private',
                             sys.executable, __file__, *sys.argv[1:], '--worker').returncode)
    build(args)
