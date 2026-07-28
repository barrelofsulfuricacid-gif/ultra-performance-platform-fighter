[CmdletBinding()]
param(
    [ValidateSet("commit", "milestone")]
    [string]$Mode = "commit",
    [string]$EvidenceDirectory,
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

[void](Find-PFHostTools `
    -RepositoryRoot $repositoryRoot `
    -ToolchainsDirectory $toolchainsDirectory `
    -PlatformKey $platformKey)
Enter-PFVisualStudio -PlatformKey $platformKey

$workflow = Join-Path $repositoryRoot "tools\workflow.ps1"
& $workflow `
    -Preset benchmark `
    -ToolchainsDirectory $toolchainsDirectory
if ($LASTEXITCODE -ne 0)
{
    Stop-PFToolchain "benchmark workflow failed"
}

$binary = Join-Path $repositoryRoot "build\benchmark\pf_benchmarks.exe"
$compileCommands = Join-Path `
    $repositoryRoot `
    "build\benchmark\compile_commands.json"
if (-not (Test-Path -LiteralPath $binary -PathType Leaf))
{
    Stop-PFToolchain "benchmark executable is missing"
}
if (-not (Test-Path -LiteralPath $compileCommands -PathType Leaf))
{
    Stop-PFToolchain "benchmark compile_commands.json is missing"
}

if ([string]::IsNullOrWhiteSpace($EvidenceDirectory))
{
    $EvidenceDirectory = Join-Path `
        $repositoryRoot `
        "performance\local\current"
}
$graphDirectory = Join-Path $repositoryRoot "performance\local\graphs"
$database = Join-Path `
    $repositoryRoot `
    "performance\local\performance.sqlite3"
$manifest = Join-Path $EvidenceDirectory "performance_manifest.txt"
New-Item `
    -ItemType Directory `
    -Path $EvidenceDirectory, $graphDirectory `
    -Force | Out-Null

$commit = (& git -C $repositoryRoot rev-parse HEAD).Trim()
$dirtyLines = @(& git -C $repositoryRoot status --porcelain)
$dirty = if ($dirtyLines.Count -eq 0) { "0" } else { "1" }

$entries = Get-Content -LiteralPath $compileCommands -Raw |
    ConvertFrom-Json
$simulationEntry = $entries |
    Where-Object { $_.file -match "[\\/]src[\\/]sim[\\/]sim\.c$" } |
    Select-Object -First 1
if ($null -eq $simulationEntry)
{
    Stop-PFToolchain (
        "could not resolve the canonical simulation compile command")
}
$compilerFlags = if (
    $simulationEntry.PSObject.Properties.Name -contains "command")
{
    [string]$simulationEntry.command
}
else
{
    [string]::Join(" ", @($simulationEntry.arguments))
}
$compilerFlags = $compilerFlags.Replace($repositoryRoot, "{ROOT}")

$compilerVersion = (& cl.exe 2>&1 | Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($compilerVersion))
{
    Stop-PFToolchain "could not read the MSVC compiler version"
}
$cpuName = if ([string]::IsNullOrWhiteSpace($env:PROCESSOR_IDENTIFIER))
{
    "unknown"
}
else
{
    $env:PROCESSOR_IDENTIFIER
}
$cpuFingerprint = (
    "$cpuName;logical_cpus=$([Environment]::ProcessorCount)")
$osFingerprint = (
    [System.Runtime.InteropServices.RuntimeInformation]::OSDescription +
    ";" +
    [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture)
$machineFingerprint = (
    "$platformKey;$cpuFingerprint;virtualization=unavailable")
$executableHash = (
    Get-FileHash -LiteralPath $binary -Algorithm SHA256
).Hash.ToLowerInvariant()

$env:PF_PERF_BUILD_CONFIGURATION = "benchmark-release"
$env:PF_PERF_COMMIT = $commit
$env:PF_PERF_COMPILER = $compilerVersion.Trim()
$env:PF_PERF_COMPILER_FLAGS = $compilerFlags
$env:PF_PERF_CPU_FINGERPRINT = $cpuFingerprint
$env:PF_PERF_DATABASE = $database
$env:PF_PERF_DIRTY = $dirty
$env:PF_PERF_EXECUTABLE_HASH = $executableHash
$env:PF_PERF_GRAPH_DIRECTORY = $graphDirectory
$env:PF_PERF_MACHINE_FINGERPRINT = $machineFingerprint
$env:PF_PERF_MANIFEST = $manifest
$env:PF_PERF_OS_FINGERPRINT = $osFingerprint
$env:PF_PERF_POWER_METADATA = if (
    [string]::IsNullOrWhiteSpace($env:PF_BENCH_POWER_METADATA))
{
    "unavailable:not-supplied"
}
else
{
    $env:PF_BENCH_POWER_METADATA
}
$env:PF_PERF_SCHEMA = Join-Path `
    $repositoryRoot `
    "performance\database\schema.sql"
$env:PF_PERF_THERMAL_METADATA = if (
    [string]::IsNullOrWhiteSpace($env:PF_BENCH_THERMAL_METADATA))
{
    "unavailable:not-supplied"
}
else
{
    $env:PF_BENCH_THERMAL_METADATA
}

& $binary --run $Mode
if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}
