param(
    [string]$Version = "manual-build"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"
$distDir = Join-Path (Join-Path $repoRoot "dist") "NFSScanner"
$artifactsDir = Join-Path $repoRoot "artifacts"
$zipName = "NFSScanner-Windows-Portable-$Version.zip"
$zipPath = Join-Path $artifactsDir $zipName

function Resolve-WinDeployQt {
    $pathCommand = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($pathCommand) {
        return $pathCommand.Source
    }

    $qtEnvNames = @("QT_ROOT_DIR", "Qt6_DIR", "Qt5_DIR", "QTDIR")
    foreach ($name in $qtEnvNames) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if (-not $value) {
            continue
        }

        $candidateRoots = @($value)
        if ($name -in @("Qt6_DIR", "Qt5_DIR")) {
            $candidateRoots += (Split-Path (Split-Path (Split-Path $value -Parent) -Parent) -Parent)
        }

        foreach ($root in $candidateRoots) {
            $candidate = Join-Path $root "bin/windeployqt.exe"
            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    $candidates = @(
        "C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe",
        "C:/Qt/6.8.3/mingw_64/bin/windeployqt.exe"
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

if (-not (Test-Path $buildDir)) {
    throw "Build directory was not found: $buildDir. Run scripts/build_windows_msvc.ps1 first."
}

Write-Host "Searching for NFSScanner.exe under build..."
$exeCandidates = Get-ChildItem -Path $buildDir -Filter "NFSScanner.exe" -Recurse -File |
    Sort-Object FullName

if (-not $exeCandidates -or $exeCandidates.Count -eq 0) {
    Write-Host "NFSScanner.exe was not found. Build directory tree:"
    Get-ChildItem -Path $buildDir -Recurse | ForEach-Object { Write-Host $_.FullName }
    throw "NFSScanner.exe was not found under build. Run scripts/build_windows_msvc.ps1 first."
}

Write-Host "Executable candidates:"
$exeCandidates | ForEach-Object { Write-Host $_.FullName }

$selectedExe = $exeCandidates | Select-Object -First 1
Write-Host "Selected executable: $($selectedExe.FullName)"

if (Test-Path $distDir) {
    Write-Host "Removing existing dist directory: $distDir"
    Remove-Item -LiteralPath $distDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactsDir | Out-Null

$distExe = Join-Path $distDir "NFSScanner.exe"
Write-Host "Copying executable to $distExe..."
Copy-Item -LiteralPath $selectedExe.FullName -Destination $distExe -Force

$winDeploy = Resolve-WinDeployQt
if ($winDeploy) {
    Write-Host "Running windeployqt: $winDeploy"
    & $winDeploy $distExe --release --compiler-runtime
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE."
    }
} else {
    Write-Host "windeployqt was not found. Portable package will not include Qt runtime DLLs."
    Write-Host "Add Qt bin to PATH or install Qt 6.8.3 before packaging a runnable portable build."
}

$assetDirs = @("resources", "docs")
$assetFiles = @("README.md", "LICENSE", "CHANGELOG.md")

foreach ($dir in $assetDirs) {
    $source = Join-Path $repoRoot $dir
    if (Test-Path $source) {
        $destination = Join-Path $distDir $dir
        Write-Host "Copying directory $source to $destination..."
        Copy-Item -Path $source -Destination $destination -Recurse -Force
    } else {
        Write-Host "Skipping missing directory: $source"
    }
}

foreach ($file in $assetFiles) {
    $source = Join-Path $repoRoot $file
    if (Test-Path $source) {
        $destination = Join-Path $distDir $file
        Write-Host "Copying file $source to $destination..."
        Copy-Item -Path $source -Destination $destination -Force
    } else {
        Write-Host "Skipping missing file: $source"
    }
}

if (Test-Path $zipPath) {
    Write-Host "Removing existing zip: $zipPath"
    Remove-Item -LiteralPath $zipPath -Force
}

Write-Host "Creating portable zip: $zipPath"
Compress-Archive -Path $distDir -DestinationPath $zipPath -CompressionLevel Optimal

if (-not (Test-Path $zipPath)) {
    throw "Portable zip was not created: $zipPath"
}

Write-Host "Portable package created successfully:"
Get-Item $zipPath | Format-List FullName,Length,LastWriteTime
