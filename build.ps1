<#
.SYNOPSIS
    Build everything in the Bug Museum: regenerate the canonical (x64) exhibit assembly,
    and compile every sub-project (capstone apps). Can target x64, x86, or both.

.DESCRIPTION
    Two passes:

    1. Exhibit variants — for each directory containing a src.c, compiles three Axis-A
       builds and writes the disassembly of the `vuln` function. asm is regenerated per
       architecture: x64 -> asm_O0/asm_O2/asm_O2_gs.txt (the canonical reference the notes
       describe), x86 -> the same names with an _x86 suffix. With -Exe, also compiles
       runnable executables to <variant>\bin\<arch>\<config>\vuln.exe.

    2. Sub-projects — every nested build.ps1 (each capstone) is invoked with the same -Arch.

    The script locates the Visual Studio toolchain itself (via vswhere) and sets up the
    requested architecture, so it can be run from ANY PowerShell — you do not need to pick
    the x86 vs x64 Native Tools prompt by hand.

.PARAMETER Arch
    x64 (default), x86, or both. Controls the toolchain, the asm produced (x64 -> asm_*.txt,
    x86 -> asm_*_x86.txt), and which executables are built.

.PARAMETER Path
    Root to scan for variants and sub-projects. Defaults to the script's own directory.

.PARAMETER Exe
    Also compile each variant into runnable executables under <variant>\bin\<arch>\<config>\.

.PARAMETER Disasm
    Passed through to sub-project build.ps1 scripts (capstones emit dumpbin disassembly).

.PARAMETER Decompile
    Also run tools\decompile.py (angr) to regenerate dec_*.c for variants (x64 only). Off by default.

.PARAMETER VariantsOnly
    Only do the exhibit-variant pass; skip compiling sub-projects.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build.ps1                 # x64 asm (+ capstones)
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe            # x64 asm + x64 exes
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch both -Exe # x64 (asm+exes) then x86 (exes)
#>
[CmdletBinding()]
param(
    [string]$Path = $PSScriptRoot,
    [ValidateSet('x64','x86','both')][string]$Arch = 'x64',
    [switch]$Exe,
    [switch]$Disasm,
    [switch]$Decompile,
    [switch]$VariantsOnly
)

$ErrorActionPreference = 'Stop'

trap {
    Write-Host ''
    Write-Host '==== BUILD ERROR =========================================' -ForegroundColor Red
    Write-Host ("  message  : {0}" -f $_.Exception.Message)               -ForegroundColor Red
    Write-Host ("  line #   : {0}" -f $_.InvocationInfo.ScriptLineNumber)  -ForegroundColor Red
    Write-Host ("  statement: {0}" -f $_.InvocationInfo.Line.Trim())       -ForegroundColor Red
    Write-Host ("  category : {0}" -f $_.CategoryInfo)                     -ForegroundColor Red
    Write-Host '==========================================================' -ForegroundColor Red
    break
}

# --- 'both' fans out into one isolated child process per arch (clean toolchain env each) ---
if ($Arch -eq 'both') {
    foreach ($a in @('x64', 'x86')) {
        Write-Host "==== building $a ====" -ForegroundColor Magenta
        $cargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-Arch', $a, '-Path', $Path)
        if ($Exe)          { $cargs += '-Exe' }
        if ($Disasm)       { $cargs += '-Disasm' }
        if ($Decompile)    { $cargs += '-Decompile' }
        if ($VariantsOnly) { $cargs += '-VariantsOnly' }
        & powershell.exe @cargs
        if ($LASTEXITCODE -ne 0) { throw "build failed for $a" }
    }
    Write-Host "Done (both)." -ForegroundColor Green
    return
}

