"""Independent frustum experiment. No serial/control imports or production changes."""
import argparse
import csv
import json
import queue
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

import cv2
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from legacy.physical_detect import colors_to_track
from vision.detector import GRIPPER_CX, GRIPPER_CY

CONFIG = Path(__file__).with_suffix('.json')
REFERENCE = (GRIPPER_CX, GRIPPER_CY)
METHODS = ('A_gradient', 'B_contour')
INK = {'BoundingRect': (0, 255, 255), 'Moments': (255, 255, 0),
       'A_gradient': (255, 0, 255), 'B_contour': (0, 255, 0), 'TopEllipse': (0, 140, 255)}


def agreed_top(a, b, cfg):
    # 单方法可能拟合侧面反光。最终输出要求两方法一致，仍非三维真值保证。
    if a is None or b is None:
        return None
    if np.linalg.norm(np.subtract(a['center'], b['center'])) > cfg['agreement_px']:
        return None
    return a if a['residual'] <= b['residual'] else b


def segmentation(frame, color):
    # 复用生产HSV范围，保持红通道0.9、5x5高斯和开运算；不在分割前画标记。
    adjusted = frame.copy()
    adjusted[:, :, 2] = cv2.multiply(adjusted[:, :, 2], .9)
    hsv = cv2.cvtColor(cv2.GaussianBlur(adjusted, (5, 5), 0), cv2.COLOR_BGR2HSV)
    p = colors_to_track[color]
    if color == 'Red':
        mask = cv2.inRange(hsv, p['lower1'], p['upper1']) | cv2.inRange(hsv, p['lower2'], p['upper2'])
    else:
        mask = cv2.inRange(hsv, p['lower'], p['upper'])
    return hsv, cv2.morphologyEx(mask, cv2.MORPH_OPEN, np.ones((5, 5), np.uint8))


def distances(points, ellipse):
    """First-order geometric distance in pixels, not an exact closest-point distance."""
    (cx, cy), (w, h), angle = ellipse
    t = np.deg2rad(angle)
    d = np.asarray(points).reshape(-1, 2) - (cx, cy)
    u = (d[:, 0] * np.cos(t) + d[:, 1] * np.sin(t)) / (w / 2)
    v = (-d[:, 0] * np.sin(t) + d[:, 1] * np.cos(t)) / (h / 2)
    r = np.sqrt(u*u + v*v)
    scale = np.sqrt((u / (w / 2))**2 + (v / (h / 2))**2)
    return np.abs(r*r - 1) / np.maximum(2 * scale, 1e-6), np.arctan2(v, u)


def candidate(points, ellipse, roi, box, cfg):
    (cx, cy), (w, h), _ = ellipse
    x, y, bw, bh = box
    if not np.isfinite(np.array(ellipse[:2])).all() or min(w, h) <= 0:
        return None
    if not (0 <= int(cx) < roi.shape[1] and 0 <= int(cy) < roi.shape[0]
            and roi[int(cy), int(cx)] and y <= cy <= y + bh * cfg['center_y_max']):
        return None
    area = np.pi * w * h / 4 / max(cv2.countNonZero(roi), 1)
    if not (min(w, h) / max(w, h) >= cfg['axis_ratio_min']
            and cfg['major_width_min'] <= max(w, h) / bw <= cfg['major_width_max']
            and cfg['area_fraction_min'] <= area <= cfg['area_fraction_max']):
        return None
    residual, angles = distances(points, ellipse)
    keep = residual <= cfg['inlier_px']
    if keep.sum() < cfg['min_points']:
        return None
    bins = np.floor((angles[keep] + np.pi) / (2*np.pi) * cfg['angle_bins']).astype(int)
    coverage = len(np.unique(bins % cfg['angle_bins'])) / cfg['angle_bins']
    rms = float(np.sqrt(np.mean(residual[keep]**2)))
    if coverage < cfg['min_coverage'] or rms > cfg['max_residual_px']:
        return None
    return {'ellipse': ellipse, 'center': (cx, cy), 'residual': rms,
            'coverage': coverage, 'inliers': int(keep.sum()),
            'score': coverage / (1 + rms), 'points': np.asarray(points).reshape(-1, 2)[keep]}


