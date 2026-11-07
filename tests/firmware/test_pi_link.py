"""Host-only tests of actual firmware C; production build remains Keil ARMCC."""
import ctypes as C
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('protocol',ROOT/'PI/communication/protocol.py')
protocol=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol)
class Target(C.Structure):
    _fields_=[('color',C.c_uint8),('valid',C.c_uint8),('dx',C.c_int16),('dy',C.c_int16),('age',C.c_uint16),('seq',C.c_uint16),('tick',C.c_uint32)]
class FirmwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('gcc'): raise unittest.SkipTest('GCC needed for host-only tests')
        tmp=ROOT/'.local/firmware-tests';tmp.mkdir(parents=True,exist_ok=True)
        source=(ROOT/'firmware/Core/Src/freertos.c').read_text(encoding='utf-8')
        text=source[source.index('#define CHASSIS_LOOP_DT_S'):source.index('/* USER CODE END PD */')]
        for n in (4,6):
            start=source.index(f'void StartTask0{n}(void const * argument)\n{{')
            end=source.index(f'  /* USER CODE END StartTask0{n} */',start)
            text+='\n'+source[start:end]+'\n}\n'
        (tmp/'tasks.inc').write_text(text,encoding='utf-8')
        dll=tmp/'firmware.dll'
        subprocess.run(['gcc','-std=c99','-shared','-O0','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-parameter','-Wl,--export-all-symbols',
            '-I'+str(ROOT/'tests/firmware/stubs'),'-I'+str(ROOT/'firmware/Core/Inc'),'-I'+str(ROOT/'firmware/Core/Src'),'-I'+str(tmp),str(ROOT/'tests/firmware/harness.c'),'-o',str(dll)],check=True)
        cls.c=C.CDLL(str(dll));cls.c.test_tick.argtypes=[C.c_uint32];cls.c.pi_link_get_target.argtypes=[C.POINTER(Target)];cls.c.test_crc.restype=C.c_uint16
    def setUp(self): self.c.test_reset()
    def feed(self,data,process=True):
        for b in data:self.c.pi_link_rx_byte_isr(b)
        if process:self.c.pi_link_process()
    def target(self):
        t=Target();self.c.pi_link_get_target(C.byref(t));return t
    def tx(self):
        b=(C.c_uint8*8192)();n=self.c.test_output(b);return protocol.Parser().feed(bytes(b[:n]))
    def send(self,color=1,valid=1,age=0,seq=123,dx=30,dy=-15):self.feed(protocol.encode(2,seq,struct.pack('<BBhhH',color,valid,dx,dy,age)))
    def test_crc_ack_status_and_signed_coordinates(self):
        self.assertEqual(self.c.test_crc(b'123456789',9),0x29B1)
        self.send(dx=-32768,dy=32767,seq=65535);t=self.target()
        self.assertEqual((t.color,t.valid,t.dx,t.dy,t.seq),(1,1,-32768,32767,65535))
        self.assertEqual(self.tx()[0][2],struct.pack('<HB',65535,0))
        self.c.test_tick(100);self.feed(protocol.encode(1,0,b'\0'*4));self.c.test_tick(500);self.c.pi_link_process()
        self.assertEqual(self.tx()[-1][2],struct.pack('<BHH',0,3,0))
    def test_fragment_corruption_resync_and_semantics(self):
        f=protocol.encode(2,10,struct.pack('<BBhhH',1,1,30,-15,0))
        self.feed(f[:9]);self.assertEqual(self.target().valid,0);self.feed(f[9:]);self.assertEqual(self.target().valid,1);self.tx()
        bad=bytearray(f);bad[-1]^=1;self.feed(b'noise\xa5'+bytes(bad)+f);self.assertEqual(len(self.tx()),1)
        self.send(valid=2);self.assertEqual(self.tx(),[]);self.send(color=7);self.assertEqual(self.tx(),[])
        self.send(color=0,valid=0);self.assertEqual(self.target().valid,0)
    def test_age_heartbeat_rollover_and_rx_fault(self):
        self.c.test_tick(0xffffff00);self.send(age=400);self.c.test_tick(0xffffff64);self.assertEqual(self.target().valid,1)
        self.c.test_tick(0xffffff65);self.assertEqual(self.target().valid,0)
        self.c.test_tick(0xfffffff0);self.send();self.c.test_tick(0x10);self.assertEqual(self.target().valid,1)
        self.c.pi_link_rx_error_isr();self.c.pi_link_process();self.assertEqual(self.target().valid,0)
        self.c.test_tick(0);self.feed(protocol.encode(1,0,b'\0'*4));self.assertEqual(self.c.pi_link_heartbeat_online(),1)
        self.c.test_tick(1500);self.assertEqual(self.c.pi_link_heartbeat_online(),0);self.send(age=501);self.assertEqual(self.target().valid,0)
    def test_overflow_partial_timeout_and_tx_failure(self):
        self.send();self.feed(bytes(300),False);self.c.pi_link_process();self.assertEqual(self.target().valid,0)
        f=protocol.encode(2,1,struct.pack('<BBhhH',1,1,30,-15,0));self.feed(f[:8]);self.c.test_tick(101);self.feed(f)
        self.assertEqual(self.target().valid,1);self.c.test_tx_fail();self.send();self.assertGreater(C.c_uint32.in_dll(self.c,'g_pi_tx_errors').value,0)
    def test_actual_mode_task(self):self.assertEqual(self.c.test_modes(),0)
    def test_actual_tower_task(self):self.assertEqual(self.c.test_grip(),0)
if __name__=='__main__':unittest.main()
