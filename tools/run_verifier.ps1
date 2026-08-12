[CmdletBinding()]
param(
    [string]$Commit,
    [string]$OutputDirectory,
    [string]$IssueDirectory
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = (& git rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repositoryRoot))
{
    throw "verifier error: run this command from a Git checkout"
}
$repositoryRoot = $repositoryRoot.Trim()

if ([string]::IsNullOrWhiteSpace($Commit))
{
    $Commit = (& git -C $repositoryRoot rev-parse HEAD).Trim()
}
if ($Commit -cnotmatch "^[0-9a-f]{40}$")
{
    throw "verifier error: commit must be a 40-character lowercase hash"
}
& git -C $repositoryRoot cat-file -e "$Commit`^{commit}"
if ($LASTEXITCODE -ne 0)
{
    throw "verifier error: commit does not exist: $Commit"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    $OutputDirectory = Join-Path `
        $repositoryRoot `
        "performance\local\commits\$Commit\verifier"
}
if ([string]::IsNullOrWhiteSpace($IssueDirectory))
{
    $IssueDirectory = Join-Path $repositoryRoot "verifier\issues\unfixed"
}

$checkDirectory = Join-Path $OutputDirectory "checks"
$diffPath = Join-Path $OutputDirectory "commit_files.txt"
$externalPath = Join-Path $OutputDirectory "external_checks.tsv"
$acceptancePath = Join-Path $repositoryRoot "verifier\acceptance_manifest.tsv"
$contentHash =
    "1728394a5b6c7d8e9fb0c1d2e3f405162738495a6b7c8d9eafc0d1e2f3041526"

New-Item -ItemType Directory -Force -Path `
    $OutputDirectory, $checkDirectory, $IssueDirectory | Out-Null

$diffLines = @(
    & git -C $repositoryRoot `
        diff-tree --root --no-commit-id --name-only -r $Commit
)
if ($LASTEXITCODE -ne 0)
{
    throw "verifier error: could not read commit diff"
}
[System.IO.File]::WriteAllLines(
    $diffPath,
    $diffLines,
    [System.Text.UTF8Encoding]::new($false))

$external = [System.Collections.Generic.List[string]]::new()
$external.Add("check`tstatus`tevidence")

function Add-PFExternal
{
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [ValidateSet("pass", "fail", "deferred")]
        [string]$Status,
        [Parameter(Mandatory)]
        [string]$Evidence
    )

    $external.Add("$Name`t$Status`t$Evidence")
}

& (Join-Path $repositoryRoot "tools\workflow.ps1") verifier
if ($LASTEXITCODE -ne 0)
{
    throw "verifier error: isolated verifier workflow failed"
}

$verifier = Join-Path $repositoryRoot "build\verifier\pf_verifier.exe"
if (-not (Test-Path -LiteralPath $verifier -PathType Leaf))
{
    throw "verifier error: verifier executable is missing"
}

$performanceLog = Join-Path $checkDirectory "m3-performance.log"
try
{
    & (Join-Path $repositoryRoot "tools\run_performance.ps1") `
        -Mode commit `
        -EvidenceDirectory (Join-Path $checkDirectory "m3_performance") `
        *>&1 |
        Set-Content -LiteralPath $performanceLog -Encoding utf8
    if ($LASTEXITCODE -ne 0)
    {
        throw "Windows performance workflow failed"
    }
    Add-PFExternal `
        -Name "m3-performance" `
        -Status "pass" `
        -Evidence "performance/local/commits/$Commit/verifier/checks/m3-performance.log"
}
catch
{
    $_ | Out-String |
        Set-Content -LiteralPath $performanceLog -Encoding utf8
    Add-PFExternal `
        -Name "m3-performance" `
        -Status "fail" `
        -Evidence "performance/local/commits/$Commit/verifier/checks/m3-performance.log"
}

foreach ($check in @(
        "m0-contract",
        "m1-foundation",
        "m1-workflow",
        "m1-setup",
        "m2-kernel",
        "m2-replay",
        "mechanical-oracle",
        "m4-item",
        "m4-projectile",
        "m4-reflector",
        "m3-regression-qualification",
        "sanitizer",
        "browser-smoke",
        "browser-runtime"))
{
    Add-PFExternal `
        -Name $check `
        -Status "deferred" `
        -Evidence "POSIX qualification or dedicated clean-machine CI lane"
}

foreach ($check in @(
        "browser-collision-interaction",
        "browser-match-flow-interaction",
        "browser-replay-interaction"))
{
    Add-PFExternal `
        -Name $check `
        -Status "deferred" `
        -Evidence (
            "Owner interaction gate; generated-page smoke covers " +
            "initial runtime state only")
}

[System.IO.File]::WriteAllLines(
    $externalPath,
    $external,
    [System.Text.UTF8Encoding]::new($false))

$buildHash = (
    Get-FileHash -LiteralPath $verifier -Algorithm SHA256
).Hash.ToLowerInvariant()
& $verifier `
    --verify `
    $acceptancePath `
    $diffPath `
    $externalPath `
    $OutputDirectory `
    $IssueDirectory `
    $Commit `
    $buildHash `
    $contentHash
if ($LASTEXITCODE -ne 0)
{
    throw "verifier workflow failed; inspect pass_manifest.md and issue records"
}

Write-Output (
    "verifier-workflow=pass commit=$Commit manifest=" +
    (Join-Path $OutputDirectory "pass_manifest.md"))
