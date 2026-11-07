#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
[[ "$root" == /home/pi/cpi-vision ]] || { echo 'Deploy to /home/pi/cpi-vision first'; exit 1; }
[[ -x /home/pi/opencv_test/.venv/bin/python ]] || { echo 'Existing venv missing'; exit 1; }
sudo install -m 644 "$root/systemd/cpi-vision.service" /etc/systemd/system/cpi-vision.service
sudo systemctl daemon-reload
sudo systemctl enable --now cpi-vision.service
