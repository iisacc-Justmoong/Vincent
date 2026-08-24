#Requires -Version 5.1

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-NecessarySigningEndpoint {
    return [System.Uri]::new("https://sign.necessary.nu/windows/sign")
}

function Normalize-NecessaryCertificateThumbprint {
    param([string]$Thumbprint)

    $text = if ($null -eq $Thumbprint) { "" } else { [string]$Thumbprint }
    $normalized = ($text -replace '\s', '').ToUpperInvariant()
    if ($normalized -notmatch '^[0-9A-F]{40}$') {
        throw "Necessary signer certificate thumbprint must contain exactly 40 hexadecimal characters."
    }
    return $normalized
}

function Assert-NecessarySigningConfiguration {
    param(
        [string]$OsslSignCodePath,
        [string]$SignToolPath,
        [string]$SigningToken,
        [string]$TimestampUrl,
        [string]$TemporaryRoot
    )

    if ([string]::IsNullOrWhiteSpace($SigningToken)) {
        throw "NECESSARY_SIGN_TOKEN is required for public release signing."
    }

    if ([string]::IsNullOrWhiteSpace($OsslSignCodePath) -or
        (-not (Test-Path -LiteralPath $OsslSignCodePath -PathType Leaf))) {
        throw "The pinned osslsigncode executable was not found: $OsslSignCodePath"
    }
    if ([System.IO.Path]::GetFileName($OsslSignCodePath) -ine "osslsigncode.exe") {
        throw "The signing helper must use the pinned osslsigncode.exe executable."
    }

    if ([string]::IsNullOrWhiteSpace($SignToolPath) -or
        (-not (Test-Path -LiteralPath $SignToolPath -PathType Leaf))) {
        throw "Windows SDK SignTool was not found: $SignToolPath"
    }
    if ([System.IO.Path]::GetFileName($SignToolPath) -ine "signtool.exe") {
        throw "The signing helper must use Windows SDK signtool.exe for final policy verification."
    }

    $timestampUri = $null
    $validTimestampUri = [System.Uri]::TryCreate(
        $TimestampUrl,
        [System.UriKind]::Absolute,
        [ref]$timestampUri
    )
    if ((-not $validTimestampUri) -or ($timestampUri.Scheme -notin @("http", "https"))) {
        throw "TimestampUrl must be an absolute HTTP or HTTPS RFC 3161 endpoint."
    }

    if ([string]::IsNullOrWhiteSpace($TemporaryRoot) -or
        (-not (Test-Path -LiteralPath $TemporaryRoot -PathType Container))) {
        throw "The Necessary signing temporary root does not exist: $TemporaryRoot"
    }
}

function Invoke-NecessaryNative {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    $nativeOutput = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    foreach ($line in @($nativeOutput)) {
        Write-Host $line
    }
    if ($exitCode -ne 0) {
        throw "Native signing command failed with exit code ${exitCode}: $FilePath"
    }
}

