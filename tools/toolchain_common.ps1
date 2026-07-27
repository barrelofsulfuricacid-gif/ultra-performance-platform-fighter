Set-StrictMode -Version Latest

function Stop-PFToolchain
{
    param([Parameter(Mandatory = $true)][string]$Message)

    throw "toolchain error: $Message"
}

function Get-PFRepositoryRoot
{
    $repositoryRoot = (& git rev-parse --show-toplevel 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repositoryRoot))
    {
        Stop-PFToolchain "run this command from a Git checkout"
    }

    return $repositoryRoot.Trim()
}

function Get-PFPlatformKey
{
    $pfWindowsHost =
        [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
        [System.Runtime.InteropServices.OSPlatform]::Windows)
    if (-not $pfWindowsHost)
    {
        Stop-PFToolchain "tools/*.ps1 support Windows; use tools/*.sh on POSIX hosts"
    }

    $architecture =
        [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
    switch ($architecture.ToString())
    {
        "X64" { return "windows-x86_64" }
        "Arm64" { return "windows-arm64" }
        default
        {
            Stop-PFToolchain "unsupported Windows architecture: $architecture"
        }
    }
}

function Import-PFBatchEnvironment
{
    param(
        [Parameter(Mandatory = $true)][string]$BatchFile,
        [string]$Arguments = ""
    )

    if (-not (Test-Path -LiteralPath $BatchFile -PathType Leaf))
    {
        Stop-PFToolchain "batch environment script is missing: $BatchFile"
    }

    $quotedBatch = '"' + $BatchFile + '"'
    $command = "$quotedBatch $Arguments >nul && set"
    $environmentLines = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0)
    {
        Stop-PFToolchain "batch environment script failed: $BatchFile"
    }

    foreach ($line in $environmentLines)
    {
        $separator = $line.IndexOf("=")
        if ($separator -gt 0)
        {
            $name = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            [Environment]::SetEnvironmentVariable(
                $name,
                $value,
                [EnvironmentVariableTarget]::Process)
        }
    }
}

function Enter-PFVisualStudio
{
    param([Parameter(Mandatory = $true)][string]$PlatformKey)

    $programFilesX86 = [Environment]::GetFolderPath(
        [Environment+SpecialFolder]::ProgramFilesX86)
    $vswhere = Join-Path $programFilesX86 `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf))
    {
        Stop-PFToolchain (
            "Visual Studio Installer's vswhere.exe is required; " +
            "install Visual Studio with the MSVC 14.44 compatibility tools")
    }

    $installationPath = (
        & $vswhere `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
    )
    if ($LASTEXITCODE -ne 0 -or
        [string]::IsNullOrWhiteSpace($installationPath))
    {
        Stop-PFToolchain (
            "Visual Studio with the MSVC C++ tools was not found")
    }

    $vsDevCmd = Join-Path $installationPath.Trim() `
        "Common7\Tools\VsDevCmd.bat"
    switch ($PlatformKey)
    {
        "windows-x86_64"
        {
            $targetArchitecture = "x64"
            $hostArchitecture = "x64"
        }
        "windows-arm64"
        {
            $targetArchitecture = "arm64"
            $hostArchitecture = "arm64"
        }
        default
        {
            Stop-PFToolchain "no MSVC lane is defined for $PlatformKey"
        }
    }

    Import-PFBatchEnvironment `
        -BatchFile $vsDevCmd `
        -Arguments (
            "-no_logo -arch=$targetArchitecture " +
            "-host_arch=$hostArchitecture -vcvars_ver=14.44")

    $compilerText = (& cl.exe 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0)
    {
        Stop-PFToolchain "MSVC cl.exe did not start after VsDevCmd"
    }
    if ($compilerText -notmatch "Version 19\.44\.")
    {
        Stop-PFToolchain (
            "expected the pinned MSVC 19.44.x compatibility toolset; got " +
            ($compilerText -replace "\s+", " ").Trim())
    }

    $env:CC = "cl.exe"
    Write-Output (
        "compiler=" + (($compilerText -split "\r?\n")[0]).Trim())
}

function Find-PFHostTools
{
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ToolchainsDirectory,
        [Parameter(Mandatory = $true)][string]$PlatformKey
    )

    $hostRoot = Join-Path $ToolchainsDirectory "host\$PlatformKey"
    $cmakeRoot = Join-Path $hostRoot "cmake-4.4.0"
    $ninjaRoot = Join-Path $hostRoot "ninja-1.13.2"
    $cmake = Get-ChildItem `
        -LiteralPath $cmakeRoot `
        -Filter cmake.exe `
        -File `
        -Recurse `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $ninja = Get-ChildItem `
        -LiteralPath $ninjaRoot `
        -Filter ninja.exe `
        -File `
        -Recurse `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if ($null -eq $cmake)
    {
        Stop-PFToolchain "pinned CMake is missing; run .\tools\bootstrap.ps1"
    }
    if ($null -eq $ninja)
    {
        Stop-PFToolchain "pinned Ninja is missing; run .\tools\bootstrap.ps1"
    }

    $cmakeVersion = (& $cmake.FullName --version | Select-Object -First 1)
    if ($LASTEXITCODE -ne 0 -or $cmakeVersion -ne "cmake version 4.4.0")
    {
        Stop-PFToolchain "expected CMake 4.4.0; got $cmakeVersion"
    }

    $ninjaVersion = (& $ninja.FullName --version)
    if ($LASTEXITCODE -ne 0 -or $ninjaVersion.Trim() -ne "1.13.2")
    {
        Stop-PFToolchain "expected Ninja 1.13.2; got $ninjaVersion"
    }

    $env:PATH = (
        $cmake.DirectoryName + [IO.Path]::PathSeparator +
        $ninja.DirectoryName + [IO.Path]::PathSeparator +
        $env:PATH)

    return [PSCustomObject]@{
        CMake = $cmake.FullName
        Ninja = $ninja.FullName
    }
}
