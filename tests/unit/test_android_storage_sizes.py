"""Run the real storage planner on sparse files; never open an Android device."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import build_storage
from test_install_piano import fixture_gpt


class AndroidStorageSizeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix='piano-storage-sizes-')
        cls.directory = Path(cls.work.name)
        build_storage.build(cls.directory / 'bin', 'host')
        cls.binary = cls.directory / 'bin/piano-storage'

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def test_mksh_plan_uses_native_64bit_section_bytes(self):
        shell = os.environ.get('MKSH') or shutil.which('mksh')
        if not shell:
            self.skipTest('Android-compatible mksh is needed for this regression')
        for capacity, section, choice in ((128, 2 << 20, 32),
                                          (256, 16 << 20, 64),
                                          (512, 8 << 30, 256)):
            with self.subTest(capacity=capacity, section=section), \
                    tempfile.TemporaryDirectory(dir=self.directory) as folder:
                folder = Path(folder)
                gpt = fixture_gpt(capacity)
                disk = folder / 'disk.img'
                with disk.open('wb') as stream:
                    stream.truncate(gpt.disk_bytes)
                    for offset, data in ((4096, gpt.primary),
                                         (8192, gpt.primary_entries),
                                         (gpt.disk_bytes - 4096, gpt.backup),
                                         (gpt.backup_table_lba * 4096, gpt.backup_entries)):
                        stream.seek(offset)
                        stream.write(data)
                module = folder / 'module'
                (module / 'bin').mkdir(parents=True)
                state = folder / 'state'
                script = (ROOT / 'android/module/storage-operations.sh').read_text()
                (module / 'storage-operations.sh').write_text(script.replace(
                    'STATE=/data/adb/piano-sunuefi/storage', 'STATE=' + shlex.quote(str(state))))
                wrappers = {
                    'piano-storage': '#!/bin/sh\nexec ' + shlex.quote(str(self.binary)) +
                        ' "$@" --offline ' + shlex.quote(str(disk)) + ' 4096\n',
                    'piano-resize-f2fs': '#!/bin/sh\nprintf \'%s\\n\' ' + shlex.quote(json.dumps({
                        'section_blocks': section // 4096, 'section_bytes': section}, separators=(',', ':'))) + '\n',
                    'getprop': '#!/bin/sh\ncase "$1" in ro.product.device) echo piano;; '
                        'ro.boot.flash.locked) echo 1;; esac\n',
                    'id': '#!/bin/sh\necho 0\n',
                }
                for name, text in wrappers.items():
                    path = module / 'bin' / name
                    path.write_text(text)
                    path.chmod(0o755)
                run = subprocess.run([shell, str(module / 'storage-operations.sh'),
                                      'plan', 'create', str(choice)], capture_output=True, text=True,
                                     env={**os.environ, 'PATH': str(module / 'bin') + ':' + os.environ['PATH']})
                self.assertEqual(run.returncode, 0, run.stderr)
                description = json.loads(run.stdout)
                self.assertTrue(description['requires_userdata_shrink'])
                self.assertEqual(description['userdata_target_bytes'] % section, 0)
                facts = subprocess.check_output([str(self.binary), 'facts',
                    str(state / (description['plan_id'] + '.plan')), '--offline', str(disk), '4096'], text=True).split()
                self.assertEqual((len(facts), facts[8], facts[9]), (10, str(choice), '0'))
                with disk.open('rb') as stream:
                    stream.seek(4096)
                    self.assertEqual(stream.read(4096), gpt.primary)