function Invoke-NecessarySigningRequest {
    param(
        [System.Uri]$Endpoint,
        [string]$SigningToken,
        [string]$InputPath,
        [string]$OutputPath
    )

    if ($Endpoint.AbsoluteUri -cne (Get-NecessarySigningEndpoint).AbsoluteUri) {
        throw "Refusing to send an Authenticode digest to an unapproved signing endpoint."
    }
    if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
        throw "The extracted Authenticode digest is missing: $InputPath"
    }

    $previousProgressPreference = $ProgressPreference
    try {
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest `
            -Uri $Endpoint `
            -Method Post `
            -ContentType "application/octet-stream" `
            -Headers @{ Authorization = "Bearer $SigningToken" } `
            -InFile $InputPath `
            -OutFile $OutputPath `
            -UseBasicParsing
    } finally {
        $ProgressPreference = $previousProgressPreference
    }

    if ((-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) -or
        ((Get-Item -LiteralPath $OutputPath).Length -le 0)) {
        throw "The Necessary HSM service returned an empty detached signature."
    }
}

function Invoke-NecessaryAuthenticodeSigning {
    param(
        [string]$File,
        [string]$OsslSignCodePath,
        [string]$SignToolPath,
        [string]$SigningToken,
        [string]$TimestampUrl,
        [string]$TemporaryRoot,
        [string]$ExpectedPublisher = "Necessary Innovations AB",
        [string]$ExpectedCertificateThumbprint = ""
    )

    Assert-NecessarySigningConfiguration `
        -OsslSignCodePath $OsslSignCodePath `
        -SignToolPath $SignToolPath `
        -SigningToken $SigningToken `
        -TimestampUrl $TimestampUrl `
        -TemporaryRoot $TemporaryRoot

    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
        throw "Authenticode signing target does not exist: $File"
    }
    $resolvedFile = (Resolve-Path -LiteralPath $File).Path
    $extension = [System.IO.Path]::GetExtension($resolvedFile).ToLowerInvariant()
    if ($extension -notin @(".exe", ".dll", ".msi")) {
        throw "Necessary signing accepts only PE executables, DLLs, and MSI files: $resolvedFile"
    }
    if ([string]::IsNullOrWhiteSpace($ExpectedPublisher)) {
        throw "An expected public Publisher identity is required."
    }

    $resolvedOsslSignCode = (Resolve-Path -LiteralPath $OsslSignCodePath).Path
    $resolvedSignTool = (Resolve-Path -LiteralPath $SignToolPath).Path
    $resolvedTemporaryRoot = (Resolve-Path -LiteralPath $TemporaryRoot).Path
    $workDirectory = Join-Path $resolvedTemporaryRoot ("necessary-" + [Guid]::NewGuid().ToString("N"))
    $rootPrefix = $resolvedTemporaryRoot.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    $resolvedWorkDirectory = [System.IO.Path]::GetFullPath($workDirectory)
    if (-not $resolvedWorkDirectory.StartsWith(
        $rootPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to create signing files outside the configured temporary root."
    }

    New-Item -ItemType Directory -Path $resolvedWorkDirectory | Out-Null
    try {
        $digestPath = Join-Path $resolvedWorkDirectory "authenticode-digest.p7"
        $detachedSignaturePath = Join-Path $resolvedWorkDirectory "detached-signature.p7"
        $attachedPath = Join-Path $resolvedWorkDirectory ("attached" + $extension)
        $timestampedPath = Join-Path $resolvedWorkDirectory ("timestamped" + $extension)

        Invoke-NecessaryNative $resolvedOsslSignCode @(
            "extract-data",
            "-h", "sha256",
            "-in", $resolvedFile,
            "-out", $digestPath
        )
        Invoke-NecessarySigningRequest `
            -Endpoint (Get-NecessarySigningEndpoint) `
            -SigningToken $SigningToken `
            -InputPath $digestPath `
            -OutputPath $detachedSignaturePath
        Invoke-NecessaryNative $resolvedOsslSignCode @(
            "attach-signature",
            "-h", "sha256",
            "-sigin", $detachedSignaturePath,
            "-in", $resolvedFile,
            "-out", $attachedPath
        )
        Invoke-NecessaryNative $resolvedOsslSignCode @(
            "add",
            "-h", "sha256",
            "-ts", $TimestampUrl,
            "-in", $attachedPath,
            "-out", $timestampedPath
        )

        $signature = Get-AuthenticodeSignature -LiteralPath $timestampedPath
        if ($signature.Status -ne "Valid") {
            throw "Authenticode public trust validation failed before replacement: $($signature.Status)"
        }
        if (-not $signature.SignerCertificate) {
            throw "The Necessary Authenticode signer certificate is missing."
        }
        $subject = [string]$signature.SignerCertificate.Subject
        if ($subject.IndexOf(
            $ExpectedPublisher,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -lt 0) {
            throw "The Authenticode Publisher is not ${ExpectedPublisher}: $subject"
        }

        $actualThumbprint = Normalize-NecessaryCertificateThumbprint `
            -Thumbprint ([string]$signature.SignerCertificate.Thumbprint)
        if (-not [string]::IsNullOrWhiteSpace($ExpectedCertificateThumbprint)) {
            $expectedThumbprint = Normalize-NecessaryCertificateThumbprint `
                -Thumbprint $ExpectedCertificateThumbprint
            if ($actualThumbprint -cne $expectedThumbprint) {
                throw "The Necessary signer certificate thumbprint changed during the release."
            }
        }
        if (-not $signature.TimeStamperCertificate) {
            throw "The Necessary Authenticode signature has no RFC 3161 timestamp."
        }

        Invoke-NecessaryNative $resolvedSignTool @(
            "verify",
            "/pa",
            "/all",
            "/tw",
            "/v",
            $timestampedPath
        )

        Copy-Item -LiteralPath $timestampedPath -Destination $resolvedFile -Force
        return [pscustomobject]@{
            File = $resolvedFile
            Subject = $subject
            Thumbprint = $actualThumbprint
            TimestampSubject = [string]$signature.TimeStamperCertificate.Subject
        }
    } finally {
        if (Test-Path -LiteralPath $resolvedWorkDirectory -PathType Container) {
            Remove-Item -LiteralPath $resolvedWorkDirectory -Recurse -Force
        }
    }
}
