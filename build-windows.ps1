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
$windowsVersionParts = @($Version -split '\.')
while ($windowsVersionParts.Count -lt 4) {
    $windowsVersionParts += "0"
}
$WindowsFileVersion = $windowsVersionParts[0..3] -join "."
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

function Resolve-Strip {
    param([string]$ResolvedQtPrefix)

    $candidates = @()
    $command = Get-Command "strip.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if ($objdump) {
        $candidates += Join-Path (Split-Path -Parent $objdump) "strip.exe"
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

function Get-PeImportedDllNames {
    param(
        [string]$Objdump,
        [string]$Binary
    )

    $output = & $Objdump -p $Binary 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect PE imports: $Binary"
    }

    return @(
        foreach ($line in $output) {
            if ($line -match 'DLL Name:\s*(?<Name>\S+)\s*$') {
                $Matches.Name
            }
        }
    ) | Sort-Object -Unique
}

function Get-PeHeader {
    param([string]$Binary)

    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Binary)
    if (($bytes.Length -lt 0x40) -or ($bytes[0] -ne 0x4d) -or ($bytes[1] -ne 0x5a)) {
        throw "Not a valid PE image: $Binary"
    }

    $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3c)
    if (($peOffset -lt 0) -or ($peOffset + 96 -gt $bytes.Length)) {
        throw "Invalid PE header offset: $Binary"
    }
    if ([System.BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) {
        throw "PE signature is missing: $Binary"
    }

    $coffHeaderOffset = $peOffset + 4
    $optionalHeaderOffset = $coffHeaderOffset + 20
    return [pscustomobject]@{
        Machine = [System.BitConverter]::ToUInt16($bytes, $coffHeaderOffset)
        PointerToSymbolTable = [System.BitConverter]::ToUInt32($bytes, $coffHeaderOffset + 8)
        OptionalHeaderMagic = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset)
        MajorOperatingSystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 40)
        MinorOperatingSystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 42)
        MajorSubsystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 48)
        MinorSubsystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 50)
        Subsystem = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 68)
        DllCharacteristics = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 70)
    }
}

