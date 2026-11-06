#!/usr/bin/env python3
"""Real PTY integration, reconnect and camera-independent operation test."""
import errno
import json
import os
from pathlib import Path
import pty
import select
import struct
import subprocess
import sys
import tempfile
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from communication.protocol import Parser, Type, encode


def main():
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='cpi-pty-') as tmp:
        device = Path(tmp) / 'uart'
        master, slave = pty.openpty()
        device.symlink_to(os.ttyname(slave))
        config = json.loads((root / 'config/default.json').read_text())
        config.update(serial_device=str(device), retry_seconds=.2, log_seconds=2)
        cfg = Path(tmp) / 'config.json'
        cfg.write_text(json.dumps(config))
        log = Path(tmp) / 'run.log'
        with log.open('w') as output:
            process = subprocess.Popen([sys.executable, str(root/'main.py'), '--mode', 'simulate', '--config', str(cfg), '--duration', '9'], stdout=output, stderr=output)
            parser = Parser()
            counts = {1: 0, 2: 0}
            stamps = {1: [], 2: []}
            start = time.monotonic()
            disconnected = reconnected = False
            try:
                while process.poll() is None:
                    elapsed = time.monotonic() - start
                    if elapsed >= 4 and not disconnected:
                        os.close(master)
                        os.close(slave)
                        disconnected = True
                        device.unlink()
                    if elapsed >= 5 and not reconnected:
                        master, slave = pty.openpty()
                        device.symlink_to(os.ttyname(slave))
                        parser = Parser()
                        reconnected = True
                    if disconnected and not reconnected:
                        time.sleep(.02)
                        continue
                    if not select.select([master], [], [], .05)[0]:
                        continue
                    try:
                        data = os.read(master, 4096)
                    except OSError as exc:
                        if exc.errno == errno.EIO:
                            continue
                        raise
                    for kind, seq, payload in parser.feed(data):
                        if kind not in counts:
                            continue
                        counts[kind] += 1
                        if elapsed < 4:
                            stamps[kind].append(time.monotonic())
                        if kind == Type.TARGET:
                            assert struct.unpack('<BBhhH', payload)[:4] == (1, 1, 30, -15)
                            os.write(master, encode(Type.TARGET_ACK, seq, struct.pack('<HB', seq, 0)))
                            os.write(master, encode(Type.STATUS, seq, struct.pack('<BHH', 1, 2, 0)))
                assert process.returncode == 0
                text = log.read_text()
                assert 'serial lost' in text and text.count('serial connected') >= 2, text
                assert 'TARGET_ACK' in text and 'STATUS' in text, text
                for kind, expected in ((1, 2), (2, 10)):
                    times = stamps[kind]
                    hz = (len(times)-1)/(times[-1]-times[0])
                    assert abs(hz - expected) < expected * .1, hz
                    print(f'{Type(kind).name}: {hz:.3f} Hz; total={counts[kind]} PASS')
                print('signed offsets / ACK / STATUS / disconnect / reconnect PASS')
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)
                os.close(master)
                os.close(slave)


if __name__ == '__main__':
    main()
