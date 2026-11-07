$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    git status --short
    git remote -v
    git branch -a
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python 3 required for static checks.' }
    python (Join-Path $PSScriptRoot 'check_firmware.py')
    if ($LASTEXITCODE) { throw 'Firmware references failed.' }
    Get-Command git,ssh,scp,python -ErrorAction SilentlyContinue | Select-Object Name,Source
} finally { Pop-Location }
