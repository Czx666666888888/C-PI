"""Run the Pi's original material detector and display its output on the PC.

No serial ports are opened. Quit with Q, Escape, or the window close button.
"""

import argparse
import queue
import struct
import subprocess
import threading
import time

import cv2
import numpy as np


WORKER = r'''
import sys, time, struct
import cv2
sys.path.insert(0, '/home/pi/opencv_test')
from physical_detect import get_color_blocks_positions
cv2.setNumThreads(2)
cap = cv2.VideoCapture('/dev/v4l/by-id/usb-Alcorlink_Corp._USB_2.0_Camera-video-index0', cv2.CAP_V4L2)
try:
    if not cap.isOpened():
        raise RuntimeError('Camera did not open')
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, 30)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    fourcc = int(cap.get(cv2.CAP_PROP_FOURCC))
    print('PI camera format=' + ''.join(chr((fourcc >> (8*i)) & 255) for i in range(4)),
          file=sys.stderr, flush=True)
    count = failures = 0
    start = report = time.monotonic()
    while True:
        ok, frame = cap.read()
        if not ok:
            failures += 1
            raise RuntimeError('Camera frame read failed')
        positions = get_color_blocks_positions(frame)
        count += 1
        now = time.monotonic()
        fps = count / (now - start)
        cv2.putText(frame, f'Pi material detector | {fps:.1f} FPS | frame {count}',
                    (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1)
        cv2.putText(frame, 'Pixel offsets only | Q / Esc to close on PC',
                    (8, 45), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        ok, jpg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
        if not ok:
            raise RuntimeError('JPEG encoding failed')
        data = jpg.tobytes()
        sys.stdout.buffer.write(struct.pack('!I', len(data)))
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
        if now - report >= 5:
            print(f'PI frames={count} fps={fps:.1f} read_failures={failures} '
                  f'size={frame.shape[1]}x{frame.shape[0]} positions={positions}',
                  file=sys.stderr, flush=True)
            report = now
except (BrokenPipeError, KeyboardInterrupt):
    pass
finally:
    cap.release()
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='pi@192.168.123.50')
    parser.add_argument('--key', help='Optional private key; otherwise use OpenSSH config/agent')
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--frames', type=int, default=0, help='0: until closed')
    parser.add_argument('--duration', type=float, default=0,
                        help='Seconds from first displayed frame; 0: until closed')
    parser.add_argument('--snapshot', help='Save the last displayed frame')
    args = parser.parse_args()
    if args.frames < 0 or args.duration < 0:
        parser.error('frames and duration must be nonnegative')
    ssh = ['ssh', '-T']
    if args.key:
        ssh += ['-i', args.key]
    process = subprocess.Popen(
        ssh + ['-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
         '-o', 'ConnectTimeout=8', '-o', 'ServerAliveInterval=3',
         '-o', 'ServerAliveCountMax=2', args.host,
         '/home/pi/opencv_test/.venv/bin/python -u -'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0,
    )
    frames = queue.Queue(maxsize=1)
    errors = queue.Queue()
    stop = threading.Event()
    counters = {'decoded': 0, 'skipped': 0, 'decode_failures': 0}

    def read_exact(length):
        result = bytearray()
        while len(result) < length:
            part = process.stdout.read(length - len(result))
            if not part:
                raise EOFError('SSH video stream ended')
            result.extend(part)
        return bytes(result)

    def receive():
        try:
            while not stop.is_set():
                length, = struct.unpack('!I', read_exact(4))
                if not 0 < length <= 4_000_000:
                    raise ValueError(f'Invalid JPEG length: {length}')
                frame = cv2.imdecode(np.frombuffer(read_exact(length), np.uint8), cv2.IMREAD_COLOR)
                if frame is None:
                    counters['decode_failures'] += 1
                    raise ValueError('JPEG decode failed')
                counters['decoded'] += 1
                if frames.full():
                    try:
                        frames.get_nowait()
                        counters['skipped'] += 1
                    except queue.Empty:
                        pass
                frames.put_nowait(frame)
        except Exception as exc:
            errors.put(exc)

    thread = threading.Thread(target=receive, daemon=True)
    shown = 0
    last = None
    start = last_frame = report = time.monotonic()
    first_frame = None
    title = 'C-PI - Raspberry Pi material detection (Q to close)'
    try:
        process.stdin.write(WORKER.encode())
        process.stdin.close()
        thread.start()
        if not args.headless:
            cv2.namedWindow(title, cv2.WINDOW_NORMAL)
            cv2.resizeWindow(title, 960, 720)
        while True:
            if not errors.empty():
                raise RuntimeError(str(errors.get()))
            try:
                last = frames.get(timeout=0.05)
                shown += 1
                last_frame = time.monotonic()
                if first_frame is None:
                    first_frame = last_frame
                if not args.headless:
                    cv2.imshow(title, last)
            except queue.Empty:
                pass
            now = time.monotonic()
            if now - last_frame > 12:
                raise TimeoutError('No new video frame for 12 seconds')
            if not args.headless:
                if cv2.waitKey(1) & 0xFF in (ord('q'), 27):
                    break
                if shown and cv2.getWindowProperty(title, cv2.WND_PROP_VISIBLE) == 0:
                    break
            if args.frames and shown >= args.frames:
                break
            if args.duration and first_frame is not None and now - first_frame >= args.duration:
                break
            if now - report >= 5:
                print(f'PC displayed={shown} decoded={counters["decoded"]} '
                      f'preview_skipped={counters["skipped"]} fps={shown / (now-start):.1f}', flush=True)
                report = now
    finally:
        stop.set()
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        if thread.is_alive():
            thread.join(timeout=5)
        process.stdout.close()
        if not args.headless:
            cv2.destroyAllWindows()
        if args.snapshot and last is not None:
            if not cv2.imwrite(args.snapshot, last):
                raise RuntimeError('Could not save snapshot')
        elapsed = time.monotonic() - start
        stream_seconds = last_frame - first_frame if first_frame is not None else 0
        fps = (shown - 1) / stream_seconds if stream_seconds > 0 else 0
        print(f'STOP displayed={shown} decoded={counters["decoded"]} '
              f'preview_skipped={counters["skipped"]} decode_failures={counters["decode_failures"]} '
              f'elapsed={elapsed:.1f}s stream_seconds={stream_seconds:.2f} fps={fps:.2f} '
              f'ssh_reaped={process.poll() is not None} receiver_alive={thread.is_alive()}', flush=True)


if __name__ == '__main__':
    main()
