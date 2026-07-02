#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",

    [string]$QtPrefix = $env:QT_PREFIX,
    [string]$LVRSPrefix = $env:LVRS_PREFIX,
    [string]$IiPaintEnginePrefix = $env:IIPAINTENGINE_PREFIX,

    [ValidateSet("Ninja", "Visual Studio 17 2022")]
    [string]$Generator = "Ninja",

    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$SkipPackage,
    [switch]$InstallForCurrentUser,
    [string]$InstallDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Version = "4.0"
$RepositoryRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$BuildDir = Join-Path $RepositoryRoot "build"
$DistRoot = Join-Path $RepositoryRoot "dist"
$StageDir = Join-Path $DistRoot "Vincent-Windows"
$ZipPath = Join-Path $DistRoot "Vincent-$Version-Windows.zip"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "=== $Message ==="
}

function Get-RequiredCommand {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found in PATH."
    }

    return $command.Source
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
    }
}

function Test-AnyPath {
    param([string[]]$Paths)

    foreach ($path in $Paths) {
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    return ""
}

function Resolve-QtPrefix {
    param([string]$ConfiguredPrefix)

    $candidates = @()
    if ($ConfiguredPrefix) {
        $candidates += $ConfiguredPrefix
    }
    if ($env:QTDIR) {
        $candidates += $env:QTDIR
    }

    $windeployqtCommand = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($windeployqtCommand) {
        $candidates += Split-Path -Parent (Split-Path -Parent $windeployqtCommand.Source)
    }

    if (Test-Path "C:\Qt") {
        $qtKits = Get-ChildItem -Path "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Get-ChildItem -Path $_.FullName -Directory -ErrorAction SilentlyContinue |
                    Where-Object {
                        Test-Path (Join-Path $_.FullName "bin\windeployqt.exe")
                    }
            } |
            Sort-Object FullName -Descending
        foreach ($kit in $qtKits) {
            $candidates += $kit.FullName
        }
    }

    foreach ($prefix in $candidates) {
        $windeployqt = Join-Path $prefix "bin\windeployqt.exe"
        $qtConfig = Join-Path $prefix "lib\cmake\Qt6\Qt6Config.cmake"
        if ((Test-Path $windeployqt) -and (Test-Path $qtConfig)) {
            return (Resolve-Path $prefix).Path
        }
    }

    throw "Qt for Windows was not found. Set QT_PREFIX to a Qt kit such as C:\Qt\6.8.3\msvc2022_64."
}

function Resolve-DependencyPrefix {
    param(
        [string]$Name,
        [string]$ConfiguredPrefix,
        [string]$DefaultPrefix,
        [string[]]$ConfigRelativePaths
    )

    $candidates = @()
    if ($ConfiguredPrefix) {
        $candidates += $ConfiguredPrefix
    }
    if ($DefaultPrefix) {
        $candidates += $DefaultPrefix
    }

    foreach ($prefix in $candidates) {
        foreach ($relativePath in $ConfigRelativePaths) {
            if (Test-Path (Join-Path $prefix $relativePath)) {
                return (Resolve-Path $prefix).Path
            }
        }
    }

    throw "$Name was not found. Install the Windows build of $Name and set the matching environment variable or script parameter."
}

