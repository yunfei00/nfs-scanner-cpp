param(
    [string]$Profile = "motion_only_grbl"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Write-Host "Profile: $Profile (no real serial required for mock checks)"
& (Join-Path $repoRoot "scripts\hardware\run_hardware_self_check.ps1") -Profile $Profile
