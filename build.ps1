<#
.SYNOPSIS
    Regenerate byte-exact MSVC assembly (and optionally decompilation) for every
    exhibit variant in the Bug Museum.

.DESCRIPTION
    For each directory containing a src.c, compiles three Axis-A builds and writes
    the disassembly of the `vuln` function to asm_O0.txt / asm_O2.txt / asm_O2_gs.txt:

        asm_O0.txt     cl /c /Od  /GS-   (debug, cookie off to show the raw shape)
        asm_O2.txt     cl /c /O2  /GS-   (release, no stack protector)
        asm_O2_gs.txt  cl /c /O2  /GS    (release, hardened — MSVC default)

    Note: MSVC enables /GS by DEFAULT, so the "no cookie" builds pass /GS- explicitly.

.PREREQUISITES
    Run from an "x64 Native Tools Command Prompt for VS" (so cl.exe and dumpbin.exe
    are on PATH and target x64), e.g.:
        powershell -ExecutionPolicy Bypass -File .\build.ps1

.PARAMETER Path
    Root to scan for variants. Defaults to the script's own directory (repo root).

.PARAMETER Decompile
    Also run tools\decompile.py (angr) to regenerate dec_*.c. Best-effort; requires
    python with angr installed. Off by default.
#>
[CmdletBinding()]
param(
    [string]$Path = $PSScriptRoot,
    [switch]$Decompile
)

$ErrorActionPreference = 'Stop'

function Require-Tool($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "$name not found on PATH. Open an 'x64 Native Tools Command Prompt for VS' and re-run."
    }
}
Require-Tool cl.exe
Require-Tool dumpbin.exe

# Extract the disassembly block for a single function from dumpbin /disasm output.
# dumpbin groups instructions under a symbol header line ("vuln:") and separates
# functions with blank lines; we capture from the header to the next blank line.
function Get-FuncDisasm([string[]]$lines, [string]$func) {
    $out = New-Object System.Collections.Generic.List[string]
    $in = $false
    foreach ($line in $lines) {
        if (-not $in) {
            if ($line -match "^_?$([regex]::Escape($func))\b" -or $line -match "\b$([regex]::Escape($func)):") {
                $in = $true
                $out.Add($line.TrimEnd())
            }
        } else {
            if ($line.Trim() -eq '') { break }
            $out.Add($line.TrimEnd())
        }
    }
    return $out
}

$builds = @(
    @{ Out = 'asm_O0.txt';    Flags = @('/c','/Od','/GS-');  Tag = 'cl /Od /GS-' },
    @{ Out = 'asm_O2.txt';    Flags = @('/c','/O2','/GS-');  Tag = 'cl /O2 /GS-' },
    @{ Out = 'asm_O2_gs.txt'; Flags = @('/c','/O2','/GS');   Tag = 'cl /O2 /GS'  }
)

$variants = Get-ChildItem -Path $Path -Recurse -Filter src.c |
            Where-Object { $_.FullName -notmatch '\\mingw-reference\\' }

if (-not $variants) { Write-Warning "No src.c found under $Path"; return }

foreach ($src in $variants) {
    $dir = $src.Directory.FullName
    Write-Host "== $($src.Directory.Name) ==" -ForegroundColor Cyan
    Push-Location $dir
    try {
        foreach ($b in $builds) {
            $obj = 'src.obj'
            if (Test-Path $obj) { Remove-Item $obj -Force }
            # Compile (stdout/stderr captured by the tool; non-zero exit means failure)
            & cl.exe @($b.Flags) /nologo /Fo:$obj src.c | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "cl failed for $($b.Tag) in $dir" }

            $disasm = & dumpbin.exe /nologo /disasm:nobytes $obj
            $func   = Get-FuncDisasm $disasm 'vuln'
            $header = "; $($b.Tag) /c src.c  ->  dumpbin /disasm:nobytes src.obj`n; Function: vuln`n"
            Set-Content -Path $b.Out -Value ($header + ($func -join "`n") + "`n") -Encoding utf8
            Write-Host "  wrote $($b.Out)"
            Remove-Item $obj -Force -ErrorAction SilentlyContinue
        }

        if ($Decompile) {
            $py = Join-Path $PSScriptRoot 'tools\decompile.py'
            if (Get-Command python -ErrorAction SilentlyContinue) {
                & python $py $dir
            } else {
                Write-Warning "  python not found; skipping decompilation for $dir"
            }
        }
    }
    finally { Pop-Location }
}

Write-Host "Done." -ForegroundColor Green
