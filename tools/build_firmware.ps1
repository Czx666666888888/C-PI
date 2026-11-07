param([string]$KeilPath = $env:KEIL_UV4)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $KeilPath) {
    $cmd = Get-Command UV4.exe -ErrorAction SilentlyContinue
    if ($cmd) { $KeilPath = $cmd.Source }
    else {
        foreach ($key in @('HKLM:\SOFTWARE\WOW6432Node\Keil\Products\MDK','HKLM:\SOFTWARE\Keil\Products\MDK')) {
            $entry = Get-ItemProperty $key -ErrorAction SilentlyContinue
            if ($entry.Path) { $KeilPath = Join-Path (Split-Path $entry.Path.TrimEnd('\') -Parent) 'UV4\UV4.exe'; break }
        }
    }
}
if (-not $KeilPath -or -not (Test-Path -LiteralPath $KeilPath)) { throw 'Keil missing. Install MDK with ARMCC 5.06u7 and STM32F4xx_DFP 2.8.0; set KEIL_UV4 or -KeilPath.' }
$project = Join-Path $root 'firmware\MDK-ARM\DJI C board.uvprojx'
$dir = Join-Path $root '.local'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir ('keil-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
Write-Host "Command: $KeilPath -r `"$project`" -j0 -o `"$log`""
$process = Start-Process -FilePath $KeilPath -ArgumentList "-r `"$project`" -j0 -o `"$log`"" -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(180000)) { throw "Keil still running (PID $($process.Id)). Inspect its dialogs; build UNVERIFIED. Log: $log" }
if (Test-Path $log) { Get-Content $log | Write-Host }
if ($process.ExitCode -gt 1) { throw "Keil failed: exit $($process.ExitCode). Log: $log" }
if (-not (Test-Path $log) -or (Get-Content $log -Raw) -notmatch '0 Error\(s\)') { throw "No verified zero-error build summary: $log" }
Get-Item (Join-Path $root 'firmware\MDK-ARM\DJI C board\DJI C board.axf'),(Join-Path $root 'firmware\MDK-ARM\DJI C board\DJI C board.hex') | Select-Object FullName,Length,LastWriteTime
