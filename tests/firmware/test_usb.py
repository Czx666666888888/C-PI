"""USB adapter tests; physical enumeration/PCD interrupts still need real hardware."""
import ctypes
from pathlib import Path
import re
import shutil
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[2]
class UsbTransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('gcc'):raise unittest.SkipTest('Host-test GCC unavailable')
        tmp=ROOT/'.local/usb-tests';tmp.mkdir(parents=True,exist_ok=True)
        source=(ROOT/'firmware/USB_DEVICE/App/usb_cpi.c').read_text(encoding='utf-8')
        # Only includes are removed; production function bodies remain unchanged.
        (tmp/'usb_body.inc').write_text(re.sub(r'^#include[^\n]*\n','',source,flags=re.M),encoding='utf-8')
        dll=tmp/'usb.dll'
        subprocess.run(['gcc','-std=c99','-shared','-Wall','-Wextra','-Wl,--export-all-symbols','-I'+str(tmp),str(ROOT/'tests/firmware/usb_harness.c'),'-o',str(dll)],check=True)
        cls.lib=ctypes.CDLL(str(dll))
    def test_async_tx_buffer_and_timeout(self):self.assertEqual(self.lib.test_usb_tx_lifetime(),0)
    def test_cdc_rx_and_linecoding(self):self.assertEqual(self.lib.test_usb_rx_and_control(),0)
    def test_static_class_allocation(self):self.assertEqual(self.lib.test_usb_static_allocation(),0)
