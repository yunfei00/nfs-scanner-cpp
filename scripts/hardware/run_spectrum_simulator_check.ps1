param(
    [string]$Device = "zna67"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sim = Join-Path $repoRoot "tools\simulators\scpi_spectrum_simulator.py"
if (-not (Test-Path $sim)) { throw "Simulator not found: $sim" }

Write-Host "Starting SCPI simulator ($Device) on port 5025..."
$proc = Start-Process python -ArgumentList @($sim, "--device", $Device) -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
try {
    & (Join-Path $repoRoot "scripts\hardware\run_hardware_self_check.ps1") -Profile "spectrum_only_$Device"
}
finally {
    if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}
