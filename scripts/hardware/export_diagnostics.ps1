$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$out = Join-Path $repoRoot "logs\diagnostics\NFSScanner_Diagnostics_$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Copy-Item -Path (Join-Path $repoRoot "config\hardware_config.json") -Destination (Join-Path $out "hardware_config_snapshot.json") -ErrorAction SilentlyContinue
Copy-Item -Path (Join-Path $repoRoot "config\profiles") -Destination (Join-Path $out "profiles") -Recurse -ErrorAction SilentlyContinue
if (Test-Path (Join-Path $repoRoot "logs")) {
    Copy-Item -Path (Join-Path $repoRoot "logs\*.log") -Destination (Join-Path $out "latest_logs") -ErrorAction SilentlyContinue
}
"# Diagnostics Export`nGenerated: $stamp`nSee NFSScanner.exe --export-diagnostics for full GUI export." | Out-File (Join-Path $out "diagnostics.md") -Encoding utf8
Write-Host "Diagnostics folder: $out"
