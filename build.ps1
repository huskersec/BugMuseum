<#
.SYNOPSIS
    Build everything in the Bug Museum: regenerate byte-exact MSVC assembly for every
    exhibit variant, and compile every sub-project (capstone apps) via its own build.ps1.

.DESCRIPTION
    Two passes:

    1. Exhibit variants — for each directory containing a src.c, compiles three Axis-A
       builds and writes the disassembly of the `vuln` function:

           asm_O0.txt     cl /c /Od  /GS-   (debug, cookie off to show the raw shape)
           asm_O2.txt     cl /c /O2  /GS-   (release, no stack protector)
           asm_O2_gs.txt  cl /c /O2  /GS    (release, hardened — MSVC default)

       Note: MSVC enables /GS by DEFAULT, so the "no cookie" builds pass /GS- explicitly.

    2. Sub-projects — every nested build.ps1 (e.g. each capstone app) is invoked, so the
       full multi-file suites are compiled too. (Withheld answer-key build scripts use a
       different name and are not picked up.)

.PREREQUISITES
    Run from an "x64 Native Tools Command Prompt for VS" (so cl.exe and dumpbin.exe
    are on PATH and target x64), e.g.:
        powershell -ExecutionPolicy Bypass -File .\build.ps1

.PARAMETER Path
    Root to scan for variants and sub-projects. Defaults to the script's own directory.

.PARAMETER Decompile
    Also run tools\decompile.py (angr) to regenerate dec_*.c for variants. Best-effort;
    requires python with angr installed. Off by default.

.PARAMETER Disasm
    Passed through to sub-project build.ps1 scripts (capstones emit dumpbin disassembly).

.PARAMETER VariantsOnly
    Only regenerate exhibit-variant asm; skip compiling sub-projects.

.PARAMETER Exe
    Also compile each exhibit variant into runnable executables under <variant>\bin\
    (one per config) so the bug can be triggered/exploited, not just read.
#>
[CmdletBinding()]
param(
    [string]$Path = $PSScriptRoot,
    [switch]$Decompile,
    [switch]$Disasm,
    [switch]$VariantsOnly,
    [switch]$Exe
)

$ErrorActionPreference = 'Stop'

# Diagnostic trap: PowerShell reports parameter-binding cast failures at the outer
# call site (line 1), masking the real location. This runs in the scope where the
# error occurred, so InvocationInfo points at the true failing line/statement.
trap {
    Write-Host ''
    Write-Host '==== BUILD ERROR =========================================' -ForegroundColor Red
    Write-Host ("  message  : {0}" -f $_.Exception.Message)              -ForegroundColor Red
    Write-Host ("  line #   : {0}" -f $_.InvocationInfo.ScriptLineNumber) -ForegroundColor Red
    Write-Host ("  statement: {0}" -f $_.InvocationInfo.Line.Trim())      -ForegroundColor Red
    Write-Host ("  category : {0}" -f $_.CategoryInfo)                    -ForegroundColor Red
    Write-Host '==========================================================' -ForegroundColor Red
    break
}

# Resolve to the actual application path (CommandType Application), so a stray
# alias/function of the same name can't shadow the real compiler/dumper.
function Resolve-Tool($name) {
    $cmd = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cmd) {
        throw "$name not found on PATH. Open an 'x64 Native Tools Command Prompt for VS' and re-run."
    }
    return $cmd.Source
}
$CL      = Resolve-Tool cl.exe
$DUMPBIN = Resolve-Tool dumpbin.exe

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
    @{ Out = 'asm_O0.txt';    Cfg = 'od';   Flags = @('/c','/Od','/GS-');  Tag = 'cl /Od /GS-' },
    @{ Out = 'asm_O2.txt';    Cfg = 'o2';   Flags = @('/c','/O2','/GS-');  Tag = 'cl /O2 /GS-' },
    @{ Out = 'asm_O2_gs.txt'; Cfg = 'o2gs'; Flags = @('/c','/O2','/GS');   Tag = 'cl /O2 /GS'  }
)

$variants = Get-ChildItem -Path $Path -Recurse -Filter src.c |
            Where-Object { $_.FullName -notmatch '\\mingw-reference\\' }

if (-not $variants) { Write-Warning "No src.c found under $Path" }

foreach ($src in $variants) {
    $dir = $src.Directory.FullName
    Write-Host "== $($src.Directory.Name) ==" -ForegroundColor Cyan
    Push-Location $dir
    try {
        foreach ($b in $builds) {
            $obj = 'src.obj'
            if (Test-Path $obj) { Remove-Item $obj -Force }
            $clArgs = @($b.Flags) + @('/nologo', "/Fo:$obj", 'src.c')
            & $CL @clArgs | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "cl failed for $($b.Tag) in $dir" }

            # NB: don't name this $disasm — it collides (case-insensitively) with the
            # [switch]$Disasm parameter, whose type constraint would reject the array.
            $dumpArgs = @('/nologo', '/disasm:nobytes', $obj)
            $asmText  = & $DUMPBIN @dumpArgs
            $func     = Get-FuncDisasm $asmText 'vuln'
            $header = "; $($b.Tag) /c src.c  ->  dumpbin /disasm:nobytes src.obj`n; Function: vuln`n"
            Set-Content -Path $b.Out -Value ($header + ($func -join "`n") + "`n") -Encoding utf8
            Write-Host "  wrote $($b.Out)"
            Remove-Item $obj -Force -ErrorAction SilentlyContinue

            if ($Exe) {
                # Mirror the capstone's build/<config>/ layout: bin/<config>/vuln.exe
                $cfgDir = Join-Path (Join-Path $dir 'bin') $b.Cfg
                if (-not (Test-Path $cfgDir)) { New-Item -ItemType Directory -Force $cfgDir | Out-Null }
                $exeFlags = @($b.Flags | Where-Object { $_ -ne '/c' })   # drop /c so it links
                $exeArgs  = $exeFlags + @('/nologo', 'src.c', "/Fe:$cfgDir\vuln.exe", "/Fo:$cfgDir\")
                & $CL @exeArgs | Out-Null
                if ($LASTEXITCODE -ne 0) { throw "cl (exe) failed for $($b.Tag) in $dir" }
                Write-Host "  wrote bin\$($b.Cfg)\vuln.exe"
            }
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

# --- pass 2: compile sub-projects (capstone apps, etc.) via their own build.ps1 ---
if (-not $VariantsOnly) {
    $projects = Get-ChildItem -Path $Path -Recurse -Filter build.ps1 |
                Where-Object { $_.FullName -ne $PSCommandPath }
    foreach ($proj in $projects) {
        Write-Host "== project: $($proj.Directory.Name) ==" -ForegroundColor Cyan
        $splat = @{}
        if ($Disasm) { $splat['Disasm'] = $true }
        & $proj.FullName @splat
        if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) {
            throw "sub-project build failed: $($proj.FullName)"
        }
    }
}

Write-Host "Done." -ForegroundColor Green
