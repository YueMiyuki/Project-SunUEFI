#!/usr/bin/env python3
"""Build a project PKGBUILD in the selected ARM64 root, then install its packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from build_nyarch_apps import ROOT, guest_environment, run
from build_release_rootfs import build_resolver


def build(root, recipe, output, resume=False):
    root, recipe, output = root.resolve(), recipe.resolve(), output.resolve()
    if os.geteuid() or not root.is_relative_to(ROOT / 'build/distros'):
        raise ValueError('Use root and a project-built ARM64 rootfs')
    if not (recipe / 'PKGBUILD').is_file():
        raise ValueError('The recipe directory must include PKGBUILD and local sources')
    folder = '/var/tmp/sunuefi-package-build'
    work = root / folder.lstrip('/')
    if work.exists() and not resume:
        raise ValueError('A package build already exists; use --resume for the same recipe')
    if work.exists():
        if (work / 'PKGBUILD').read_bytes() != (recipe / 'PKGBUILD').read_bytes():
            raise ValueError('Resume requires the same package recipe')
    else:
        shutil.copytree(recipe, work, symlinks=True)
    with build_resolver(root), guest_environment(root) as guest:
        dependencies = guest(
            ['bash', '-c', 'source "$1/PKGBUILD"; printf "%s\\n" "${makedepends[@]}" "${depends[@]}"',
             'dependencies', folder], capture_output=True, text=True).stdout.splitlines()
        dependencies = list(dict.fromkeys(['base-devel', *[p for p in dependencies if p]]))
        # A locally built dependency (for example libssc on SteamOS) need not
        # exist in the base's repositories. Resolve installed versions first.
        missing = subprocess.run(['chroot', root, 'pacman', '-T', *dependencies],
                                 capture_output=True, text=True)
        if missing.returncode not in (0, 127):
            raise RuntimeError(missing.stderr)
        if missing.returncode:
            guest(['pacman', '-S', '--needed', '--noconfirm', *missing.stdout.splitlines()])
        if subprocess.run(['chroot', root, 'id', '-u', 'sunuefi-package'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
            guest(['useradd', '-m', '-u', '60002', 'sunuefi-package'])
        guest(['chown', '-R', 'sunuefi-package:sunuefi-package', folder])
        if not (work / '.sunuefi-built').exists():
            flags = ' --noextract' if resume and (work / 'src').is_dir() else ''
            guest(['runuser', '-u', 'sunuefi-package', '--', 'bash', '-c',
                   'cd "$1"; makepkg --force --noconfirm --nocheck --nodeps' + flags, 'makepkg', folder])
            (work / '.sunuefi-built').write_text('makepkg completed\n')
        packages = [p for p in work.glob('*.pkg.tar.*') if p.suffix in ('.xz', '.zst', '.gz')]
        if not packages:
            raise ValueError('makepkg produced no binary packages')
        output.mkdir(parents=True, exist_ok=True)
        rows = {}
        for path in packages:
            shutil.copy2(path, output / path.name)
            rows[path.name] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
        guest(['pacman', '-U', '--noconfirm', *[folder + '/' + p.name for p in packages]])
        guest(['userdel', '-r', 'sunuefi-package'])
    (output / 'manifest.json').write_text(json.dumps({
        'package_format': 'pacman', 'architecture': 'aarch64',
        'recipe_sha256': hashlib.sha256((recipe / 'PKGBUILD').read_bytes()).hexdigest(),
        'packages': rows}, indent=2) + '\n')
    shutil.rmtree(work)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('rootfs', 'recipe', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.worker:
        raise SystemExit(run(['unshare', '--mount', '--propagation', 'private',
                              sys.executable, __file__, *sys.argv[1:], '--worker']).returncode)
    build(args.rootfs, args.recipe, args.output, args.resume)