function Import-VisualStudioEnvironment {
    param([string]$ResolvedQtPrefix)

    if ($ResolvedQtPrefix -match "mingw") {
        return
    }
    if (Get-Command "cl.exe" -ErrorAction SilentlyContinue) {
        return
    }

    $programFilesX86 = ${env:ProgramFiles(x86)}
    if (-not $programFilesX86) {
        $programFilesX86 = $env:ProgramFiles
    }

    $vsWhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        Write-Warning "cl.exe is not in PATH and vswhere.exe was not found. Run this script from a Developer PowerShell for VS if CMake cannot find MSVC."
        return
    }

    $installationPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installationPath) {
        Write-Warning "Visual Studio C++ tools were not found. Run this script after installing the Desktop development with C++ workload."
        return
    }

    $vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Warning "VsDevCmd.bat was not found under $installationPath."
        return
    }

    Write-Step "Loading Visual Studio build environment"
    $environmentDump = cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 -no_logo && set"
    foreach ($line in $environmentDump) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function Copy-DependencyRuntimeFiles {
    param(
        [string]$Name,
        [string]$Prefix,
        [string]$Destination
    )

    $candidateDirs = @(
        (Join-Path $Prefix "bin"),
        (Join-Path $Prefix "lib"),
        (Join-Path $Prefix "platforms\windows\bin")
    )

    $copied = 0
    foreach ($dir in $candidateDirs) {
        if (-not (Test-Path $dir)) {
            continue
        }

        Get-ChildItem -Path $dir -Filter "*.dll" -File -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-Item $_.FullName -Destination $Destination -Force
            $copied += 1
        }
    }

    if ($copied -eq 0) {
        Write-Warning "No runtime DLLs were copied for $Name from $Prefix. This is acceptable only when the dependency is static."
    }
}

function Copy-DependencyQmlImports {
    param(
        [string]$Name,
        [string]$Prefix,
        [string]$Destination
    )

    $candidateDirs = @(
        (Join-Path $Prefix "lib\qt6\qml"),
        (Join-Path $Prefix "qml"),
        (Join-Path $Prefix "platforms\windows\lib\qt6\qml")
    )

    $targetRoot = Join-Path $Destination "qml"
    $copied = $false
    foreach ($dir in $candidateDirs) {
        if (-not (Test-Path $dir)) {
            continue
        }

        New-Item -ItemType Directory -Force $targetRoot | Out-Null
        Copy-Item (Join-Path $dir "*") -Destination $targetRoot -Recurse -Force
        $copied = $true
    }

    if (-not $copied) {
        Write-Warning "No QML imports were copied for $Name from $Prefix. This is acceptable only when the dependency does not ship QML modules."
    }
}

function Resolve-VincentExecutable {
    param(
        [string]$BuildDirectory,
        [string]$Configuration
    )

    $candidates = @(
        (Join-Path $BuildDirectory "Vincent.exe"),
        (Join-Path $BuildDirectory "$Configuration\Vincent.exe")
    )

    $executable = Test-AnyPath $candidates
    if (-not $executable) {
        throw "Vincent.exe was not produced in build/. Checked: $($candidates -join ', ')"
    }

    return $executable
}

function Verify-WindowsStage {
    param([string]$Directory)

    $vincentExe = Join-Path $Directory "Vincent.exe"
    if (-not (Test-Path $vincentExe)) {
        throw "Staged Vincent.exe is missing."
    }

    if (-not (Get-ChildItem -Path $Directory -Filter "Qt6Core*.dll" -File -ErrorAction SilentlyContinue)) {
        throw "Qt runtime deployment is incomplete: Qt6Core*.dll is missing."
    }

    $windowsPlatformPlugin = Join-Path $Directory "platforms\qwindows.dll"
    if (-not (Test-Path $windowsPlatformPlugin)) {
        throw "Qt runtime deployment is incomplete: platforms\qwindows.dll is missing."
    }
}

