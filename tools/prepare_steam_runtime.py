#!/usr/bin/env python3
"""Preinstall public ARM64 Proton and Steam Linux Runtime without account data."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import tarfile

from prepare_steamos_rootfs import download

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'config/steam-runtime.json'


def prepare(root, output):
    root, output = Path(root).resolve(), Path(output).resolve()
    if os.geteuid() or not root.is_relative_to(ROOT / 'build/distros'):
        raise ValueError('Use root and a project rootfs')
    cfg = json.loads(CONFIG.read_text())
    cache = ROOT / 'build/source-cache/steam-runtime'
    cache.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    steam = root / 'home/piano/.local/share/Steam'
    for name, parent in (('proton', steam / 'compatibilitytools.d'),
                         ('runtime', steam / 'steamapps/common')):
        item = cfg[name]
        archive = cache / item['filename']
        download(item['url'], archive, item['sha256'])
        if archive.stat().st_size != item['bytes']:
            raise ValueError('Runtime archive size mismatch: ' + name)
        parent.mkdir(parents=True, exist_ok=True)
        destination = parent / item['directory']
        # Replacing these public tool directories preserves games and account
        # state. No live Steam installation is used as a build input.
        if destination.exists():
            shutil.rmtree(destination)
        with tarfile.open(archive) as stream:
            members = stream.getmembers()
            if any(Path(m.name).parts[0] != item['directory'] for m in members):
                raise ValueError('Unexpected runtime archive root: ' + name)
            stream.extractall(parent, members=members, filter='data')
    proton = steam / 'compatibilitytools.d' / cfg['proton']['directory']
    runtime = steam / 'steamapps/common' / cfg['runtime']['directory']
    for required in (proton / 'proton', proton / 'compatibilitytool.vdf',
                     proton / 'files/bin-arm64/wine',
                     runtime / '_v2-entry-point', runtime / 'toolmanifest.vdf'):
        if not required.is_file():
            raise ValueError('Incomplete runtime payload: ' + str(required))
    # Keep Proton's require_tool_appid: the matching runtime is fully installed
    # and registered, so Steam can select its ordinary container launch path.
    item = cfg['runtime']
    (steam / 'steamapps' / ('appmanifest_' + item['appid'] + '.acf')).write_text(
        '"AppState"\n{\n'
        f'\t"appid" "{item["appid"]}"\n'
        '\t"Universe" "1"\n\t"name" "Steam Linux Runtime 4.0 - Arm64"\n'
        '\t"StateFlags" "4"\n'
        f'\t"installdir" "{item["directory"]}"\n'
        f'\t"buildid" "{item["buildid"]}"\n'
        f'\t"SizeOnDisk" "{item["installed_bytes"]}"\n'
        '\t"InstalledDepots"\n\t{\n'
        f'\t\t"{item["depot_id"]}"\n\t\t{{\n'
        f'\t\t\t"manifest" "{item["manifest"]}"\n'
        f'\t\t\t"size" "{item["installed_bytes"]}"\n'
        '\t\t}\n\t}\n}\n')
    # A default has priority 75. Priority 250 is an explicit per-application
    # override and would force even native Linux runtime tools through Proton,
    # causing Steam to deselect and remove their Linux depots on first launch.
    config = steam / 'config/config.vdf'
    config.parent.mkdir(parents=True, exist_ok=True)
    default = (
        '"InstallConfigStore"\n{\n\t"Software"\n\t{\n\t\t"Valve"\n\t\t{\n'
        '\t\t\t"Steam"\n\t\t\t{\n\t\t\t\t"CompatToolMapping"\n\t\t\t\t{\n'
        '\t\t\t\t\t"0"\n\t\t\t\t\t{\n'
        f'\t\t\t\t\t\t"name" "{cfg["proton"]["directory"]}"\n'
        '\t\t\t\t\t\t"config" ""\n\t\t\t\t\t\t"priority" "75"\n'
        '\t\t\t\t\t}\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n')
    if config.exists() and config.read_text() != default:
        raise ValueError('Set the runtime default before any Steam account configuration')
    config.write_text(default)
    record = {**cfg, 'config_sha256': hashlib.sha256(CONFIG.read_bytes()).hexdigest(),
              'account_data_used': False, 'fully_extracted': True}
    (output / 'SOURCE.json').write_text(json.dumps(record, indent=2) + '\n')
    return record
