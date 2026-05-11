param(
    [string]$QtVersion = "6.8.3",
    [string]$QtRoot = "C:/Qt",
    [string]$Arch = "win64_msvc2022_64"
)

$ErrorActionPreference = "Stop"

function Get-QtCompilerDirectory {
    param(
        [string]$AqtArch
    )

    if ($AqtArch.StartsWith("win64_")) {
        return $AqtArch.Substring("win64_".Length)
    }

    return $AqtArch
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$compilerDir = Get-QtCompilerDirectory -AqtArch $Arch
$qtPath = Join-Path (Join-Path $QtRoot $QtVersion) $compilerDir

Write-Host "Step 1/2: Setup Qt..."
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "setup_qt_windows.ps1") `
    -QtVersion $QtVersion `
    -QtRoot $QtRoot `
    -Arch $Arch

if ($LASTEXITCODE -ne 0) {
    throw "Qt setup failed."
}

Write-Host "Step 2/2: Build NFSScanner..."
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build_windows_msvc.ps1") `
    -QtPath $qtPath

if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

Write-Host "Development build completed."
Write-Host "Next step:"
Write-Host "powershell -ExecutionPolicy Bypass -File scripts/run_windows_msvc.ps1"
