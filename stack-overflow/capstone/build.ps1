<#
.SYNOPSIS
    Build the Cascade configuration suite (cfgcore.dll, the provider plugins, and
    the three frontends) across the museum's build matrix.

.DESCRIPTION
    Produces four build configurations, each in its own output folder:

        od       cl /Od /GS-            debug, no stack cookie
        o2       cl /O2 /GS-            release, no stack cookie
        o2gs     cl /O2 /GS             release, stack cookie
        o2gscf   cl /O2 /GS /guard:cf   release, cookie + Control Flow Guard

    All configs build with the shared dynamic CRT (/MD), C11, and
    _CRT_SECURE_NO_WARNINGS (this code uses classic CRT string routines on
    purpose). /sdl and /WX are intentionally NOT used.

.PARAMETER Disasm
    After building, also emit `dumpbin /disasm:nobytes` next to each binary
    (<name>.asm). Reading the disassembly is part of the exercise; it is not
    shipped pre-generated.

.PREREQUISITES
    Run from an "x64 Native Tools Command Prompt for VS":
        powershell -ExecutionPolicy Bypass -File .\build.ps1 [-Disasm]
#>
[CmdletBinding()]
param(
    [switch]$Disasm,
    [string]$OutRoot = "$PSScriptRoot\build"
)

$ErrorActionPreference = 'Stop'

function Need($tool) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found on PATH. Open an 'x64 Native Tools Command Prompt for VS' and re-run."
    }
}
Need cl.exe
if ($Disasm) { Need dumpbin.exe }

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

$bins = @('cfgcore.dll','provider_file.dll','provider_env.dll','provider_reg.dll',
          'cfgapply.exe','cfgsvcd.exe','regimport.exe')

foreach ($c in $configs) {
    $dir = Join-Path $OutRoot $c.name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $cf   = $common + $c.flags
    $fo   = "/Fo:$dir\"
    $core = "$dir\cfgcore.lib"

    # Core engine (DLL) — build first; everything else imports it.
    Run-Cl ($cf + @('/LD','/DCFGCORE_EXPORTS', "$src\cfgcore\cfgcore.c",
                    "/Fe:$dir\cfgcore.dll", $fo, '/link', "/IMPLIB:$core"))

    # Provider plugins (DLLs).
    Run-Cl ($cf + @('/LD','/DPROVIDER_EXPORTS', "$src\providers\provider_file.c",
                    "/Fe:$dir\provider_file.dll", $fo, $core, '/link', "/IMPLIB:$dir\provider_file.lib"))
    Run-Cl ($cf + @('/LD','/DPROVIDER_EXPORTS', "$src\providers\provider_env.c",
                    "/Fe:$dir\provider_env.dll", $fo, $core, '/link', "/IMPLIB:$dir\provider_env.lib"))
    Run-Cl ($cf + @('/LD','/DPROVIDER_EXPORTS', "$src\providers\provider_reg.c",
                    "/Fe:$dir\provider_reg.dll", $fo, $core, 'advapi32.lib', '/link', "/IMPLIB:$dir\provider_reg.lib"))

    # Frontends (EXEs).
    Run-Cl ($cf + @("$src\cfgapply\cfgapply.c",   "/Fe:$dir\cfgapply.exe",  $fo, $core))
    Run-Cl ($cf + @("$src\cfgsvcd\cfgsvcd.c",     "/Fe:$dir\cfgsvcd.exe",   $fo, $core, 'ws2_32.lib'))
    Run-Cl ($cf + @("$src\regimport\regimport.c", "/Fe:$dir\regimport.exe", $fo, $core))

    Write-Host "built $($c.name)  ->  $dir" -ForegroundColor Green

    if ($Disasm) {
        foreach ($bin in $bins) {
            $b = Join-Path $dir $bin
            if (Test-Path $b) { & dumpbin.exe /nologo /disasm:nobytes $b | Set-Content "$dir\$bin.asm" }
        }
    }
}

Write-Host "done. binaries under $OutRoot\<config>\" -ForegroundColor Cyan