def fit_edges(edge, roi, box, cfg, robust):
    contours, _ = cv2.findContours(edge, cv2.RETR_LIST, cv2.CHAIN_APPROX_NONE)
    ys, xs = np.nonzero(edge)
    all_points = np.column_stack((xs, ys)).astype(np.float32)
    rng = np.random.default_rng(cfg['seed'])
    if len(all_points) > cfg['max_edge_points']:
        all_points = all_points[rng.choice(len(all_points), cfg['max_edge_points'], replace=False)]
    proposals = [c.reshape(-1, 2).astype(np.float32) for c in contours if len(c) >= cfg['min_points']]
    if robust and len(all_points) >= cfg['min_points']:
        # 内部亮度边缘点采样，不对整个颜色外轮廓fitEllipse。
        proposals += [all_points[rng.choice(len(all_points), 5, replace=False)]
                      for _ in range(cfg['ransac_iterations'])]
    best = None
    for points in proposals:
        try:
            ellipse = cv2.fitEllipse(points)
            support = all_points if robust else points
            fit = candidate(support, ellipse, roi, box, cfg)
            if fit:
                refined = cv2.fitEllipse(fit['points'].astype(np.float32))
                fit = candidate(support, refined, roi, box, cfg)
            if fit and (best is None or fit['score'] > best['score']):
                best = fit
        except cv2.error:
            continue
    return best


def analyze(frame, cfg):
    if frame.shape[:2] != (480, 640):
        raise ValueError('Require 640x480; reference (290,260) must not be rescaled silently')
    hsv, mask = segmentation(frame, cfg['color'])
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    result = {'BoundingRect': None, 'Moments': None, **{m: None for m in METHODS}, 'TopEllipse': None}
    roi = np.zeros_like(mask)
    edges = [np.zeros_like(mask), np.zeros_like(mask)]
    contour = max(contours, key=cv2.contourArea) if contours else None
    box = None
    if contour is not None and cv2.contourArea(contour) > cfg['min_area']:
        box = x, y, w, h = cv2.boundingRect(contour)
        result['BoundingRect'] = (x + w//2, y + h//2)
        moments = cv2.moments(contour)
        if moments['m00']:
            result['Moments'] = (moments['m10']/moments['m00'], moments['m01']/moments['m00'])
        cv2.drawContours(roi, [contour], -1, 255, -1)
        margin = cfg['interior_margin']
        interior = cv2.erode(roi, np.ones((2*margin+1, 2*margin+1), np.uint8))
        value = hsv[:, :, 2]
        gradient = cv2.magnitude(cv2.Sobel(value, cv2.CV_32F, 1, 0),
                                 cv2.Sobel(value, cv2.CV_32F, 0, 1))
        values = gradient[(interior > 0) & (gradient > 0)]
        threshold = np.percentile(values, cfg['gradient_percentile']) if len(values) else np.inf
        edges[0] = cv2.Canny(value, cfg['canny_low'], cfg['canny_high'])
        edges[0][(interior == 0) | (gradient < threshold)] = 0
        gray = cv2.GaussianBlur(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY), (5, 5), 0)
        edges[1] = cv2.Canny(gray, cfg['canny_low'], cfg['canny_high']) & interior
        for m, edge, robust in zip(METHODS, edges, (True, False)):
            result[m] = fit_edges(edge, roi, box, cfg, robust)
        result['TopEllipse'] = agreed_top(result[METHODS[0]], result[METHODS[1]], cfg)
    return result, mask, edges, contour, box


def dashboard(frame, result, mask, edges, contour, box, label):
    overlay = frame.copy()
    if box:
        x, y, w, h = box
        cv2.rectangle(overlay, (x, y), (x+w, y+h), INK['BoundingRect'], 1)
        cv2.drawContours(overlay, [contour], -1, (255, 120, 0), 1)
    for name, color in INK.items():
        item = result[name]
        center = item['center'] if isinstance(item, dict) else item
        if isinstance(item, dict):
            cv2.ellipse(overlay, item['ellipse'], color, 2)
        if center is not None:
            cv2.drawMarker(overlay, tuple(np.rint(center).astype(int)), color, cv2.MARKER_CROSS, 14, 2)
    cv2.drawMarker(overlay, REFERENCE, (255, 255, 255), cv2.MARKER_CROSS, 22, 2)
    edge_view = frame // 3
    edge_view[edges[0] > 0] = INK['A_gradient']
    edge_view[edges[1] > 0] = INK['B_contour']
    panels = [frame.copy(), cv2.cvtColor(mask, cv2.COLOR_GRAY2BGR), overlay, edge_view]
    for p, title in zip(panels, ['Original | '+label, 'HSV Mask',
                                 'Rect=yellow Moments=cyan Top=orange A/B=candidates', 'Internal candidates A / B']):
        cv2.putText(p, title, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, .48, (255, 255, 255), 1)
    for i, name in enumerate(INK):
        item = result[name]
        c = item.get('center') if isinstance(item, dict) else item
        text = 'INVALID' if c is None else f'({c[0]:.1f},{c[1]:.1f}) dx={c[0]-290:.1f} dy={260-c[1]:.1f}'
        cv2.putText(panels[2], f'{name}: {text}', (8, 370+i*22), cv2.FONT_HERSHEY_SIMPLEX,
                    .43, INK[name], 1)
    return np.vstack((np.hstack(panels[:2]), np.hstack(panels[2:])))


