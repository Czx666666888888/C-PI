#!/usr/bin/env python3
"""Headless vision + latest-observation UART service; never commands motion."""
import argparse
import json
import logging
import signal
import struct
import threading
import time
from pathlib import Path

from communication.protocol import Parser, Type, encode

LOG = logging.getLogger('cpi')


class Observation:
    def __init__(self):
        self.lock = threading.Lock()
        self.value = (0, 0, 0, 0)
        self.stamp = 0.0

    def set(self, value, stamp):
        with self.lock:
            self.value, self.stamp = value, stamp

    def payload(self, now, max_age):
        with self.lock:
            value, stamp = self.value, self.stamp
        age = min(65535, max(0, int((now - stamp) * 1000)))
        if not value[1] or age > max_age:
            value = (0, 0, 0, 0)
        return struct.pack('<BBhhH', *value, age)


def camera_worker(config, observation, stop, preview=False):
    import cv2
    from vision.detector import detect
    cv2.setNumThreads(2)
    cap = None
    report = 0
    count = 0
    try:
        while not stop.is_set():
            try:
                if cap is None:
                    cap = cv2.VideoCapture(config['camera'], cv2.CAP_V4L2)
                    if not cap.isOpened():
                        raise RuntimeError('open failed')
                    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
                    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
                    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
                    cap.set(cv2.CAP_PROP_FPS, 30)
                    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                    LOG.info('camera opened %s', config['camera'])
                ok, frame = cap.read()
                stamp = time.monotonic()  # userspace read completion, not sensor exposure
                if not ok:
                    raise RuntimeError('read failed')
                if frame.shape[:2] != (480, 640):
                    raise RuntimeError('camera must deliver 640x480 for existing reference')
                value, positions, annotated = detect(frame, config['target_color'])
                observation.set(value, stamp)
                count += 1
                if stamp - report >= config['log_seconds']:
                    LOG.info('vision frames=%d positions=%s selected=%s', count, positions, value)
                    report = stamp
                if preview:
                    cv2.imshow('C-PI', annotated)
                    if cv2.waitKey(1) & 0xFF in (27, ord('q')):
                        stop.set()
            except Exception as exc:
                observation.set((0, 0, 0, 0), time.monotonic())
                LOG.warning('camera unavailable: %s; retrying', exc)
                if cap is not None:
                    cap.release()
                    cap = None
                stop.wait(config['retry_seconds'])
    finally:
        if cap is not None:
            cap.release()
        if preview:
            cv2.destroyAllWindows()
        LOG.info('camera released')


def run(config, mode, stop, duration=0, preview=False):
    observation = Observation()
    start = time.monotonic()
    worker = None
    if mode != 'simulate':
        worker = threading.Thread(target=camera_worker, args=(config, observation, stop, preview), daemon=True)
        worker.start()
    port = None
    parser = Parser()
    seq = 0
    retry = target_due = heartbeat_due = report = start
    tx_target = tx_heartbeat = rx = 0
    try:
        while not stop.is_set():
            now = time.monotonic()
            if duration and now - start >= duration:
                break
            if mode == 'simulate':
                observation.set((1, 1, 30, -15), now)
            if mode != 'vision' and port is None and now >= retry:
                retry = now + config['retry_seconds']
                try:
                    import serial
                    port = serial.Serial(config['serial_device'], config['baud'], timeout=0,
                                         write_timeout=0.05, exclusive=True)
                    parser = Parser()
                    LOG.info('serial connected %s 115200/8N1', config['serial_device'])
                except (OSError, ImportError) as exc:
                    LOG.warning('serial unavailable: %s', exc)
            if port is not None:
                try:
                    for kind, received_seq, payload in parser.feed(port.read(1024)):
                        rx += 1
                        if kind == Type.TARGET_ACK:
                            LOG.info('TARGET_ACK frame_seq=%d target_seq=%d result=%d', received_seq,
                                     *struct.unpack('<HB', payload))
                        elif kind == Type.STATUS:
                            LOG.info('STATUS frame_seq=%d state=%d flags=%d error=%d', received_seq,
                                     *struct.unpack('<BHH', payload))
                    for kind, due, interval in ((Type.TARGET, target_due, .1), (Type.HEARTBEAT, heartbeat_due, .5)):
                        if now < due:
                            continue
                        if kind == Type.TARGET:
                            payload = observation.payload(time.monotonic(), config['max_age_ms'])
                            target_due = now + interval
                        else:
                            payload = struct.pack('<I', int((now - start) * 1000) & 0xFFFFFFFF)
                            heartbeat_due = now + interval
                        frame = encode(kind, seq, payload)
                        if port.out_waiting > 64:
                            raise OSError('serial output backlog; dropping stale output')
                        if port.write(frame) != len(frame):
                            raise OSError('short write')
                        if kind == Type.TARGET:
                            tx_target += 1
                        else:
                            tx_heartbeat += 1
                        seq = (seq + 1) & 0xFFFF
                except OSError as exc:
                    LOG.warning('serial lost: %s', exc)
                    port.close()
                    port = None
                    retry = now + config['retry_seconds']
            if now >= report:
                LOG.info('state serial=%s TARGET=%d HEARTBEAT=%d RX=%d crc_or_header_errors=%d target=%s',
                         'connected' if port else 'offline', tx_target, tx_heartbeat, rx, parser.errors,
                         struct.unpack('<BBhhH', observation.payload(now, config['max_age_ms'])))
                report = now + config['log_seconds']
            stop.wait(.005)
    finally:
        stop.set()
        if port is not None:
            port.close()
        if worker:
            worker.join(timeout=4)
            if worker.is_alive():
                LOG.error('camera read did not return; process exit releases device')
        LOG.info('stopped; serial released')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', default=str(Path(__file__).parent / 'config/default.json'))
    parser.add_argument('--mode', choices=['simulate', 'vision', 'combined'], default='combined')
    parser.add_argument('--serial-device')
    parser.add_argument('--camera')
    parser.add_argument('--target-color', choices=['Red', 'Yellow', 'Blue', 'Green', 'Black', 'Light Blue'])
    parser.add_argument('--preview', action='store_true', help='Optional local desktop window')
    parser.add_argument('--duration', type=float, default=0)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    for key in ('serial_device', 'camera', 'target_color'):
        if getattr(args, key) is not None:
            config[key] = getattr(args, key)
    if config['baud'] != 115200 or config['retry_seconds'] < .1 or config['max_age_ms'] <= 0 or config['log_seconds'] <= 0:
        parser.error('Invalid UART/timing configuration')
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
    stop = threading.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stop.set())
    run(config, args.mode, stop, args.duration, args.preview)


if __name__ == '__main__':
    main()
