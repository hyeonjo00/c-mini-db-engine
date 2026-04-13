$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $root "build/c-mini-db-engine.exe"

if (-not (Test-Path $binary)) {
    & (Join-Path $root "build.ps1") -Target cli
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Get-Content (Join-Path $PSScriptRoot "smoke_commands.txt") | & $binary
exit $LASTEXITCODE
