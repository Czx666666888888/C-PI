#!/usr/bin/env bash
set -euo pipefail
host=${1:-pi@192.168.123.50}
key=${CPI_SSH_KEY:-$HOME/.ssh/id_ed25519_go2}
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
ssh -i "$key" "$host" 'mkdir -p /home/pi/cpi-vision'
rsync -av --exclude='.venv' --exclude='__pycache__' --exclude='*.pyc' --exclude='*.log' --exclude='tests' -e "ssh -i $key" "$root/" "$host:/home/pi/cpi-vision/"
