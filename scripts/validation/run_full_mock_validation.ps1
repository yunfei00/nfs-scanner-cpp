param(
    [switch]$SkipBuild,
    [string]$Version = "v0.14.0-mock-validated"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$validationDir = Join-Path $repoRoot "validation_output"
$buildExe = Join-Path $repoRoot "build/Release/NFSScannerSelfCheck.exe"
$cliExe = Join-Path $repoRoot "build/Release/NFSScannerCli.exe"
$profileDir = Join-Path $repoRoot "config/profiles"
$portableZip = Join-Path $repoRoot "artifacts/NFSScanner-Windows-Portable-$Version.zip"

function Write-StatusFile {
    param(
        [string]$Path,
        [string]$Status,
        [string]$Detail = ""
    )
    $content = @($Status)
    if ($Detail) { $content += $Detail }
    Set-Content -LiteralPath $Path -Value ($content -join "`n") -Encoding UTF8
}

Write-Host "Repository: $repoRoot"
Write-Host "Validation output: $validationDir"

if (Test-Path $validationDir) {
    Remove-Item -LiteralPath $validationDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $validationDir | Out-Null

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        Write-Host "==> Build"
        & powershell -ExecutionPolicy Bypass -File (Join-Path $repoRoot "scripts/build_windows_msvc.ps1")
        if ($LASTEXITCODE -ne 0) {
            Write-StatusFile (Join-Path $validationDir "build_result.txt") "FAIL" "build_windows_msvc.ps1 exit $LASTEXITCODE"
            throw "Build failed."
        }
        Write-StatusFile (Join-Path $validationDir "build_result.txt") "PASS" "MSVC Release build succeeded"
    } else {
        Write-StatusFile (Join-Path $validationDir "build_result.txt") "SKIP" "SkipBuild requested"
    }

    if (-not (Test-Path $buildExe)) { throw "NFSScannerSelfCheck.exe not found: $buildExe" }
    if (-not (Test-Path $cliExe)) { throw "NFSScannerCli.exe not found: $cliExe" }

    Write-Host "==> SelfCheck"
    $selfCheckLog = Join-Path $validationDir "selfcheck.log"
    & $buildExe 2>&1 | Tee-Object -FilePath $selfCheckLog
    if ($LASTEXITCODE -ne 0) {
        Write-StatusFile (Join-Path $validationDir "selfcheck_result.txt") "FAIL" "See selfcheck.log"
        throw "SelfCheck failed."
    }
    $selfCheckText = Get-Content -LiteralPath $selfCheckLog -Raw
    $passCount = ([regex]::Matches($selfCheckText, "PASS:")).Count
    Set-Content -LiteralPath (Join-Path $validationDir "selfcheck_count.txt") -Value $passCount -Encoding UTF8
    Write-StatusFile (Join-Path $validationDir "selfcheck_result.txt") "PASS" "$passCount tests passed"

    Write-Host "==> Mock validation (NFSScannerCli)"
    & $cliExe --profile mock_all --profile-dir $profileDir --run-full-validation --output $validationDir
    if ($LASTEXITCODE -ne 0) { throw "Mock validation failed." }

    Write-Host "==> Portable package"
    & powershell -ExecutionPolicy Bypass -File (Join-Path $repoRoot "scripts/package_portable_windows.ps1") -Version $Version
    if ($LASTEXITCODE -ne 0) {
        Write-StatusFile (Join-Path $validationDir "portable_result.txt") "FAIL" $portableZip
        throw "Portable packaging failed."
    }
    if (-not (Test-Path $portableZip)) {
        Write-StatusFile (Join-Path $validationDir "portable_result.txt") "FAIL" "Zip not found"
        throw "Portable zip missing: $portableZip"
    }
    $zipInfo = Get-Item $portableZip
    Write-StatusFile (Join-Path $validationDir "portable_result.txt") "PASS" $portableZip "Size=$($zipInfo.Length) bytes"

    Write-Host "==> Generate report"
    & $cliExe --generate-validation-report --input $validationDir --output $validationDir --portable-zip $portableZip
    if ($LASTEXITCODE -ne 0) { throw "Report generation failed." }

    $reportPath = Join-Path $validationDir "FULL_MOCK_VALIDATION_REPORT.md"
    if (-not (Test-Path $reportPath)) { throw "Report not found: $reportPath" }

    Write-Host ""
    Write-Host "Full mock validation PASSED"
    Write-Host "Report: $reportPath"
    Write-Host "Portable: $portableZip"
}
finally {
    Pop-Location
}
