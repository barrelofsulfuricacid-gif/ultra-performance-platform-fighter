[CmdletBinding()]
param(
    [switch]$Web,
    [switch]$VerifyOnly,
    [switch]$NoSmoke,
    [string]$Prefix
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = (& git rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repositoryRoot))
{
    throw "toolchain error: run this command from a Git checkout"
}
$repositoryRoot = $repositoryRoot.Trim()

. (Join-Path $repositoryRoot "tools\toolchain_common.ps1")

$platformKey = Get-PFPlatformKey
$toolchainsDirectory = if ([string]::IsNullOrWhiteSpace($Prefix))
{
    Join-Path $repositoryRoot ".toolchains"
}
else
{
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
        $Prefix)
}
$lockPath = Join-Path $repositoryRoot "dependencies\toolchains.lock.tsv"
if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf))
{
    Stop-PFToolchain "missing archive lock: $lockPath"
}
$lockRecords = @(Import-Csv -LiteralPath $lockPath -Delimiter "`t")

function Get-PFLockRecord
{
    param(
        [Parameter(Mandatory = $true)][string]$Component,
        [Parameter(Mandatory = $true)][string]$Platform
    )

    $matches = @(
        $lockRecords |
            Where-Object {
                $_.component -eq $Component -and $_.platform -eq $Platform
            })
    if ($matches.Count -ne 1)
    {
        Stop-PFToolchain (
            "expected one lock record for $Component/$Platform; " +
            "found $($matches.Count)")
    }

    return $matches[0]
}

function Save-PFLockedArchive
{
    param(
        [Parameter(Mandatory = $true)]$Record,
        [Parameter(Mandatory = $true)][string]$Directory,
        [string]$FileName
    )

    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $destinationName = if ([string]::IsNullOrWhiteSpace($FileName))
    {
        $Record.archive
    }
    else
    {
        $FileName
    }
    $destination = Join-Path $Directory $destinationName

    if (Test-Path -LiteralPath $destination -PathType Leaf)
    {
        $actualHash = (
            Get-FileHash -LiteralPath $destination -Algorithm SHA256
        ).Hash.ToLowerInvariant()
        $actualSize = (Get-Item -LiteralPath $destination).Length.ToString()
        if ($actualHash -eq $Record.sha256 -and
            $actualSize -eq $Record.size)
        {
            Write-Host "archive=verified path=$destination"
            return $destination
        }
    }

    if ($VerifyOnly)
    {
        Stop-PFToolchain "locked archive is missing or invalid: $destination"
    }

    $partial = "$destination.partial.$PID"
    try
    {
        Invoke-WebRequest -Uri $Record.url -OutFile $partial
        $actualHash = (
            Get-FileHash -LiteralPath $partial -Algorithm SHA256
        ).Hash.ToLowerInvariant()
        $actualSize = (Get-Item -LiteralPath $partial).Length.ToString()
        if ($actualHash -ne $Record.sha256)
        {
            Stop-PFToolchain "SHA-256 mismatch for $($Record.url)"
        }
        if ($actualSize -ne $Record.size)
        {
            Stop-PFToolchain "byte-length mismatch for $($Record.url)"
        }

        Move-Item -LiteralPath $partial -Destination $destination -Force
    }
    finally
    {
        if (Test-Path -LiteralPath $partial -PathType Leaf)
        {
            Remove-Item -LiteralPath $partial -Force
        }
    }

    Write-Host "archive=downloaded-and-verified path=$destination"
    return $destination
}

$hostRoot = Join-Path $toolchainsDirectory "host\$platformKey"
$downloadRoot = Join-Path $toolchainsDirectory "downloads"
$temporaryRoot = Join-Path $toolchainsDirectory "tmp"
New-Item -ItemType Directory -Path $hostRoot, $downloadRoot, $temporaryRoot `
    -Force | Out-Null

$cmakeRoot = Join-Path $hostRoot "cmake-4.4.0"
if (-not (Test-Path -LiteralPath $cmakeRoot -PathType Container))
{
    $cmakeRecord = Get-PFLockRecord -Component cmake -Platform $platformKey
    $cmakeArchive = Save-PFLockedArchive `
        -Record $cmakeRecord `
        -Directory $downloadRoot
    $cmakeTemporary = Join-Path $temporaryRoot ("cmake." + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $cmakeTemporary | Out-Null
    Expand-Archive `
        -LiteralPath $cmakeArchive `
        -DestinationPath $cmakeTemporary
    Move-Item -LiteralPath $cmakeTemporary -Destination $cmakeRoot
}

$ninjaRoot = Join-Path $hostRoot "ninja-1.13.2"
if (-not (Test-Path -LiteralPath $ninjaRoot -PathType Container))
{
    $ninjaRecord = Get-PFLockRecord -Component ninja -Platform $platformKey
    $ninjaArchive = Save-PFLockedArchive `
        -Record $ninjaRecord `
        -Directory $downloadRoot
    $ninjaTemporary = Join-Path $temporaryRoot ("ninja." + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $ninjaTemporary | Out-Null
    Expand-Archive `
        -LiteralPath $ninjaArchive `
        -DestinationPath $ninjaTemporary
    Move-Item -LiteralPath $ninjaTemporary -Destination $ninjaRoot
}

$hostTools = Find-PFHostTools `
    -RepositoryRoot $repositoryRoot `
    -ToolchainsDirectory $toolchainsDirectory `
    -PlatformKey $platformKey
