#!/usr/bin/env python3
"""Build and install the shared FFmpeg BSP patches as a native SteamOS package.

Preserve the base image's codec configuration and dependency list. FFmpeg 7.0
and 7.1 share library SONAMEs; do not replace newer ABI generations with 7.1.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

import prepare_ffmpeg_source as source
from build_nyarch_apps import guest_environment
from build_release_rootfs import build_resolver

ROOT = Path(__file__).resolve().parents[1]


def build(root, output, resume=False):
    root, output = root.resolve(), output.resolve()
    if os.geteuid() or not root.is_relative_to(ROOT / 'build/distros'):
        raise ValueError('Use root and a disposable project rootfs')
    if (output.exists() and not resume) or not output.is_relative_to(ROOT / 'build'):
        raise ValueError('Use a new output below build/')
    installed = subprocess.check_output(['chroot', root, 'ffmpeg', '-version'],
                                        text=True, stderr=subprocess.STDOUT)
    if not re.search(r'^ffmpeg version n?7\.[01](?:[.\s-])', installed):
        raise ValueError('This recipe requires FFmpeg 7.0/7.1; do not downgrade another ABI')
    line = next(line for line in installed.splitlines() if line.startswith('configuration:'))
    options = shlex.split(line.split(':', 1)[1])
    dbpath = subprocess.check_output(['chroot', root, 'pacman-conf', 'DBPath'], text=True).strip()
    records = list((root / dbpath.lstrip('/') / 'local').glob('ffmpeg-*/desc'))
    records = [p for p in records if '%NAME%\nffmpeg\n' in p.read_text()]
    if len(records) != 1:
        raise ValueError('Expected one installed FFmpeg package record')
    text = records[0].read_text()
    def field(name):
        return text.split('%' + name + '%\n', 1)[1].split('\n\n', 1)[0].splitlines()
    deps, provides = field('DEPENDS'), field('PROVIDES')
    epoch = field('VERSION')[0].split(':')[0] if ':' in field('VERSION')[0] else '0'
    output.mkdir(parents=True, exist_ok=resume)
    provenance = (json.loads((output / 'source/SOURCE.json').read_text())
                  if resume else source.prepare(output / 'source'))
    series = json.loads(source.SERIES.read_text())
    expected_patches = [
        {'path': str((source.SERIES.parent / row['path']).relative_to(ROOT)),
         'sha256': row['sha256']} for row in series['series']]
    if (provenance.get('patches') != expected_patches or
            provenance.get('upstream_version') != series['upstream_version']):
        raise ValueError('Cached FFmpeg source differs from the configured patch series')
    # Build from the same patched source used by Debian, inside the target ABI.
    guest_work = root / 'var/tmp/sunuefi-ffmpeg'
    if guest_work.exists() and not resume:
        raise ValueError('Existing FFmpeg build directory must be inspected before retry')
    guest_work.mkdir(parents=True, exist_ok=resume)
    if not (guest_work / 'ffmpeg-source').exists():
        shutil.copytree(output / 'source', guest_work / 'ffmpeg-source')
    quote_array = lambda values: '(' + ' '.join(shlex.quote(v) for v in values) + ')'
    recipe = (
        "pkgname=ffmpeg\npkgver=7.1.5\npkgrel=1.3\n"
        f"epoch={epoch}\npkgdesc='FFmpeg with Piano stateful V4L2 fixes'\n"
        "arch=(aarch64)\nurl=https://ffmpeg.org\nlicense=(GPL-3.0-or-later)\n"
        "options=(!debug !lto)\n"
        f"depends={quote_array(deps)}\nprovides={quote_array(provides)}\n"
        "build() {\n  cd \"$startdir/ffmpeg-source\"\n  ./configure " +
        shlex.join(options) + "\n  make -j4\n}\n"
        "package() {\n  cd \"$startdir/ffmpeg-source\"\n"
        "  make DESTDIR=\"$pkgdir\" install\n"
        "  install -Dm644 COPYING.GPLv3 \"$pkgdir/usr/share/licenses/ffmpeg/COPYING.GPLv3\"\n}\n")
    (guest_work / 'PKGBUILD').write_text(recipe)
    with build_resolver(root), guest_environment(root) as guest:
        guest(['pacman', '-S', '--needed', '--noconfirm', 'base-devel', 'clang',
               'nasm', 'amf-headers', 'avisynthplus', 'ffnvcodec-headers',
               'frei0r-plugins', 'ladspa', 'opencl-headers', 'vulkan-headers',
               *deps])
        if subprocess.run(['chroot', root, 'id', 'sunuefi-ffmpeg'],
                          capture_output=True).returncode:
            guest(['useradd', '-m', 'sunuefi-ffmpeg'])
        guest(['chown', '-R', 'sunuefi-ffmpeg:sunuefi-ffmpeg', '/var/tmp/sunuefi-ffmpeg'])
        guest(['runuser', '-u', 'sunuefi-ffmpeg', '--', 'bash', '-c',
               'cd /var/tmp/sunuefi-ffmpeg && makepkg --noconfirm --nocheck --nodeps'])
        packages = list(guest_work.glob('ffmpeg-*.pkg.tar.zst'))
        if len(packages) != 1:
            raise ValueError('FFmpeg package was not produced')
        package = packages[0]
        guest(['pacman', '-U', '--noconfirm', '/var/tmp/sunuefi-ffmpeg/' + package.name])
        version = guest(['ffmpeg', '-version'], capture_output=True, text=True).stdout
        if '7.1.5' not in version:
            raise ValueError('Installed FFmpeg differs from the built package')
        guest(['userdel', '-r', 'sunuefi-ffmpeg'])
    shutil.copy2(package, output / package.name)
    provenance.update(package_format='pacman', packages_built=True, original_configuration=options,
                      package=package.name, package_sha256=source.sha(package),
                      installed_version=version, device_verified=False)
    (output / 'SOURCE.json').write_text(json.dumps(provenance, indent=2) + '\n')
    shutil.rmtree(guest_work)
    return provenance


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rootfs', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    if not args.worker:
        raise SystemExit(subprocess.run(
            ['unshare', '--mount', '--propagation', 'private',
             sys.executable, __file__, *sys.argv[1:], '--worker']).returncode)
    print(json.dumps(build(args.rootfs, args.output, args.resume), indent=2))
