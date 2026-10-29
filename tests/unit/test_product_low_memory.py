import platform
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'upstream/Mu-Silicium/Mu_Basecore'
HOST_PROCESSOR_BIND='AArch64' if platform.machine() in ('aarch64','arm64') else 'X64'
sys.path.insert(0,str(ROOT/'tools'))
from prepare_product import fix_product_low_heap
from plan_piano_dram import extract_dt,parse_native_c
import piano_vendor_inputs as vendor


def function(text,name):
    m=re.search(r'STATIC\s+EFI_STATUS\s+(?:EFIAPI\s+)?'+name+r'\s*\(',text)
    if not m:raise ValueError(name)
    begin=text.index('{',m.start());depth=1;end=begin+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]


class ProductLowMemoryTests(unittest.TestCase):
    def test_semantic_dt_and_native_cache_preservation(self):
        original=(ROOT/'uefi/platforms/pianoProbePkg/Library/MemoryMapLib/MemoryMapLib.c').read_text()
        # Same board DTB that prepare_product passes to fix_product_low_heap
        dt=vendor.load(ROOT)['board_dtb']
        result,record=fix_product_low_heap(original,dt)
        new={r['name']:r for r in parse_native_c(result)}
        for row in parse_native_c(original):
            if row['name']in ('DXE_Heap','DBI_Dump'):continue
            self.assertEqual(row,new[row['name']])
        self.assertEqual(new['DXE_Heap']['base'],0xBD980000)
        self.assertEqual(new['DXE_Heap']['end'],0xD4E23000)
        self.assertFalse(record['high_ddr_added'])
        with self.assertRaises(ValueError):fix_product_low_heap(result,dt)
        newer=ROOT/'private/analysis/android-memory-2026-10-05/live.dtb'
        if newer.exists():self.assertEqual(result,fix_product_low_heap(original,newer.read_bytes())[0])

    def test_actual_sec_hob_mmu_and_dma_sources(self):
        original=(ROOT/'uefi/platforms/pianoProbePkg/Library/MemoryMapLib/MemoryMapLib.c').read_text()
        source,_=fix_product_low_heap(original,vendor.load(ROOT)['board_dtb'])
        (ROOT/'build/product-low-memory-test.c').write_text(source)
        sec=function((ROOT/'upstream/Mu-Silicium/Silicon/Silicium/SiliciumPkg/Sec/Sec.c').read_text(),'InitializeMemory').replace('InitializeMemory','PianoActualInitializeMemory')
        dma=function((ROOT/'uefi/core/PianoDma.c').read_text(),'Heap')
        with tempfile.TemporaryDirectory(prefix='piano-product-low-')as tmp:
            tmp=Path(tmp);(tmp/'PianoActualSecMemory.h').write_text(sec);(tmp/'PianoActualDmaHeap.h').write_text(dma)
            exe=tmp/'low'
            includes=[BASE/'MdePkg/Include',BASE/'MdePkg/Include'/HOST_PROCESSOR_BIND,BASE/'MdeModulePkg/Include',BASE/'UefiCpuPkg/Include',BASE/'EmbeddedPkg/Include',ROOT/'upstream/Mu-Silicium/Silicon/Silicium/SiliciumPkg/Include',tmp]
            command=['cc','-std=gnu11','-fshort-wchar','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-include',str(ROOT/'tests/native/PianoCmaPcdShim.h'),'-D_PCD_VALUE_PcdCPUCoresStackBase=0xA760D000ULL','-D_PCD_VALUE_PcdCPUCorePrimaryStackSize=0x40000U']
            for include in includes:command+=['-I',str(include)]
            command+=[str(ROOT/'tests/native/PianoProductLowMemoryTest.c'),str(BASE/'EmbeddedPkg/Library/PrePiHobLib/Hob.c'),'-Wl,--gc-sections','-o',str(exe)]
            build=subprocess.run(command,capture_output=True,text=True);self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'});self.assertEqual(run.returncode,0,run.stdout+run.stderr);print(run.stdout.strip())


if __name__=='__main__':unittest.main()