function Get-PeSubsystem {
    param([string]$Binary)

    return (Get-PeHeader -Binary $Binary).Subsystem
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

function Assert-StagedPeImportClosure {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

    if ($ResolvedQtPrefix -notmatch "mingw") {
        return
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $objdump) {
        throw "objdump.exe was not found. MinGW Windows packages require PE import-closure checks."
    }

    $availableDllNames = @{}
    Get-ChildItem -LiteralPath $Directory -Filter "*.dll" -File -Recurse -ErrorAction SilentlyContinue |
        ForEach-Object { $availableDllNames[$_.Name.ToUpperInvariant()] = $true }

    $systemDirectories = @(
        [Environment]::GetFolderPath([Environment+SpecialFolder]::System),
        $env:SystemRoot
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique
    foreach ($systemDirectory in $systemDirectories) {
        Get-ChildItem -LiteralPath $systemDirectory -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $availableDllNames[$_.Name.ToUpperInvariant()] = $true }
    }

    $stageRoot = [System.IO.Path]::GetFullPath($Directory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    $unresolvedImports = @(
        Get-ChildItem -LiteralPath $Directory -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in @(".exe", ".dll") } |
            ForEach-Object {
                $relativeBinary = $_.FullName.Substring($stageRoot.Length)
                foreach ($importName in Get-PeImportedDllNames -Objdump $objdump -Binary $_.FullName) {
                    $normalizedImport = $importName.ToUpperInvariant()
                    if (($normalizedImport -notlike "API-MS-WIN-*") -and
                        ($normalizedImport -notlike "EXT-MS-*") -and
                        (-not $availableDllNames.ContainsKey($normalizedImport))) {
                        "$relativeBinary -> $importName"
                    }
                }
            }
    )

    if ($unresolvedImports.Count -gt 0) {
        throw "Staged PE import closure is incomplete: $($unresolvedImports -join '; ')"
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

function Remove-EmbeddedDependencyQmlImport {
    param(
        [string]$ModuleName,
        [string]$Destination
    )

    if ($ModuleName -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Invalid QML module name: $ModuleName"
    }

    $destinationRoot = [System.IO.Path]::GetFullPath($Destination)
    $qmlRoot = [System.IO.Path]::GetFullPath((Join-Path $destinationRoot "qml"))
    $moduleRoot = [System.IO.Path]::GetFullPath((Join-Path $qmlRoot $ModuleName))
    $qmlPrefix = $qmlRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $moduleRoot.StartsWith($qmlPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a QML module outside the staged qml directory: $moduleRoot"
    }

    if (-not (Test-Path $moduleRoot)) {
        return
    }

    Remove-Item -LiteralPath $moduleRoot -Recurse -Force
}

function Get-ReleaseOnlyQtArtifactRelativePaths {
    return @(
        "qmltooling",
        "imageformats\qpdf.dll",
        "Qt6Pdf.dll",
        "qml\QtQuick\Pdf",
        "platforminputcontexts\qtvirtualkeyboardplugin.dll",
        "Qt6VirtualKeyboard.dll",
        "qml\QtQuick\VirtualKeyboard",
        "sqldrivers\qsqlpsql.dll",
        "sqldrivers\qsqlmimer.dll"
    )
}

function Remove-ReleaseOnlyQtArtifacts {
    param(
        [string]$Directory,
        [string]$BuildType
    )

    if ($BuildType -eq "Debug") {
        return
    }

    $stageRoot = [System.IO.Path]::GetFullPath($Directory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    foreach ($relativePath in Get-ReleaseOnlyQtArtifactRelativePaths) {
        $artifactPath = [System.IO.Path]::GetFullPath((Join-Path $Directory $relativePath))
        if (-not $artifactPath.StartsWith($stageRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a Qt artifact outside the staged directory: $artifactPath"
        }
        if (Test-Path $artifactPath) {
            Remove-Item -LiteralPath $artifactPath -Recurse -Force
        }
    }
}

function Strip-WindowsRuntimeBinaries {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix,
        [string]$Configuration
    )

    if (($ResolvedQtPrefix -notmatch "mingw") -or ($Configuration -notin @("Release", "MinSizeRel"))) {
        return
    }

    $strip = Resolve-Strip -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $strip) {
        throw "strip.exe was not found. MinGW release packages require symbol stripping."
    }

    foreach ($runtimeName in @("Vincent.exe", "LVRS.dll", "libiiPaintEngine.dll")) {
        $runtimePath = Join-Path $Directory $runtimeName
        if (Test-Path $runtimePath) {
            Invoke-Native $strip @("--strip-all", $runtimePath)
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
        [string]$ResolvedQtPrefix,
        [string]$ExpectedFileVersion,
        [string]$BuildType
    )

    $vincentExe = Join-Path $Directory "Vincent.exe"
    if (-not (Test-Path $vincentExe)) {
        throw "Staged Vincent.exe is missing."
    }

    $peHeader = Get-PeHeader -Binary $vincentExe
    if ($peHeader.Machine -ne 0x8664) {
        throw "Staged Vincent.exe is not an AMD64 native binary (machine=0x$('{0:x4}' -f $peHeader.Machine))."
    }
    if ($peHeader.OptionalHeaderMagic -ne 0x020b) {
        throw "Staged Vincent.exe is not PE32+ (magic=0x$('{0:x4}' -f $peHeader.OptionalHeaderMagic))."
    }
    if ((Get-PeSubsystem -Binary $vincentExe) -ne 2) {
        throw "Staged Vincent.exe must use the Windows GUI subsystem."
    }
    $requiredDllCharacteristics = 0x0160
    if (($peHeader.DllCharacteristics -band $requiredDllCharacteristics) -ne $requiredDllCharacteristics) {
        throw "Staged Vincent.exe is missing required ASLR/DEP PE security flags."
    }
    if (($ResolvedQtPrefix -match "mingw") -and
        ($BuildType -in @("Release", "MinSizeRel")) -and
        ($peHeader.PointerToSymbolTable -ne 0)) {
        throw "Staged MinGW Vincent.exe still contains a COFF symbol table."
    }

    $versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($vincentExe)
    if ($versionInfo.FileVersion -ne $ExpectedFileVersion) {
        throw "Staged Vincent.exe file version '$($versionInfo.FileVersion)' does not match '$ExpectedFileVersion'."
    }
    if ($versionInfo.ProductVersion -ne $ExpectedFileVersion) {
        throw "Staged Vincent.exe product version '$($versionInfo.ProductVersion)' does not match '$ExpectedFileVersion'."
    }
    if ($versionInfo.ProductName -ne "Vincent") {
        throw "Staged Vincent.exe product name is not Vincent."
    }

    if (-not (Get-ChildItem -Path $Directory -Filter "Qt6Core*.dll" -File -ErrorAction SilentlyContinue)) {
        throw "Qt runtime deployment is incomplete: Qt6Core*.dll is missing."
    }

    $windowsPlatformPlugin = Join-Path $Directory "platforms\qwindows.dll"
    if (-not (Test-Path $windowsPlatformPlugin)) {
        throw "Qt runtime deployment is incomplete: platforms\qwindows.dll is missing."
    }

    $quickShapesPlugin = Join-Path $Directory "qml\QtQuick\Shapes\qmlshapesplugin.dll"
    if (-not (Test-Path $quickShapesPlugin)) {
        throw "Qt runtime deployment is incomplete: the Qt Quick Shapes QML plugin is missing."
    }

    $debugQtRuntimes = @(
        Get-ChildItem -Path $Directory -Filter "Qt6*.dll" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.VersionInfo.IsDebug }
    )
    if (($BuildType -ne "Debug") -and ($debugQtRuntimes.Count -gt 0)) {
        throw "Release staging contains debug Qt runtime DLLs: $($debugQtRuntimes.Name -join ', ')."
    }

    if ($BuildType -ne "Debug") {
        foreach ($developmentOnlyPath in Get-ReleaseOnlyQtArtifactRelativePaths) {
            if (Test-Path (Join-Path $Directory $developmentOnlyPath)) {
                throw "Release staging contains an excluded development or unused plugin: $developmentOnlyPath"
            }
        }
    }

    Assert-MinGwRuntimeCompatibility -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
    Assert-StagedPeImportClosure -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
}

function Resolve-SafeCurrentUserInstallDirectory {
    param(
        [string]$TargetDirectory,
        [string]$SourceDirectory
    )

    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if (-not $localAppData) {
        throw "The current user's Local Application Data directory could not be resolved."
    }
    if (-not $TargetDirectory) {
        $TargetDirectory = Join-Path $localAppData "Programs\Vincent"
    }

    $localAppDataRoot = [System.IO.Path]::GetFullPath($localAppData).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    )
    $localAppDataPrefix = $localAppDataRoot + [System.IO.Path]::DirectorySeparatorChar
    $resolvedTarget = [System.IO.Path]::GetFullPath($TargetDirectory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    )
    if (-not $resolvedTarget.StartsWith($localAppDataPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "The current-user install directory must be below $localAppDataRoot."
    }

    if ($SourceDirectory) {
        $resolvedSource = [System.IO.Path]::GetFullPath($SourceDirectory).TrimEnd(
            [System.IO.Path]::DirectorySeparatorChar
        )
        $targetPrefix = $resolvedTarget + [System.IO.Path]::DirectorySeparatorChar
        $sourcePrefix = $resolvedSource + [System.IO.Path]::DirectorySeparatorChar
        if ($resolvedTarget.Equals($resolvedSource, [System.StringComparison]::OrdinalIgnoreCase) -or
            $resolvedTarget.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
            $resolvedSource.StartsWith($targetPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "The current-user install directory must not overlap the staged source directory."
        }
    }

    if (Test-Path -LiteralPath $resolvedTarget) {
        $targetItem = Get-Item -LiteralPath $resolvedTarget -Force
        if (-not $targetItem.PSIsContainer) {
            throw "The current-user install target is not a directory: $resolvedTarget"
        }
        if (($targetItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "The current-user install directory must not be a reparse point: $resolvedTarget"
        }

        $existingEntries = @(Get-ChildItem -LiteralPath $resolvedTarget -Force -ErrorAction Stop)
        if ($existingEntries.Count -gt 0) {
            $ownershipMarker = Join-Path $resolvedTarget ".vincent-install-root"
            $installedExecutable = Join-Path $resolvedTarget "Vincent.exe"
            $isExistingVincentInstall = (Test-Path -LiteralPath $ownershipMarker)
            if ((-not $isExistingVincentInstall) -and (Test-Path -LiteralPath $installedExecutable)) {
                $isExistingVincentInstall = (Get-Item -LiteralPath $installedExecutable).VersionInfo.ProductName -eq "Vincent"
            }
            if (-not $isExistingVincentInstall) {
                throw "Refusing to replace a non-empty directory that is not an existing Vincent installation: $resolvedTarget"
            }
        }
    }

    return $resolvedTarget
}

function Install-ForCurrentUser {
    param(
        [string]$SourceDirectory,
        [string]$TargetDirectory
    )

    $TargetDirectory = Resolve-SafeCurrentUserInstallDirectory `
        -TargetDirectory $TargetDirectory `
        -SourceDirectory $SourceDirectory

    Write-Step "Installing Vincent for the current user"
    if (Test-Path -LiteralPath $TargetDirectory) {
        Remove-Item -LiteralPath $TargetDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Force $TargetDirectory | Out-Null
    Copy-Item (Join-Path $SourceDirectory "*") -Destination $TargetDirectory -Recurse -Force
    Set-Content -LiteralPath (Join-Path $TargetDirectory ".vincent-install-root") `
        -Value "Vincent current-user installation" `
        -Encoding ASCII

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
$buildTesting = if ($SkipTests) { "OFF" } else { "ON" }

Write-Step "Configuring Vincent"
Invoke-Native $CMake @(
    "-S", $RepositoryRoot,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBUILD_TESTING=$buildTesting",
    "-DCMAKE_PREFIX_PATH=$prefixPath"
)

Write-Step "Building Vincent"
$buildArguments = @("--build", $BuildDir, "--config", $BuildType, "--parallel")
if ($SkipTests) {
    $buildArguments += @("--target", "Vincent")
}
Invoke-Native $CMake $buildArguments

if (-not $SkipTests) {
    Write-Step "Running tests"
    Invoke-Native $CTest @("--test-dir", $BuildDir, "--output-on-failure", "-C", $BuildType)
}

Write-Step "Staging Windows runtime"
New-Item -ItemType Directory -Force $DistRoot | Out-Null
Remove-Item $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $StageDir | Out-Null

$VincentExecutable = Resolve-VincentExecutable -BuildDirectory $BuildDir -Configuration $BuildType
$StagedVincentExecutable = Join-Path $StageDir "Vincent.exe"
Copy-Item $VincentExecutable -Destination $StagedVincentExecutable -Force
if ((Get-FileHash -Algorithm SHA256 $VincentExecutable).Hash -ne (Get-FileHash -Algorithm SHA256 $StagedVincentExecutable).Hash) {
    throw "Staged Vincent.exe does not match the executable produced by the current build."
}

$deployMode = if ($BuildType -eq "Debug") { "--debug" } else { "--release" }
Invoke-Native $WindeployQt @(
    $deployMode,
    "--force",
    "--compiler-runtime",
    "--translations", "en,ko",
    "--skip-plugin-types", "qmltooling",
    "--exclude-plugins", "qpdf,qtvirtualkeyboardplugin",
    "--verbose", "0",
    "--qmldir", (Join-Path $RepositoryRoot "App\qml"),
    $StagedVincentExecutable
)

Copy-DependencyRuntimeFiles -Name "LVRS" -Prefix $LVRSPrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiPaintEngine" -Prefix $IiPaintEnginePrefix -Destination $StageDir
Remove-EmbeddedDependencyQmlImport -ModuleName "LVRS" -Destination $StageDir
Remove-ReleaseOnlyQtArtifacts -Directory $StageDir -BuildType $BuildType
Strip-WindowsRuntimeBinaries -Directory $StageDir -ResolvedQtPrefix $QtPrefix -Configuration $BuildType
Verify-WindowsStage -Directory $StageDir -ResolvedQtPrefix $QtPrefix -ExpectedFileVersion $WindowsFileVersion -BuildType $BuildType

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
