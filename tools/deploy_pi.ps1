param([string]$Target = 'pi@192.168.123.50', [string]$RemoteDir = '/home/pi/cpi-vision', [switch]$Apply)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python 3 is required.' }
$argsList = @((Join-Path $PSScriptRoot 'pi_sync.py'), 'deploy', '--target', $Target, '--remote-dir', $RemoteDir)
if ($Apply) { $argsList += '--apply' }
& python @argsList
if ($LASTEXITCODE) { throw 'Sync failed; inspect conflicts / SSH diagnostics above.' }
