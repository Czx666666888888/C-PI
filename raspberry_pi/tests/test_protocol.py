import struct
import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from communication.protocol import Parser, Type, crc16, encode
from main import Observation


class ProtocolTests(unittest.TestCase):
    def test_known_crc(self):
        self.assertEqual(crc16(b'123456789'), 0x29B1)

    def test_stream(self):
        payload = struct.pack('<BBhhH', 1, 1, -32768, 32767, 65535)
        frame = encode(Type.TARGET, 65535, payload)
        parser = Parser()
        self.assertEqual(parser.feed(b'noise' + frame[:5]), [])
        self.assertEqual(parser.feed(frame[5:] + frame), [(2, 65535, payload)] * 2)
        bad = bytearray(frame)
        bad[8] ^= 1
        self.assertEqual(parser.feed(bad + frame), [(2, 65535, payload)])
        self.assertGreater(parser.errors, 0)

    def test_header_recovery(self):
        frame = encode(1, 0, bytes(4))
        p = Parser()
        self.assertEqual(p.feed(b'\xa5\x5a\x02\x02\x00\x00\xff' + frame), [(1, 0, bytes(4))])
        with self.assertRaises(ValueError):
            encode(2, 0, bytes(7))

    def test_stale_and_missing(self):
        obs = Observation()
        obs.set((1, 1, 30, -15), 10)
        self.assertEqual(struct.unpack('<BBhhH', obs.payload(10.1, 500))[:4], (1, 1, 30, -15))
        self.assertEqual(struct.unpack('<BBhhH', obs.payload(11, 500))[:4], (0, 0, 0, 0))
        obs.set((0, 0, 0, 0), 11)
        self.assertEqual(struct.unpack('<BBhhH', obs.payload(11.1, 500))[:4], (0, 0, 0, 0))


if __name__ == '__main__':
    unittest.main()
