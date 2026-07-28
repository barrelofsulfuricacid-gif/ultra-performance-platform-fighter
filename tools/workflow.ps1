[CmdletBinding()]
param(
    [ValidateSet(
        "debug",
        "sanitizer",
        "release",
        "profile",
        "benchmark",
        "headless",
        "verifier",
        "web")]
    [string]$Preset = "debug",
    [string]$ToolchainsDirectory
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
if ([string]::IsNullOrWhiteSpace($ToolchainsDirectory))
{
    $toolchainsDirectory = Join-Path $repositoryRoot ".toolchains"
}
else
{
    $toolchainsDirectory =
        $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
            $ToolchainsDirectory)
}
$env:PF_TOOLCHAINS_DIR = $toolchainsDirectory

$hostTools = Find-PFHostTools `
    -RepositoryRoot $repositoryRoot `
    -ToolchainsDirectory $toolchainsDirectory `
    -PlatformKey $platformKey

$sqlitePresets = @("debug", "sanitizer", "release", "profile", "benchmark")
if ($sqlitePresets -contains $Preset)
{
    $sqliteRoot = Join-Path `
        $toolchainsDirectory `
        "dependencies\sqlite-amalgamation-3530400"
    $sqliteSource = Join-Path $sqliteRoot "sqlite3.c"
    $sqliteHeader = Join-Path $sqliteRoot "sqlite3.h"
    if (-not (Test-Path -LiteralPath $sqliteSource -PathType Leaf) -or
        -not (Test-Path -LiteralPath $sqliteHeader -PathType Leaf))
    {
        Stop-PFToolchain (
            "pinned SQLite source is missing; run .\tools\bootstrap.ps1")
    }
    $env:PF_SQLITE_SOURCE_DIR = $sqliteRoot
}

if ($Preset -eq "profile")
{
    $tracyRoot = Join-Path `
        $toolchainsDirectory `
        "dependencies\tracy-0.13.1"
    $tracyHeader = Join-Path $tracyRoot "public\tracy\TracyC.h"
    $tracyClient = Join-Path $tracyRoot "public\TracyClient.cpp"
    if (-not (Test-Path -LiteralPath $tracyHeader -PathType Leaf) -or
        -not (Test-Path -LiteralPath $tracyClient -PathType Leaf))
    {
        Stop-PFToolchain (
            "pinned Tracy source is missing; run .\tools\bootstrap.ps1")
    }
    $env:PF_TRACY_SOURCE_DIR = $tracyRoot
}

$sdlPresets = @("debug", "sanitizer", "release", "profile")
if ($sdlPresets -contains $Preset)
{
    $sdlRoot = Join-Path $toolchainsDirectory "dependencies\SDL3-3.4.12"
    $sdlVersionHeader = Join-Path $sdlRoot "include\SDL3\SDL_version.h"
    if (-not (Test-Path -LiteralPath $sdlVersionHeader -PathType Leaf))
    {
        Stop-PFToolchain (
            "pinned SDL3 source is missing; run .\tools\bootstrap.ps1")
    }
    $env:PF_SDL_SOURCE_DIR = $sdlRoot
}

if ($Preset -eq "web")
{
    $emsdkCommit = "db04e88298d9916fc51fcd3743045ca3eb695127"
    $emsdkRoot = Join-Path $toolchainsDirectory "web\emsdk-$emsdkCommit"
    $emsdkEnvironment = Join-Path $emsdkRoot "emsdk_env.bat"
    if (-not (Test-Path -LiteralPath $emsdkEnvironment -PathType Leaf))
    {
        Stop-PFToolchain (
            "pinned Emscripten SDK is missing; run " +
            ".\tools\bootstrap.ps1 -Web")
    }

    Import-PFBatchEnvironment -BatchFile $emsdkEnvironment
    $env:EMSDK = $emsdkRoot
}
else
{
    Enter-PFVisualStudio -PlatformKey $platformKey
}

& $hostTools.CMake --workflow --preset $Preset
if ($LASTEXITCODE -ne 0)
{
    Stop-PFToolchain "CMake workflow '$Preset' failed"
}