WORKER = r'''
import cv2,sys,struct
cv2.setNumThreads(2)
cap=cv2.VideoCapture('/dev/v4l/by-id/usb-Alcorlink_Corp._USB_2.0_Camera-video-index0',cv2.CAP_V4L2)
try:
 if not cap.isOpened(): raise RuntimeError('Camera open failed')
 cap.set(cv2.CAP_PROP_FOURCC,cv2.VideoWriter_fourcc(*'MJPG'))
 cap.set(cv2.CAP_PROP_FRAME_WIDTH,640);cap.set(cv2.CAP_PROP_FRAME_HEIGHT,480)
 cap.set(cv2.CAP_PROP_BUFFERSIZE,1)
 while True:
  ok,frame=cap.read()
  if not ok: raise RuntimeError('Camera read failed')
  if frame.shape[:2]!=(480,640): raise RuntimeError('Not 640x480')
  ok,jpg=cv2.imencode('.jpg',frame,[cv2.IMWRITE_JPEG_QUALITY,95])
  if not ok: raise RuntimeError('JPEG encode failed')
  data=jpg.tobytes();sys.stdout.buffer.write(struct.pack('!I',len(data))+data);sys.stdout.buffer.flush()
except (BrokenPipeError,KeyboardInterrupt): pass
finally: cap.release()
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', type=Path, default=CONFIG)
    p.add_argument('--host', default='cpi')
    p.add_argument('--image', type=Path)
    p.add_argument('--duration', type=float, default=60)
    p.add_argument('--label', default='unlabelled')
    p.add_argument('--output', type=Path, required=True, help='Local untracked results directory')
    p.add_argument('--headless', action='store_true')
    args = p.parse_args()
    if args.duration <= 0:
        p.error('--duration must be positive')
    cfg = json.loads(args.config.read_text(encoding='utf-8'))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'config.json').write_text(json.dumps(cfg, indent=2), encoding='utf-8')
    process = None
    latest, errors = queue.Queue(maxsize=1), queue.Queue()
    stop = threading.Event()
    received = skipped = 0

    def receive():
        nonlocal received, skipped
        def exact(n):
            data = bytearray()
            while len(data) < n:
                part = process.stdout.read(n-len(data))
                if not part: raise EOFError('SSH stream ended')
                data.extend(part)
            return data
        try:
            while not stop.is_set():
                size, = struct.unpack('!I', exact(4))
                if not 0 < size < 4000000: raise ValueError('Invalid JPEG size')
                frame = cv2.imdecode(np.frombuffer(exact(size), np.uint8), cv2.IMREAD_COLOR)
                if frame is None: raise ValueError('JPEG decode failure')
                received += 1
                if latest.full():
                    try:
                        latest.get_nowait()
                        skipped += 1
                    except queue.Empty:
                        pass
                latest.put_nowait(frame)
        except Exception as exc:
            errors.put(exc)

    rows = []
    thread = None
    title = 'C-PI frustum experiment | 1 center 2 left 3 right 4 up 5 down | Q Esc'
    start = report = time.monotonic()
    first = None
    completed = False
    try:
        if not args.image:
            process = subprocess.Popen(['ssh', '-T', '-o', 'BatchMode=yes', '-o',
                'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=8', '-o', 'ServerAliveInterval=3',
                '-o', 'ServerAliveCountMax=2', args.host, '/home/pi/opencv_test/.venv/bin/python -u -'],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)
            process.stdin.write(WORKER.encode()); process.stdin.close()
            thread = threading.Thread(target=receive, daemon=True); thread.start()
        if not args.headless:
            cv2.namedWindow(title, cv2.WINDOW_NORMAL); cv2.resizeWindow(title, 1280, 960)
        with (args.output / 'frames.csv').open('w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=['frame', 'seconds', 'label', 'method', 'valid',
                'cx', 'cy', 'dx', 'dy', 'residual_px', 'coverage', 'delta_rect_x', 'delta_rect_y'])
            writer.writeheader()
            while True:
                if not errors.empty(): raise errors.get()
                try:
                    frame = cv2.imread(str(args.image)) if args.image else latest.get(timeout=12)
                except queue.Empty: raise TimeoutError('No image for 12 seconds')
                now = time.monotonic()
                if first is None: first = now
                result, mask, edges, contour, box = analyze(frame, cfg)
                count = len(rows)//len(INK) + 1
                for name, item in result.items():
                    c = item.get('center') if isinstance(item, dict) else item
                    rect = result['BoundingRect']
                    row = dict(frame=count, seconds=now-first, label=args.label, method=name, valid=c is not None,
                        cx=c[0] if c else '', cy=c[1] if c else '', dx=c[0]-290 if c else '', dy=260-c[1] if c else '',
                        residual_px=item['residual'] if isinstance(item, dict) else '',
                        coverage=item['coverage'] if isinstance(item, dict) else '',
                        delta_rect_x=c[0]-rect[0] if c and rect else '', delta_rect_y=c[1]-rect[1] if c and rect else '')
                    rows.append(row); writer.writerow(row)
                view = dashboard(frame, result, mask, edges, contour, box, args.label)
                if now-report >= cfg['report_seconds'] or args.image:
                    for row in rows[-len(INK):]:
                        center_text = ('INVALID' if not row['valid'] else
                            f"cx={row['cx']:.2f}, cy={row['cy']:.2f}, dx={row['dx']:.2f}, dy={row['dy']:.2f}")
                        print(f"{row['method']}: {center_text}", flush=True)
                    print('FitResidual:', {m: result[m]['residual'] if result[m] else None for m in (*METHODS, 'TopEllipse')}, flush=True)
                    print('FitValid:', result['TopEllipse'] is not None,
                          'CandidateValid:', {m: result[m] is not None for m in METHODS}, flush=True)
                    f.flush()
                    cv2.imwrite(str(args.output / 'last_raw.jpg'), frame)
                    cv2.imwrite(str(args.output / 'last_dashboard.jpg'), view)
                    report = now
                if not args.headless:
                    cv2.imshow(title, view)
                    key = cv2.waitKey(1) & 255
                    if key in (27, ord('q')) or cv2.getWindowProperty(title, cv2.WND_PROP_VISIBLE) == 0: break
                    if key in range(ord('1'), ord('6')):
                        args.label = ['center', 'left', 'right', 'up', 'down'][key-ord('1')]
                if args.image or now-first >= args.duration: break
            cv2.imwrite(str(args.output / 'last_dashboard.jpg'), view)
            completed = True
    finally:
        stop.set()
        if process:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
            if thread: thread.join(timeout=5)
            process.stdout.close()
        if not args.headless: cv2.destroyAllWindows()
        summary = {'received': received, 'preview_skipped': skipped,
                   'processed': len(rows)//len(INK), 'elapsed_seconds': time.monotonic()-start,
                   'completed': completed,
                   'receiver_alive': thread is not None and thread.is_alive(),
                   'ssh_reaped': process is None or process.poll() is not None, 'groups': {}}
        for label in sorted({r['label'] for r in rows}):
            for method in INK:
                group = [r for r in rows if r['label'] == label and r['method'] == method]
                valid = [r for r in group if r['valid']]
                centers = np.array([[r['cx'], r['cy']] for r in valid])
                steps = [np.hypot(b['cx']-a['cx'], b['cy']-a['cy']) for a, b in zip(group, group[1:])
                         if a['valid'] and b['valid']]
                summary['groups'][label+'/'+method] = {'frames': len(group), 'valid': len(valid),
                    'success_rate': len(valid)/len(group),
                    'consecutive_step_rms_px': float(np.sqrt(np.mean(np.square(steps)))) if steps else None,
                    'mean_center': centers.mean(axis=0).tolist() if valid else None,
                    'std_xy_px': centers.std(axis=0).tolist() if valid else None,
                    'mean_delta_rect': np.mean([[r['delta_rect_x'], r['delta_rect_y']] for r in valid], axis=0).tolist() if valid else None}
        (args.output/'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
        print('STOP', json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
