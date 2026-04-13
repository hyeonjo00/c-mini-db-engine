param(
    [ValidateSet("studio", "cli")]
    [string]$Target = "studio",
    [string]$Compiler = "",
    [string]$RaylibDir = "",
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build"
$includeDir = Join-Path $root "include"

function Resolve-CCompiler {
    param([string]$Requested)

    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        return $Requested
    }

    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    $clang = Get-Command clang -ErrorAction SilentlyContinue

    if ($gcc) {
        return $gcc.Source
    }
    if ($clang) {
        return $clang.Source
    }

    throw "No C compiler was found on PATH. Install GCC (MinGW-w64) or pass -Compiler <path>."
}

function Resolve-RaylibDir {
    param([string]$Requested)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        $candidates += $Requested
    }
    if (-not [string]::IsNullOrWhiteSpace($env:RAYLIB_DIR)) {
        $candidates += $env:RAYLIB_DIR
    }

    $candidates += (Join-Path $root "deps\\raylib")
    $candidates += (Join-Path $root "external\\raylib")

    foreach ($candidate in $candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) {
        $includeCandidate = Join-Path $candidate "include\\raylib.h"
        $libCandidateA = Join-Path $candidate "lib\\libraylib.a"
        $libCandidateB = Join-Path $candidate "lib\\raylib.lib"
        if ((Test-Path $includeCandidate) -and ((Test-Path $libCandidateA) -or (Test-Path $libCandidateB))) {
            return $candidate
        }
    }

    throw "Raylib was not found. Set -RaylibDir or RAYLIB_DIR to a folder containing include\\raylib.h and lib\\libraylib.a (or raylib.lib)."
}

function Get-SourceList {
    param([string]$SelectedTarget)

    $sources = Get-ChildItem (Join-Path $root "src\\*.c") | Sort-Object Name
    switch ($SelectedTarget) {
        "studio" {
            return $sources | Where-Object { $_.Name -ne "main.c" -and $_.Name -ne "ui.c" }
        }
        "cli" {
            return $sources | Where-Object { $_.Name -ne "studio_main.c" -and $_.Name -ne "studio_app.c" }
        }
        default {
            throw "Unsupported target: $SelectedTarget"
        }
    }
}

$Compiler = Resolve-CCompiler -Requested $Compiler
New-Item -ItemType Directory -Force $buildDir | Out-Null

$output = if ($Target -eq "studio") {
    Join-Path $buildDir "c-mini-db-studio.exe"
} else {
    Join-Path $buildDir "c-mini-db-engine.exe"
}

$sources = Get-SourceList -SelectedTarget $Target | ForEach-Object { $_.FullName }
$arguments = @(
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-I", $includeDir
) + $sources

if ($Target -eq "studio") {
    $resolvedRaylib = Resolve-RaylibDir -Requested $RaylibDir
    $raylibInclude = Join-Path $resolvedRaylib "include"
    $raylibLib = Join-Path $resolvedRaylib "lib"

    $arguments += @(
        "-I", $raylibInclude,
        "-L", $raylibLib,
        "-o", $output,
        "-lraylib",
        "-lopengl32",
        "-lgdi32",
        "-lwinmm",
        "-lshell32",
        "-mwindows"
    )
} else {
    $arguments += @("-o", $output)
}

& $Compiler @arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built $output"

if ($Run) {
    & $output
    exit $LASTEXITCODE
}
 