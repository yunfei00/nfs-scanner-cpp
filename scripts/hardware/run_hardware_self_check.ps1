param(
    [string]$Profile = "mock_all"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $repoRoot

Write-Host "== Build =="
& (Join-Path $repoRoot "scripts\build_windows_msvc.ps1")

$selfCheck = Join-Path $repoRoot "build\Release\NFSScannerSelfCheck.exe"
if (-not (Test-Path $selfCheck)) { throw "SelfCheck not found: $selfCheck" }

Write-Host "== SelfCheck =="
& $selfCheck
if ($LASTEXITCODE -ne 0) { throw "SelfCheck failed" }

Write-Host "== Profile validation ($Profile) =="
$configDir = Join-Path $repoRoot "config\profiles\$Profile.json"
if (-not (Test-Path $configDir)) { Write-Warning "Profile file missing: $configDir" }

Write-Host "== Diagnostics export =="
& (Join-Path $PSScriptRoot "export_diagnostics.ps1")

Write-Host "Hardware self-check pipeline PASS"