function Install-ForCurrentUser {
    param(
        [string]$SourceDirectory,
        [string]$TargetDirectory
    )

    if (-not $TargetDirectory) {
        $TargetDirectory = Join-Path $env:LOCALAPPDATA "Programs\Vincent"
    }

    Write-Step "Installing Vincent for the current user"
    if (Test-Path $TargetDirectory) {
        Remove-Item $TargetDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Force $TargetDirectory | Out-Null
    Copy-Item (Join-Path $SourceDirectory "*") -Destination $TargetDirectory -Recurse -Force

    $programsMenu = [Environment]::GetFolderPath("Programs")
    $shortcutPath = Join-Path $programsMenu "Vincent.lnk"
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $TargetDirectory "Vincent.exe"
    $shortcut.WorkingDirectory = $TargetDirectory
    $shortcut.IconLocation = Join-Path $TargetDirectory "Vincent.exe"
    $shortcut.Save()

    Write-Host "Installed to: $TargetDirectory"
    Write-Host "Start Menu shortcut: $shortcutPath"
}

$isWindowsRuntime = if (Get-Variable IsWindows -ErrorAction SilentlyContinue) {
    $IsWindows
} else {
    $env:OS -eq "Windows_NT"
}
if (-not $isWindowsRuntime) {
    throw "build-windows.ps1 must be run on Windows."
}

Write-Step "Resolving toolchain"
$QtPrefix = Resolve-QtPrefix $QtPrefix
Import-VisualStudioEnvironment $QtPrefix
$CMake = Get-RequiredCommand "cmake.exe"
if ($Generator -eq "Ninja") {
    Get-RequiredCommand "ninja.exe" | Out-Null
}
$CTest = Get-RequiredCommand "ctest.exe"
$WindeployQt = Join-Path $QtPrefix "bin\windeployqt.exe"

$LVRSPrefix = Resolve-DependencyPrefix `
    -Name "LVRS" `
    -ConfiguredPrefix $LVRSPrefix `
    -DefaultPrefix (Join-Path $HOME ".local\LVRS") `
    -ConfigRelativePaths @("LVRSConfig.cmake", "lib\cmake\LVRS\LVRSConfig.cmake", "share\cmake\LVRS\LVRSConfig.cmake")

$IiPaintEnginePrefix = Resolve-DependencyPrefix `
    -Name "iiPaintEngine" `
    -ConfiguredPrefix $IiPaintEnginePrefix `
    -DefaultPrefix (Join-Path $HOME ".local\iiPaintEngine") `
    -ConfigRelativePaths @("lib\cmake\iiPaintEngine\iiPaintEngineConfig.cmake", "iiPaintEngineConfig.cmake")

Write-Host "Qt: $QtPrefix"
Write-Host "LVRS: $LVRSPrefix"
Write-Host "iiPaintEngine: $IiPaintEnginePrefix"

if ($Clean) {
    Write-Step "Cleaning generated Windows artifacts"
    Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $StageDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
}

$prefixPath = @($QtPrefix, $LVRSPrefix, $IiPaintEnginePrefix) -join ";"

Write-Step "Configuring Vincent"
Invoke-Native $CMake @(
    "-S", $RepositoryRoot,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBUILD_TESTING=ON",
    "-DCMAKE_PREFIX_PATH=$prefixPath"
)

Write-Step "Building Vincent"
Invoke-Native $CMake @("--build", $BuildDir, "--config", $BuildType)

if (-not $SkipTests) {
    Write-Step "Running tests"
    Invoke-Native $CTest @("--test-dir", $BuildDir, "--output-on-failure", "-C", $BuildType)
}

Write-Step "Staging Windows runtime"
New-Item -ItemType Directory -Force $DistRoot | Out-Null
Remove-Item $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $StageDir | Out-Null

$VincentExecutable = Resolve-VincentExecutable -BuildDirectory $BuildDir -Configuration $BuildType
Copy-Item $VincentExecutable -Destination (Join-Path $StageDir "Vincent.exe") -Force

$deployMode = if ($BuildType -eq "Debug") { "--debug" } else { "--release" }
Invoke-Native $WindeployQt @(
    $deployMode,
    "--force",
    "--compiler-runtime",
    "--qmldir", (Join-Path $RepositoryRoot "App\qml"),
    (Join-Path $StageDir "Vincent.exe")
)

Copy-DependencyRuntimeFiles -Name "LVRS" -Prefix $LVRSPrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiPaintEngine" -Prefix $IiPaintEnginePrefix -Destination $StageDir
Copy-DependencyQmlImports -Name "LVRS" -Prefix $LVRSPrefix -Destination $StageDir
Verify-WindowsStage $StageDir

if (-not $SkipPackage) {
    Write-Step "Creating ZIP package"
    Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPath -Force
    Write-Host "Package: $ZipPath"
}

if ($InstallForCurrentUser) {
    Install-ForCurrentUser -SourceDirectory $StageDir -TargetDirectory $InstallDir
}

Write-Step "Done"
Write-Host "Staged app: $StageDir"
