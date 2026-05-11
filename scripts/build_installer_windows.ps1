param(
    [string]$Version = "manual-build"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$distExe = Join-Path (Join-Path (Join-Path $repoRoot "dist") "NFSScanner") "NFSScanner.exe"
$artifactsDir = Join-Path $repoRoot "artifacts"
$scriptPath = Join-Path (Join-Path $repoRoot "installer") "nfs_scanner.iss"

function Resolve-AppVersion {
    param(
        [string]$ReleaseVersion
    )

    if ($ReleaseVersion -eq "manual-build") {
        return "0.0.0"
    }

    if ($ReleaseVersion.StartsWith("v")) {
        return $ReleaseVersion.Substring(1)
    }

    return $ReleaseVersion
}

function Resolve-Iscc {
    $candidates = @(
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

Write-Host "Repository: $repoRoot"
Write-Host "Version:    $Version"

if (-not (Test-Path $distExe)) {
    Write-Host "Required portable layout was not found: $distExe"
    Write-Host "First run:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1"
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts/package_portable_windows.ps1 -Version $Version"
    Write-Host "Or download the portable package from GitHub Actions and extract it so dist/NFSScanner/NFSScanner.exe exists."
    throw "dist/NFSScanner/NFSScanner.exe was not found."
}

if (-not (Test-Path $scriptPath)) {
    throw "Inno Setup script was not found: $scriptPath"
}

$iscc = Resolve-Iscc
if (-not $iscc) {
    Write-Host "ISCC.exe was not found."
    Write-Host "Checked:"
    Write-Host "  C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    Write-Host "  C:\Program Files\Inno Setup 6\ISCC.exe"
    Write-Host "Install Inno Setup 6, then rerun this script."
    throw "Inno Setup ISCC.exe was not found."
}

$appVersion = Resolve-AppVersion -ReleaseVersion $Version
$outputFile = "NFSScanner-Setup-$Version"
$expectedInstaller = Join-Path $artifactsDir "$outputFile.exe"

New-Item -ItemType Directory -Force -Path $artifactsDir | Out-Null

Write-Host "ISCC:       $iscc"
Write-Host "AppVersion: $appVersion"
Write-Host "OutputFile: $outputFile"
Write-Host "Building installer..."

& $iscc `
    "/DAppVersion=$appVersion" `
    "/DOutputFile=$outputFile" `
    $scriptPath

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $expectedInstaller)) {
    Write-Host "Expected installer was not found: $expectedInstaller"
    Write-Host "artifacts directory tree:"
    Get-ChildItem -Path $artifactsDir -Recurse -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host $_.FullName }
    throw "Installer output was not found."
}

Write-Host "Installer created successfully:"
Get-Item $expectedInstaller | Format-List FullName,Length,LastWriteTime
