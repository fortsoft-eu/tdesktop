$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$exe = Join-Path $repo 'out\Debug\Telegram.exe'
$python = Join-Path $repo '.build-tools\python310\tools\pythonw.exe'
if (!(Test-Path -LiteralPath $exe)) {
    throw 'Build the Debug client first.'
}
if (!(Test-Path -LiteralPath $python)) {
    throw 'The local 64-bit Python runtime is missing.'
}

$running = Get-Process -Name Telegram -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $exe }
if ($running) {
    throw 'This checkout is already running. Close it before starting diagnostics.'
}

$profile = Join-Path $repo '.local-data'
New-Item -ItemType Directory -Path $profile -Force | Out-Null
$monitor = Join-Path $PSScriptRoot 'capture_local_crash.py'
Start-Process -FilePath $python -ArgumentList ('"' + $monitor + '"') `
    -WorkingDirectory $repo -WindowStyle Hidden

Write-Host 'Diagnostic Telegram started with your existing isolated profile.'
Write-Host 'Crash records stay in .local-data\crash-diagnostics and are never uploaded.'
Write-Host 'Full dumps can contain messages and login keys. Do not share them publicly.'
