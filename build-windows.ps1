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
    [switch]$CreateMsi,
    [string]$WixToolsDir = $env:WIX_TOOLS_DIR,
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
$MsiWorkDir = Join-Path $BuildDir "msi"
$MsiPath = Join-Path $BuildDir "Vincent-$Version-Windows.msi"

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

function Resolve-Objdump {
    param([string]$ResolvedQtPrefix)

    $candidates = @()
    $command = Get-Command "objdump.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    if ($ResolvedQtPrefix -match "mingw") {
        $qtRoot = Split-Path -Parent (Split-Path -Parent $ResolvedQtPrefix)
        if ($qtRoot) {
            $candidates += Get-ChildItem -Path (Join-Path $qtRoot "Tools") -Filter "objdump.exe" -Recurse -File -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return ""
}

function Test-BinaryMentionsSymbol {
    param(
        [string]$Objdump,
        [string]$Binary,
        [string]$Symbol
    )

    $output = & $Objdump -p $Binary 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect binary imports: $Binary"
    }

    return @($output | Select-String -SimpleMatch $Symbol).Count -gt 0
}

function Assert-MinGwRuntimeCompatibility {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

    if ($ResolvedQtPrefix -notmatch "mingw") {
        return
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $objdump) {
        throw "objdump.exe was not found. MinGW Windows packages require objdump-based DLL compatibility checks."
    }

    $libstdcpp = Join-Path $Directory "libstdc++-6.dll"
    if (-not (Test-Path $libstdcpp)) {
        throw "MinGW runtime deployment is incomplete: libstdc++-6.dll is missing."
    }

    $symbol = "__cxa_thread_atexit"
    $runtimeExportsSymbol = Test-BinaryMentionsSymbol -Objdump $objdump -Binary $libstdcpp -Symbol $symbol
    $incompatibleDlls = @(
        Get-ChildItem -Path $Directory -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ne "libstdc++-6.dll" } |
            Where-Object { Test-BinaryMentionsSymbol -Objdump $objdump -Binary $_.FullName -Symbol $symbol }
    )

    if (($incompatibleDlls.Count -gt 0) -and (-not $runtimeExportsSymbol)) {
        $names = ($incompatibleDlls | ForEach-Object { $_.Name }) -join ", "
        throw "MinGW ABI mismatch: $names imports $symbol, but staged libstdc++-6.dll does not export it. Rebuild those dependencies with the same MinGW kit as Qt: $ResolvedQtPrefix"
    }
}

function Resolve-WixTools {
    param([string]$ConfiguredToolsDir)

    $candidates = @()
    if ($ConfiguredToolsDir) {
        $candidates += $ConfiguredToolsDir
    }
    if ($env:WIX) {
        $candidates += $env:WIX
        $candidates += Join-Path $env:WIX "bin"
    }
    $candidates += Join-Path $RepositoryRoot "build\tools\wix314"

    $heatCommand = Get-Command "heat.exe" -ErrorAction SilentlyContinue
    if ($heatCommand) {
        $candidates += Split-Path -Parent $heatCommand.Source
    }

    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }
        if ((Test-Path (Join-Path $candidate "heat.exe")) -and
            (Test-Path (Join-Path $candidate "candle.exe")) -and
            (Test-Path (Join-Path $candidate "light.exe"))) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "WiX Toolset was not found. Set WIX_TOOLS_DIR to a directory containing heat.exe, candle.exe, and light.exe."
}

function Write-MsiProductFile {
    param(
        [string]$OutputPath,
        [string]$ProductVersion
    )

    $productSource = @'
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="*"
           Codepage="1251"
           Name="Vincent"
           Language="1033"
           Version="%%PRODUCT_VERSION%%"
           Manufacturer="IISACC"
           UpgradeCode="5F580AD5-3A19-4E89-924A-8B7C7A3A9F9B">
    <Package InstallerVersion="500"
             Compressed="yes"
             InstallScope="perUser"
             Description="Vincent %%PRODUCT_VERSION%% Windows Installer" />
    <MajorUpgrade AllowSameVersionUpgrades="yes"
                  DowngradeErrorMessage="A newer version of Vincent is already installed." />
    <MediaTemplate EmbedCab="yes" />
    <Icon Id="VincentIcon" SourceFile="$(var.SourceDir)\resources\Appicon.ico" />
    <Property Id="ARPPRODUCTICON" Value="VincentIcon" />
    <Property Id="WIXUI_INSTALLDIR" Value="INSTALLFOLDER" />
    <UIRef Id="WixUI_InstallDir" />
    <UIRef Id="WixUI_ErrorProgressText" />

    <Feature Id="MainFeature" Title="Vincent" Level="1">
      <ComponentGroupRef Id="VincentRuntime" />
      <ComponentRef Id="ApplicationShortcutComponent" />
    </Feature>
  </Product>

  <Fragment>
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="LocalAppDataFolder">
        <Directory Id="LocalProgramsFolder" Name="Programs">
          <Directory Id="INSTALLFOLDER" Name="Vincent" />
        </Directory>
      </Directory>
      <Directory Id="ProgramMenuFolder">
        <Directory Id="ApplicationProgramsFolder" Name="Vincent" />
      </Directory>
    </Directory>
  </Fragment>

  <Fragment>
    <DirectoryRef Id="ApplicationProgramsFolder">
      <Component Id="ApplicationShortcutComponent" Guid="D3D91B09-F3E7-4CA5-8E42-F5F25C8ACD2A" Win64="yes">
        <Shortcut Id="ApplicationStartMenuShortcut"
                  Name="Vincent"
                  Description="Launch Vincent"
                  Target="[INSTALLFOLDER]Vincent.exe"
                  WorkingDirectory="INSTALLFOLDER" />
        <RemoveFolder Id="ApplicationProgramsFolder" On="uninstall" />
        <RegistryValue Root="HKCU"
                       Key="Software\IISACC\Vincent"
                       Name="installed"
                       Type="integer"
                       Value="1"
                       KeyPath="yes" />
      </Component>
    </DirectoryRef>
  </Fragment>
</Wix>
'@

    $productSource = $productSource.Replace("%%PRODUCT_VERSION%%", $ProductVersion)
    Set-Content -Path $OutputPath -Value $productSource -Encoding UTF8
}

