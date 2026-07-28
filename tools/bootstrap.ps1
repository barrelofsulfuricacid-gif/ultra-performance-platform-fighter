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

$sdlRoot = Join-Path $toolchainsDirectory "dependencies\SDL3-3.4.12"
if (-not (Test-Path -LiteralPath $sdlRoot -PathType Container))
{
    $sdlRecord = Get-PFLockRecord -Component sdl-source -Platform all
    $sdlArchive = Save-PFLockedArchive `
        -Record $sdlRecord `
        -Directory $downloadRoot
    $sdlTemporary = Join-Path $temporaryRoot ("sdl." + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $sdlTemporary | Out-Null
    & tar.exe `
        -xzf $sdlArchive `
        --strip-components=1 `
        -C $sdlTemporary
    if ($LASTEXITCODE -ne 0)
    {
        Stop-PFToolchain "failed to extract the locked SDL3 archive"
    }
    New-Item `
        -ItemType Directory `
        -Path (Split-Path -Parent $sdlRoot) `
        -Force | Out-Null
    Move-Item -LiteralPath $sdlTemporary -Destination $sdlRoot
}

$sdlVersionHeader = Join-Path $sdlRoot "include\SDL3\SDL_version.h"
if (-not (Test-Path -LiteralPath $sdlVersionHeader -PathType Leaf))
{
    Stop-PFToolchain "locked SDL3 source is incomplete: $sdlRoot"
}
$sdlVersionText = Get-Content -LiteralPath $sdlVersionHeader -Raw
if ($sdlVersionText -notmatch "(?m)^#define SDL_MAJOR_VERSION\s+3$" -or
    $sdlVersionText -notmatch "(?m)^#define SDL_MINOR_VERSION\s+4$" -or
    $sdlVersionText -notmatch "(?m)^#define SDL_MICRO_VERSION\s+12$")
{
    Stop-PFToolchain "locked SDL3 source is not version 3.4.12"
}
$env:PF_SDL_SOURCE_DIR = $sdlRoot
Write-Output "sdl-source=3.4.12 path=$sdlRoot"

$sqliteRoot = Join-Path `
    $toolchainsDirectory `
    "dependencies\sqlite-amalgamation-3530400"
if (-not (Test-Path -LiteralPath $sqliteRoot -PathType Container))
{
    $sqliteRecord = Get-PFLockRecord `
        -Component sqlite-source `
        -Platform all
    $sqliteArchive = Save-PFLockedArchive `
        -Record $sqliteRecord `
        -Directory $downloadRoot
    $sqliteTemporary = Join-Path `
        $temporaryRoot `
        ("sqlite." + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $sqliteTemporary | Out-Null
    Expand-Archive `
        -LiteralPath $sqliteArchive `
        -DestinationPath $sqliteTemporary
    $sqliteExtracted = Join-Path `
        $sqliteTemporary `
        "sqlite-amalgamation-3530400"
    New-Item `
        -ItemType Directory `
        -Path (Split-Path -Parent $sqliteRoot) `
        -Force | Out-Null
    Move-Item -LiteralPath $sqliteExtracted -Destination $sqliteRoot
}

$sqliteHeader = Join-Path $sqliteRoot "sqlite3.h"
$sqliteSource = Join-Path $sqliteRoot "sqlite3.c"
if (-not (Test-Path -LiteralPath $sqliteHeader -PathType Leaf) -or
    -not (Test-Path -LiteralPath $sqliteSource -PathType Leaf))
{
    Stop-PFToolchain "locked SQLite source is incomplete: $sqliteRoot"
}
$sqliteHeaderText = Get-Content -LiteralPath $sqliteHeader -Raw
$sqliteSourceText = Get-Content -LiteralPath $sqliteSource -Raw
if ($sqliteHeaderText -notmatch '#define SQLITE_VERSION\s+"3\.53\.4"')
{
    Stop-PFToolchain "locked SQLite source is not version 3.53.4"
}
if ($sqliteSourceText -notmatch
    "bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc")
{
    Stop-PFToolchain "locked SQLite source ID is incorrect"
}
$env:PF_SQLITE_SOURCE_DIR = $sqliteRoot
Write-Output "sqlite-source=3.53.4 path=$sqliteRoot"

$tracyRoot = Join-Path $toolchainsDirectory "dependencies\tracy-0.13.1"
if (-not (Test-Path -LiteralPath $tracyRoot -PathType Container))
{
    $tracyRecord = Get-PFLockRecord -Component tracy-source -Platform all
    $tracyArchive = Save-PFLockedArchive `
        -Record $tracyRecord `
        -Directory $downloadRoot
    $tracyTemporary = Join-Path `
        $temporaryRoot `
        ("tracy." + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $tracyTemporary | Out-Null
    & tar.exe `
        -xzf $tracyArchive `
        --strip-components=1 `
        -C $tracyTemporary
    if ($LASTEXITCODE -ne 0)
    {
        Stop-PFToolchain "failed to extract the locked Tracy archive"
    }
    New-Item `
        -ItemType Directory `
        -Path (Split-Path -Parent $tracyRoot) `
        -Force | Out-Null
    Move-Item -LiteralPath $tracyTemporary -Destination $tracyRoot
}

$tracyHeader = Join-Path $tracyRoot "public\tracy\TracyC.h"
$tracyClient = Join-Path $tracyRoot "public\TracyClient.cpp"
$tracyVersion = Join-Path `
    $tracyRoot `
    "public\common\TracyVersion.hpp"
if (-not (Test-Path -LiteralPath $tracyHeader -PathType Leaf) -or
    -not (Test-Path -LiteralPath $tracyClient -PathType Leaf) -or
    -not (Test-Path -LiteralPath $tracyVersion -PathType Leaf))
{
    Stop-PFToolchain "locked Tracy source is incomplete: $tracyRoot"
}
$tracyVersionText = Get-Content -LiteralPath $tracyVersion -Raw
if ($tracyVersionText -notmatch "enum \{ Major = 0 \};" -or
    $tracyVersionText -notmatch "enum \{ Minor = 13 \};" -or
    $tracyVersionText -notmatch "enum \{ Patch = 1 \};")
{
    Stop-PFToolchain "locked Tracy source is not version 0.13.1"
}
$env:PF_TRACY_SOURCE_DIR = $tracyRoot
Write-Output "tracy-source=0.13.1 path=$tracyRoot"

function Install-PFTarDependency
{
    param(
        [Parameter(Mandatory = $true)][string]$Component,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Destination -PathType Container))
    {
        $record = Get-PFLockRecord -Component $Component -Platform all
        $archive = Save-PFLockedArchive `
            -Record $record `
            -Directory $downloadRoot
        $temporary = Join-Path `
            $temporaryRoot `
            ($Component + "." + [guid]::NewGuid())
        New-Item -ItemType Directory -Path $temporary | Out-Null
        & tar.exe `
            -xzf $archive `
            --strip-components=1 `
            -C $temporary
        if ($LASTEXITCODE -ne 0)
        {
            Stop-PFToolchain "failed to extract locked $Component archive"
        }
        New-Item `
            -ItemType Directory `
            -Path (Split-Path -Parent $Destination) `
            -Force | Out-Null
        Move-Item -LiteralPath $temporary -Destination $Destination
    }
}

$tracyCapstoneRoot = Join-Path `
    $toolchainsDirectory `
    "dependencies\capstone-6.0.0-Alpha5"
$tracyPpqsortRoot = Join-Path `
    $toolchainsDirectory `
    "dependencies\ppqsort-1.0.6"
$tracyZstdRoot = Join-Path `
    $toolchainsDirectory `
    "dependencies\zstd-1.5.7"
Install-PFTarDependency `
    -Component tracy-capstone-source `
    -Destination $tracyCapstoneRoot
Install-PFTarDependency `
    -Component tracy-ppqsort-source `
    -Destination $tracyPpqsortRoot
Install-PFTarDependency `
    -Component tracy-zstd-source `
    -Destination $tracyZstdRoot
if (-not (Test-Path -LiteralPath (
        Join-Path $tracyCapstoneRoot "include\capstone\capstone.h")) -or
    -not (Test-Path -LiteralPath (
        Join-Path $tracyPpqsortRoot "include\ppqsort.h")) -or
    -not (Test-Path -LiteralPath (
        Join-Path $tracyZstdRoot "lib\zstd.h")))
{
    Stop-PFToolchain "locked Tracy capture dependency source is incomplete"
}
$env:PF_TRACY_CAPSTONE_SOURCE_DIR = $tracyCapstoneRoot
$env:PF_TRACY_PPQSORT_SOURCE_DIR = $tracyPpqsortRoot
$env:PF_TRACY_ZSTD_SOURCE_DIR = $tracyZstdRoot
Write-Output "tracy-capture-dependencies=locked"

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
