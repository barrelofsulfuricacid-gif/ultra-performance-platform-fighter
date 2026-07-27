[CmdletBinding()]
param([ValidateRange(1, 65535)][int]$Port = 8000)

$ErrorActionPreference = "Stop"
$repositoryRoot = (& git rev-parse --show-toplevel).Trim()
$webRoot = Join-Path $repositoryRoot "build\web"
$entryPoint = Join-Path $webRoot "web_client.html"

if (-not (Test-Path -LiteralPath $entryPoint -PathType Leaf))
{
    throw "web smoke is missing; run .\tools\workflow.ps1 web first"
}
if ($null -eq (Get-Command python.exe -ErrorAction SilentlyContinue))
{
    throw "Python 3 is required to serve the browser smoke"
}

Write-Output "serving=http://127.0.0.1:$Port/web_client.html"
& python.exe -m http.server `
    $Port `
    --bind 127.0.0.1 `
    --directory $webRoot
if ($LASTEXITCODE -ne 0)
{
    throw "the browser smoke server exited with code $LASTEXITCODE"
}
