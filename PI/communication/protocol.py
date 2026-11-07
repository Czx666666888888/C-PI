"""C-PI V1 framing. No hardware imports."""
import struct
from enum import IntEnum


class Type(IntEnum):
    HEARTBEAT = 1
    TARGET = 2
    TARGET_ACK = 3
    STATUS = 4
    DETECT_REQUEST = 5
    QR_RESULT = 6


LENGTHS = {1: 4, 2: 8, 3: 3, 4: 5}


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def encode(kind, seq, payload):
    if len(payload) > 255 or (kind in LENGTHS and len(payload) != LENGTHS[kind]):
        raise ValueError('Invalid payload length')
    data = struct.pack('<2sBBHB', b'\xa5\x5a', 1, kind, seq, len(payload)) + payload
    return data + struct.pack('<H', crc16(data))


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.errors = 0

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while len(self.buffer) >= 2:
            if self.buffer[:2] != b'\xa5\x5a':
                del self.buffer[0]
                continue
            if len(self.buffer) < 7:
                break
            _, version, kind, seq, length = struct.unpack('<2sBBHB', self.buffer[:7])
            if version != 1 or kind not in range(1, 7) or (kind in LENGTHS and length != LENGTHS[kind]):
                self.errors += 1
                del self.buffer[0]
                continue
            size = 9 + length
            if len(self.buffer) < size:
                break
            frame = self.buffer[:size]
            if crc16(frame[:-2]) != struct.unpack('<H', frame[-2:])[0]:
                self.errors += 1
                del self.buffer[0]
                continue
            result.append((kind, seq, bytes(frame[7:-2])))
            del self.buffer[:size]
        return result