function Remove-NonAsciiHarvestedFiles {
    param([string]$HarvestPath)

    $content = Get-Content -Path $HarvestPath -Raw
    $nonAsciiComponentIds = [regex]::Matches($content, '(?ms)<Component Id="([^"]+)"[^>]*>\s*<File [^>]*Source="[^"]*[^\x00-\x7F][^"]*"[^>]*/>\s*</Component>') |
        ForEach-Object { $_.Groups[1].Value } |
        Sort-Object -Unique

    foreach ($id in $nonAsciiComponentIds) {
        $componentPattern = '(?ms)^\s*<Component Id="' + [regex]::Escape($id) + '".*?</Component>\r?\n'
        $refPattern = '(?m)^\s*<ComponentRef Id="' + [regex]::Escape($id) + '" />\r?\n'
        $content = [regex]::Replace($content, $componentPattern, "")
        $content = [regex]::Replace($content, $refPattern, "")
    }

    Set-Content -Path $HarvestPath -Value $content -Encoding UTF8
}

function New-MsiInstaller {
    param(
        [string]$SourceDirectory,
        [string]$OutputPath,
        [string]$WorkDirectory,
        [string]$ToolsDirectory,
        [string]$ProductVersion
    )

    $wixTools = Resolve-WixTools -ConfiguredToolsDir $ToolsDirectory
    $heat = Join-Path $wixTools "heat.exe"
    $candle = Join-Path $wixTools "candle.exe"
    $light = Join-Path $wixTools "light.exe"
    $productPath = Join-Path $WorkDirectory "VincentProduct.wxs"
    $runtimePath = Join-Path $WorkDirectory "VincentRuntime.wxs"

    New-Item -ItemType Directory -Force $WorkDirectory | Out-Null
    Write-MsiProductFile -OutputPath $productPath -ProductVersion $ProductVersion
    Invoke-Native $heat @("dir", $SourceDirectory, "-cg", "VincentRuntime", "-dr", "INSTALLFOLDER", "-srd", "-sreg", "-sfrag", "-gg", "-var", "var.StageDir", "-out", $runtimePath)
    Remove-NonAsciiHarvestedFiles -HarvestPath $runtimePath

    Remove-Item -Path (Join-Path $WorkDirectory "*.wixobj"), $OutputPath -Force -ErrorAction SilentlyContinue
    Invoke-Native $candle @("-dStageDir=$SourceDirectory", "-dSourceDir=$RepositoryRoot", "-arch", "x64", "-out", "$WorkDirectory\", $productPath, $runtimePath)
    Invoke-Native $light @("-ext", "WixUIExtension", "-cultures:en-us", "-sice:ICE38", "-sice:ICE64", "-sice:ICE91", "-out", $OutputPath, (Join-Path $WorkDirectory "VincentProduct.wixobj"), (Join-Path $WorkDirectory "VincentRuntime.wixobj"))
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

function Remove-QmlResourcePreferDirectives {
    param(
        [string]$ModuleName,
        [string]$Destination
    )

    $moduleRoot = Join-Path (Join-Path $Destination "qml") $ModuleName
    if (-not (Test-Path $moduleRoot)) {
        return
    }

    Get-ChildItem -Path $moduleRoot -Filter "qmldir" -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $originalLines = @(Get-Content -Path $_.FullName)
        $filteredLines = @($originalLines | Where-Object {
            $_ -notmatch '^\s*prefer\s+:/qt/qml/'
        })

        if ($filteredLines.Count -ne $originalLines.Count) {
            Set-Content -Path $_.FullName -Value $filteredLines -Encoding UTF8
        }
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
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

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

    Assert-MinGwRuntimeCompatibility -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
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
    Remove-Item $MsiPath -Force -ErrorAction SilentlyContinue
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
Remove-QmlResourcePreferDirectives -ModuleName "LVRS" -Destination $StageDir
Verify-WindowsStage -Directory $StageDir -ResolvedQtPrefix $QtPrefix

if (-not $SkipPackage) {
    Write-Step "Creating ZIP package"
    Remove-Item $ZipPath -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPath -Force
    Write-Host "Package: $ZipPath"
}

if ($CreateMsi) {
    Write-Step "Creating MSI installer"
    $msiVersion = if ($Version -match '^\d+\.\d+$') { "$Version.0" } else { $Version }
    New-MsiInstaller -SourceDirectory $StageDir -OutputPath $MsiPath -WorkDirectory $MsiWorkDir -ToolsDirectory $WixToolsDir -ProductVersion $msiVersion
    Write-Host "MSI: $MsiPath"
}

if ($InstallForCurrentUser) {
    Install-ForCurrentUser -SourceDirectory $StageDir -TargetDirectory $InstallDir
}

Write-Step "Done"
Write-Host "Staged app: $StageDir"
