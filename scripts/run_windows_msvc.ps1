param(
    [string]$QtPath = "C:/Qt/6.8.3/msvc2022_64"
)

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
if (Test-Path (Join-Path $QtPath "bin")) {
    $env:PATH = "$(Join-Path $QtPath "bin");$env:PATH"
} else {
    Write-Host "Qt bin directory was not found: $(Join-Path $QtPath "bin")"
}
& $exe.FullName
