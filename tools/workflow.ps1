[CmdletBinding()]
param(
    [ValidateSet(
        "debug",
        "sanitizer",
        "release",
        "profile",
        "benchmark",
        "headless",
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
