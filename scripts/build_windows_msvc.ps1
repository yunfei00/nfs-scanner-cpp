param(
    [string]$QtPath = "C:/Qt/6.8.3/msvc2022_64",
    [string]$CMakePath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"

function Resolve-CMakePath {
    param(
        [string]$ExplicitPath
    )

    if ($ExplicitPath) {
        if (-not (Test-Path $ExplicitPath)) {
            throw "CMake path does not exist: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        return $cmakeCommand.Source
    }

    $candidates = @(
        "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
        "C:/Program Files/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/CMake/bin/cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "cmake was not found. Please install CMake, add it to PATH, or pass -CMakePath."
}

function Test-CMakeGenerator {
    param(
        [string]$CMakeExe,
        [string]$GeneratorName
    )

    $generatorOutput = & $CMakeExe --help 2>&1 | Out-String
    return ($generatorOutput -match [regex]::Escape($GeneratorName))
}

$cmakeExe = Resolve-CMakePath -ExplicitPath $CMakePath
$generator = "Visual Studio 17 2022"

Write-Host "Repository: $repoRoot"
Write-Host "Qt path:    $QtPath"
Write-Host "CMake:      $cmakeExe"
Write-Host "Generator:  $generator"

$cmakeVersionOutput = & $cmakeExe --version 2>&1
$cmakeVersionExitCode = $LASTEXITCODE
if ($cmakeVersionExitCode -ne 0) {
    throw "Failed to get CMake version from: $cmakeExe"
}
$cmakeVersion = $cmakeVersionOutput | Select-Object -First 1
Write-Host "CMake ver:  $cmakeVersion"

if (-not (Test-Path $QtPath)) {
    throw "Qt path does not exist: $QtPath. Please run: powershell -ExecutionPolicy Bypass -File scripts/setup_qt_windows.ps1"
}

if (-not (Test-Path (Join-Path $QtPath "lib/cmake/Qt6/Qt6Config.cmake"))) {
    throw "Qt6Config.cmake was not found under QtPath: $QtPath. Please run: powershell -ExecutionPolicy Bypass -File scripts/setup_qt_windows.ps1"
}

if (-not (Test-Path (Join-Path $QtPath "lib/cmake/Qt6SerialPort/Qt6SerialPortConfig.cmake"))) {
    throw "Qt6SerialPortConfig.cmake was not found under QtPath: $QtPath. Please run: powershell -ExecutionPolicy Bypass -File scripts/setup_qt_windows.ps1"
}

if (-not (Test-CMakeGenerator -CMakeExe $cmakeExe -GeneratorName $generator)) {
    throw "CMake generator '$generator' is not available. Please install Visual Studio Build Tools 2022 with the 'Desktop development with C++' workload."
}

if (Test-Path $buildDir) {
    Write-Host "Removing existing build directory: $buildDir"
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "Configuring with $generator x64..."
& $cmakeExe -S $repoRoot -B $buildDir `
    -G $generator `
    -A x64 `
    "-DCMAKE_PREFIX_PATH=$QtPath"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed. Please verify QtPath and install Visual Studio Build Tools 2022 with the 'Desktop development with C++' workload."
}

Write-Host "Building Release..."
& $cmakeExe --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed. Please check the compiler output above."
}

$exe = Get-ChildItem -Path $buildDir -Filter "NFSScanner.exe" -Recurse |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $exe) {
    throw "Build finished, but NFSScanner.exe was not found under: $buildDir"
}

Write-Host "Build succeeded."
Write-Host "Executable: $($exe.FullName)"
