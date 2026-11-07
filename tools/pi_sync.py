"""Hash-based, additive OpenSSH sync. Default is preflight; never delete files."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = {'.git', '.venv', 'venv', '__pycache__', 'logs', 'log', '.ssh', 'node_modules'}
SUFFIXES = {'.pyc', '.pyo', '.log', '.pem', '.key', '.p12'}

def allowed(name):
    p = PurePosixPath(name)
    return bool(p.parts) and not p.is_absolute() and all(x not in EXCLUDED and x not in ('.', '..') and not x.startswith('.env') and not x.lower().startswith(('id_rsa', 'id_ed25519')) for x in p.parts) and p.suffix.lower() not in SUFFIXES

def digest(data):
    return hashlib.sha256(data).hexdigest()

def snapshot(root):
    result = {}
    if root.is_symlink(): raise RuntimeError('Symlink root refused')
    if not root.exists(): return result
    for current, dirs, files in os.walk(root, followlinks=False):
        dirs[:] = [d for d in dirs if not (Path(current)/d).is_symlink() and allowed((Path(current)/d).relative_to(root).as_posix())]
        for name in files:
            p = Path(current)/name
            rel = p.relative_to(root).as_posix()
            if allowed(rel) and not p.is_symlink():
                data = p.read_bytes()
                if len(data) > 4*1024*1024: raise RuntimeError('File over 4 MiB: '+rel)
                result[rel] = {'hash': digest(data), 'data': base64.b64encode(data).decode()}
    return result

def plan(source, dest, baseline):
    changes, conflicts = [], []
    for name, item in source.items():
        old = dest.get(name, {}).get('hash')
        if old == item['hash']: continue
        # A different destination can only be replaced if it matches the last shared baseline.
        if old is not None and old != baseline.get(name): conflicts.append(name)
        else: changes.append(name)
    return changes, conflicts

def write_files(root, source, expected, names):
    root.mkdir(parents=True, exist_ok=True)
    # Validate all destinations before any write; per-file check again immediately before replace.
    def check(name):
        if not allowed(name): raise RuntimeError('Unsafe path')
        p = root/name
        for parent in [p, *p.parents]:
            if parent.is_symlink(): raise RuntimeError('Symlink refused: '+str(parent))
            if parent == root: break
        value = digest(p.read_bytes()) if p.exists() else None
        if value != expected.get(name, {}).get('hash'): raise RuntimeError('Concurrent change: '+name)
        return p
    for name in names: check(name)
    for name in names:
        p = check(name)
        data = base64.b64decode(source[name]['data'], validate=True)
        if digest(data) != source[name]['hash']: raise RuntimeError('Hash mismatch')
        p.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp = tempfile.mkstemp(prefix='.cpi-sync-', dir=p.parent)
        try:
            with os.fdopen(fd, 'wb') as output: output.write(data)
            check(name)
            os.chmod(tmp, (p.stat().st_mode & 0o777) if p.exists() else (0o755 if p.suffix == '.sh' else 0o644))
            os.replace(tmp, p)
        finally:
            if os.path.exists(tmp): os.unlink(tmp)

def remote_worker():
    req = json.load(__import__('sys').stdin)
    root = Path(req['root'])
    if req['op'] == 'write': write_files(root, req['source'], req['expected'], req['names'])
    print(json.dumps(snapshot(root)))

def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('mode', choices=['deploy', 'pull'])
    a.add_argument('--target', default='pi@192.168.123.50')
    a.add_argument('--remote-dir', default='/home/pi/cpi-vision')
    a.add_argument('--apply', action='store_true')
    args = a.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_.-]+@[A-Za-z0-9_.-]+', args.target): a.error('Expected user@host')
    if not re.fullmatch(r'/[A-Za-z0-9_./-]+', args.remote_dir) or '..' in PurePosixPath(args.remote_dir).parts or args.remote_dir == '/': a.error('Unsafe remote directory')
    # Send fixed Python code through a quoted SSH command; transfer JSON through stdin.
    code = Path(__file__).read_text().split('def main():')[0].replace('ROOT = Path(__file__).resolve().parents[1]', 'ROOT = Path.cwd()') + '\nremote_worker()\n'
    encoded = base64.b64encode(code.encode()).decode()
    command = 'python3 -c "import base64;exec(base64.b64decode(\''+encoded+'\'))"'
    def remote(request):
        request['root'] = args.remote_dir
        result = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes', '-o', 'ConnectTimeout=5', args.target, command], input=json.dumps(request), text=True, capture_output=True, timeout=60)
        if result.returncode: raise RuntimeError(result.stderr.strip() or 'SSH failed')
        return json.loads(result.stdout)
    key = digest((args.target+':'+args.remote_dir).encode())[:20]
    state = ROOT/'.local'/('pi-sync-'+key+'.json')
    baseline = json.loads(state.read_text()) if state.exists() else {}
    local = snapshot(ROOT/'PI')
    distant = remote({'op': 'read'})
    source, dest = (local, distant) if args.mode == 'deploy' else (distant, local)
    changes, conflicts = plan(source, dest, baseline)
    if args.mode == 'pull' and changes:
        result = subprocess.run(['git', '-C', str(ROOT), 'status', '--porcelain', '-z', '--untracked-files=all', '--', 'PI'], capture_output=True)
        if result.returncode: raise RuntimeError('Cannot verify Windows Git status')
        # Conservative: any uncommitted PI change blocks a writing pull.
        if result.stdout: conflicts.append('PI has uncommitted/untracked files; commit or save them first')
    print('Changes:', json.dumps(changes)); print('Conflicts:', json.dumps(conflicts))
    if conflicts: raise RuntimeError('No files written. Resolve differing files manually; first sync never overwrites differing content.')
    if not args.apply:
        print('PRECHECK only. Repeat with -Apply after reviewing. No service restart or deletion.'); return
    if args.mode == 'deploy':
        distant = remote({'op': 'write', 'source': source, 'expected': dest, 'names': changes})
    else:
        write_files(ROOT/'PI', source, dest, changes)
        local = snapshot(ROOT/'PI')
        subprocess.run(['git', '-C', str(ROOT), 'diff', '--stat', '--', 'PI'], check=True)
        subprocess.run(['git', '-C', str(ROOT), 'status', '--short', '--', 'PI'], check=True)
    shared = {name: item['hash'] for name, item in local.items() if distant.get(name, {}).get('hash') == item['hash']}
    state.parent.mkdir(exist_ok=True)
    state.write_text(json.dumps(shared, indent=2))
    print('PASS: additive sync verified; no files deleted, no services restarted.')

if __name__ == '__main__':
    try: main()
    except Exception as exc:
        raise SystemExit('SYNC FAILED: '+str(exc))
