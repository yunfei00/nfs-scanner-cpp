param(
    [string]$QtVersion = "6.8.3",
    [string]$QtRoot = "C:/Qt",
    [string]$Arch = "win64_msvc2022_64"
)

$ErrorActionPreference = "Stop"

function Resolve-PythonPath {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        throw "Python was not found. Please install Python 3, enable the Windows Store Python alias, or install/enable Python Launcher, then reopen PowerShell."
    }

    $versionOutput = & $pythonCommand.Source --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Python was found but could not run. Please install Python 3, enable the Windows Store Python alias, or install/enable Python Launcher. Output: $versionOutput"
    }

    return $pythonCommand.Source
}

function Test-PythonModule {
    param(
        [string]$PythonExe,
        [string]$ModuleName,
        [string[]]$Arguments
    )

    try {
        & $PythonExe -m $ModuleName @Arguments *> $null
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

function Get-QtCompilerDirectory {
    param(
        [string]$AqtArch
    )

    if ($AqtArch.StartsWith("win64_")) {
        return $AqtArch.Substring("win64_".Length)
    }

    return $AqtArch
}

$pythonExe = Resolve-PythonPath
$compilerDir = Get-QtCompilerDirectory -AqtArch $Arch
$qtPath = Join-Path (Join-Path $QtRoot $QtVersion) $compilerDir
$qtConfigPath = Join-Path $qtPath "lib/cmake/Qt6/Qt6Config.cmake"
$serialPortConfigPath = Join-Path $qtPath "lib/cmake/Qt6SerialPort/Qt6SerialPortConfig.cmake"

Write-Host "Python:     $pythonExe"
Write-Host "Qt version: $QtVersion"
Write-Host "Qt root:    $QtRoot"
Write-Host "Qt arch:    $Arch"
Write-Host "Qt path:    $qtPath"

if ((Test-Path $qtConfigPath) -and (Test-Path $serialPortConfigPath)) {
    Write-Host "Qt and Qt SerialPort are already installed."
    Write-Host "Qt installed successfully."
    Write-Host "QtPath: $qtPath"
    exit 0
}

Write-Host "Checking pip..."
if (-not (Test-PythonModule -PythonExe $pythonExe -ModuleName "pip" -Arguments @("--version"))) {
    throw "pip is not available for this Python installation. Please install pip or reinstall Python with pip enabled."
}

Write-Host "Checking aqtinstall..."
if (-not (Test-PythonModule -PythonExe $pythonExe -ModuleName "aqt" -Arguments @("--help"))) {
    Write-Host "aqtinstall was not found. Upgrading pip and installing aqtinstall..."
    & $pythonExe -m pip install --upgrade pip
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to upgrade pip."
    }

    & $pythonExe -m pip install aqtinstall
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install aqtinstall."
    }
}

if (Test-Path $qtConfigPath) {
    Write-Host "Qt base is already installed, but Qt SerialPort is missing."
} else {
    Write-Host "Qt base is not installed."
}

Write-Host "Installing Qt with Qt SerialPort using aqtinstall..."
& $pythonExe -m aqt install-qt windows desktop $QtVersion $Arch -m qtserialport --outputdir $QtRoot
if ($LASTEXITCODE -ne 0) {
    throw "aqtinstall failed to install Qt $QtVersion $Arch with qtserialport into $QtRoot."
}

if (-not (Test-Path $qtConfigPath)) {
    throw "Qt installation finished, but Qt6Config.cmake was not found: $qtConfigPath"
}

if (-not (Test-Path $serialPortConfigPath)) {
    throw "Qt installation finished, but Qt6SerialPortConfig.cmake was not found: $serialPortConfigPath"
}

Write-Host "Qt installed successfully."
Write-Host "QtPath: $qtPath"
