<#
.SYNOPSIS
    Build the Encore spooler suite (spoolcore.dll, the filter plugins, and the two
    frontends). Can target x64, x86, or both.

.DESCRIPTION
    Four configurations per architecture, each under build\<arch>\<config>\:

        od       cl /Od /GS-            debug, no stack cookie
        o2       cl /O2 /GS-            release, no stack cookie
        o2gs     cl /O2 /GS             release, stack cookie
        o2gscf   cl /O2 /GS /guard:cf   release, cookie + Control Flow Guard

    All configs build with the shared dynamic CRT (/MD), C11, and _CRT_SECURE_NO_WARNINGS.
    The script locates the VS toolchain itself (vswhere), so run it from any PowerShell.

    Note: /GS is moot (double-free isn't a stack-buffer overflow). /guard:cf matters for
    the op-dispatch table, the GetProcAddress filter dispatch, and the hijacked indirect
    call when a reused job's filter pointer is invoked.

.PARAMETER Arch
    x64 (default), x86, or both.

.PARAMETER Disasm
    After building, also emit `dumpbin /disasm:nobytes` next to each binary.
#>
[CmdletBinding()]
param(
    [ValidateSet('x64','x86','both')][string]$Arch = 'x64',
    [switch]$Disasm,
    [string]$OutRoot = "$PSScriptRoot\build"
)

$ErrorActionPreference = 'Stop'

if ($Arch -eq 'both') {
    foreach ($a in @('x64', 'x86')) {
        Write-Host "==== Encore: $a ====" -ForegroundColor Magenta
        $cargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-Arch', $a)
        if ($Disasm) { $cargs += '-Disasm' }
        & powershell.exe @cargs
        if ($LASTEXITCODE -ne 0) { throw "Encore build failed for $a" }
    }
    return
}

function Import-VsDevEnv([string]$TargetArch) {
    if ($env:VSCMD_ARG_TGT_ARCH -eq $TargetArch) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Install Visual Studio or run from a Native Tools prompt." }
    $vs = & $vswhere -latest -property installationPath
    if (-not $vs) { throw "No Visual Studio installation found by vswhere." }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvarsall.bat not found at $vcvars" }
    & cmd.exe /c "`"$vcvars`" $TargetArch >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($matches[1])" -Value $matches[2] }
    }
    if ($env:VSCMD_ARG_TGT_ARCH -ne $TargetArch) { throw "Failed to initialize the $TargetArch toolchain." }
}
Import-VsDevEnv $Arch

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw "cl.exe not found after toolchain setup." }
if ($Disasm -and -not (Get-Command dumpbin.exe -ErrorAction SilentlyContinue)) { throw "dumpbin.exe not found." }

$src    = "$PSScriptRoot\src"
$common = @('/nologo', '/MD', '/std:c11', '/D_CRT_SECURE_NO_WARNINGS', '/W3')

$configs = @(
    @{ name = 'od';     flags = @('/Od', '/GS-') },
    @{ name = 'o2';     flags = @('/O2', '/GS-') },
    @{ name = 'o2gs';   flags = @('/O2', '/GS') },
    @{ name = 'o2gscf'; flags = @('/O2', '/GS', '/guard:cf') }
)

function Run-Cl([string[]]$arglist) {
    $output = & cl.exe @arglist 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "---- cl output -------------------------------------------" -ForegroundColor DarkYellow
        $output | ForEach-Object { Write-Host $_ }
        Write-Host "----------------------------------------------------------" -ForegroundColor DarkYellow
        throw "cl failed (exit $LASTEXITCODE):`n  cl $($arglist -join ' ')"
    }
}

$bins = @('spoolcore.dll','filter_raster.dll','filter_text.dll','filter_pdfish.dll','spoold.exe','spoolc.exe')

foreach ($c in $configs) {
    $dir = Join-Path (Join-Path $OutRoot $Arch) $c.name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $cf   = $common + $c.flags
    $fo   = "/Fo:$dir\"
    $core = "$dir\spoolcore.lib"

    # Core store (DLL) — build first; the frontends import it.
    Run-Cl ($cf + @('/LD','/DSPOOLCORE_EXPORTS', "$src\spoolcore\spoolcore.c",
                    "/Fe:$dir\spoolcore.dll", $fo, '/link', "/IMPLIB:$core"))

    # Filter plugins (standalone DLLs, loaded at runtime).
    foreach ($flt in @('filter_raster','filter_text','filter_pdfish')) {
        Run-Cl ($cf + @('/LD','/DFILTER_EXPORTS', "$src\filters\$flt.c",
                        "/Fe:$dir\$flt.dll", $fo, '/link', "/IMPLIB:$dir\$flt.lib"))
    }

    # Frontends (EXEs).
    Run-Cl ($cf + @("$src\spoold\spoold.c", "/Fe:$dir\spoold.exe", $fo, $core, 'ws2_32.lib'))
    Run-Cl ($cf + @("$src\spoolc\spoolc.c", "/Fe:$dir\spoolc.exe", $fo, $core))

    Write-Host "built $Arch/$($c.name)  ->  $dir" -ForegroundColor Green

    if ($Disasm) {
        foreach ($bin in $bins) {
            $b = Join-Path $dir $bin
            if (Test-Path $b) { & dumpbin.exe /nologo /disasm:nobytes $b | Set-Content "$dir\$bin.asm" }
        }
    }
}

Write-Host "done ($Arch). binaries under $OutRoot\$Arch\<config>\" -ForegroundColor Cyan