# --- locate + enter the VS toolchain for $Arch (skips if already set up for it) ---
function Import-VsDevEnv([string]$TargetArch) {
    if ($env:VSCMD_ARG_TGT_ARCH -eq $TargetArch) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install Visual Studio, or run from a Native Tools prompt."
    }
    $vs = & $vswhere -latest -property installationPath
    if (-not $vs) { throw "No Visual Studio installation found by vswhere." }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvarsall.bat not found at $vcvars" }
    & cmd.exe /c "`"$vcvars`" $TargetArch >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($matches[1])" -Value $matches[2] }
    }
    if ($env:VSCMD_ARG_TGT_ARCH -ne $TargetArch) {
        throw "Failed to initialize the $TargetArch toolchain via vcvarsall."
    }
}
Import-VsDevEnv $Arch
Write-Host "[arch] toolchain target = $($env:VSCMD_ARG_TGT_ARCH)" -ForegroundColor DarkCyan

# Resolve to the actual application path so a stray alias/function can't shadow the tool.
function Resolve-Tool($name) {
    $cmd = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $cmd) { throw "$name not found after toolchain setup." }
    return $cmd.Source
}
$CL      = Resolve-Tool cl.exe
$DUMPBIN = Resolve-Tool dumpbin.exe

# Extract the disassembly block for a single function from dumpbin /disasm output.
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

$canonical = ($Arch -eq 'x64')   # dec_*.c is kept as the single x64-canonical decompilation set

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

            $dumpArgs = @('/nologo', '/disasm:nobytes', $obj)
            $asmText  = & $DUMPBIN @dumpArgs
            $func     = Get-FuncDisasm $asmText 'vuln'
            # x64 -> canonical asm_*.txt (matches the notes); x86 -> asm_*_x86.txt alongside it
            $asmOut = if ($Arch -eq 'x64') { $b.Out } else { $b.Out -replace '\.txt$', "_$Arch.txt" }
            $header = "; [$Arch] $($b.Tag) /c src.c  ->  dumpbin /disasm:nobytes src.obj`n; Function: vuln`n"
            Set-Content -Path $asmOut -Value ($header + ($func -join "`n") + "`n") -Encoding utf8
            Write-Host "  wrote $asmOut"
            Remove-Item $obj -Force -ErrorAction SilentlyContinue

            if ($Exe) {
                # bin/<arch>/<config>/vuln.exe — both arches coexist
                $cfgDir = Join-Path (Join-Path (Join-Path $dir 'bin') $Arch) $b.Cfg
                if (-not (Test-Path $cfgDir)) { New-Item -ItemType Directory -Force $cfgDir | Out-Null }
                $exeFlags = @($b.Flags | Where-Object { $_ -ne '/c' })   # drop /c so it links
                $exeArgs  = $exeFlags + @('/nologo', 'src.c', "/Fe:$cfgDir\vuln.exe", "/Fo:$cfgDir\")
                & $CL @exeArgs | Out-Null
                if ($LASTEXITCODE -ne 0) { throw "cl (exe) failed for $($b.Tag) in $dir" }
                Write-Host "  wrote bin\$Arch\$($b.Cfg)\vuln.exe"
            }
        }

        if ($Decompile -and $canonical) {
            $py = Join-Path $PSScriptRoot 'tools\decompile.py'
            if (Get-Command python -ErrorAction SilentlyContinue) { & python $py $dir }
            else { Write-Warning "  python not found; skipping decompilation for $dir" }
        }
    }
    finally { Pop-Location }
}

# --- pass 2: compile sub-projects (capstone apps) via their own build.ps1, same -Arch ---
if (-not $VariantsOnly) {
    $projects = Get-ChildItem -Path $Path -Recurse -Filter build.ps1 |
                Where-Object { $_.FullName -ne $PSCommandPath }
    foreach ($proj in $projects) {
        Write-Host "== project: $($proj.Directory.Name) ($Arch) ==" -ForegroundColor Cyan
        $splat = @{ Arch = $Arch }
        if ($Disasm) { $splat['Disasm'] = $true }
        & $proj.FullName @splat
        if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) {
            throw "sub-project build failed: $($proj.FullName)"
        }
    }
}

Write-Host "Done ($Arch)." -ForegroundColor Green