Enter-PFVisualStudio -PlatformKey $platformKey

if ($Web)
{
    if ($platformKey -ne "windows-x86_64")
    {
        Stop-PFToolchain (
            "the locked Emscripten Windows SDK currently supports x86_64")
    }
    if ($null -eq (Get-Command python.exe -ErrorAction SilentlyContinue))
    {
        Stop-PFToolchain "Python 3 is required for the pinned Emscripten SDK"
    }

    $emsdkCommit = "db04e88298d9916fc51fcd3743045ca3eb695127"
    $releaseRevision = "9074aa513b501925adb1361e208932ad32a29a5f"
    $emsdkRoot = Join-Path $toolchainsDirectory "web\emsdk-$emsdkCommit"
    if (-not (Test-Path -LiteralPath $emsdkRoot -PathType Container))
    {
        $emsdkRecord = Get-PFLockRecord -Component emsdk -Platform all
        $emsdkArchive = Save-PFLockedArchive `
            -Record $emsdkRecord `
            -Directory $downloadRoot
        $emsdkTemporary = Join-Path `
            $temporaryRoot `
            ("emsdk." + [guid]::NewGuid())
        New-Item -ItemType Directory -Path $emsdkTemporary | Out-Null
        & tar.exe `
            -xzf $emsdkArchive `
            --strip-components=1 `
            -C $emsdkTemporary
        if ($LASTEXITCODE -ne 0)
        {
            Stop-PFToolchain "failed to extract the locked emsdk archive"
        }
        New-Item `
            -ItemType Directory `
            -Path (Split-Path -Parent $emsdkRoot) `
            -Force | Out-Null
        Move-Item -LiteralPath $emsdkTemporary -Destination $emsdkRoot
    }

    $emsdkDownloads = Join-Path $emsdkRoot "downloads"
    $sdkRecord = Get-PFLockRecord `
        -Component emscripten-sdk `
        -Platform $platformKey
    [void](Save-PFLockedArchive `
        -Record $sdkRecord `
        -Directory $emsdkDownloads `
        -FileName "$releaseRevision-$($sdkRecord.archive)")
    $nodeRecord = Get-PFLockRecord -Component node -Platform $platformKey
    [void](Save-PFLockedArchive `
        -Record $nodeRecord `
        -Directory $emsdkDownloads)

    $oldKeepDownloads = $env:EMSDK_KEEP_DOWNLOADS
    try
    {
        $env:EMSDK_KEEP_DOWNLOADS = "1"
        Push-Location $emsdkRoot
        try
        {
            & .\emsdk.bat install 6.0.3
            if ($LASTEXITCODE -ne 0)
            {
                Stop-PFToolchain "emsdk failed to install Emscripten 6.0.3"
            }
            & .\emsdk.bat activate 6.0.3
            if ($LASTEXITCODE -ne 0)
            {
                Stop-PFToolchain "emsdk failed to activate Emscripten 6.0.3"
            }
        }
        finally
        {
            Pop-Location
        }
    }
    finally
    {
        $env:EMSDK_KEEP_DOWNLOADS = $oldKeepDownloads
    }

    $emcc = Join-Path $emsdkRoot "upstream\emscripten\emcc.bat"
    if (-not (Test-Path -LiteralPath $emcc -PathType Leaf))
    {
        Stop-PFToolchain "emcc was not installed"
    }
    $emccVersion = (& $emcc --version | Select-Object -First 1)
    if ($LASTEXITCODE -ne 0 -or $emccVersion -notmatch "6\.0\.3")
    {
        Stop-PFToolchain "installed emcc is not Emscripten 6.0.3"
    }
    Write-Output "emscripten=6.0.3 revision=$releaseRevision"
}

& git -C $repositoryRoot config core.hooksPath .githooks
if ($LASTEXITCODE -ne 0)
{
    Stop-PFToolchain "failed to configure the repository hooks path"
}

if (-not $NoSmoke)
{
    $workflow = Join-Path $repositoryRoot "tools\workflow.ps1"
    $preset = if ($Web) { "web" } else { "headless" }
    & $workflow $preset -ToolchainsDirectory $toolchainsDirectory
    if ($LASTEXITCODE -ne 0)
    {
        Stop-PFToolchain "$preset smoke workflow failed"
    }
}

Write-Output (
    "bootstrap=pass platform=$platformKey toolchains=$toolchainsDirectory")
