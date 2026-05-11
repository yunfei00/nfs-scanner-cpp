$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"

if (-not (Test-Path $buildDir)) {
    Write-Host "NFSScanner.exe was not found. Please run scripts/build_windows_msvc.ps1 first."
    exit 1
}

$exe = Get-ChildItem -Path $buildDir -Filter "NFSScanner.exe" -Recurse |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $exe) {
    Write-Host "NFSScanner.exe was not found. Please run scripts/build_windows_msvc.ps1 first."
    exit 1
}

Write-Host "Starting: $($exe.FullName)"
& $exe.FullName
