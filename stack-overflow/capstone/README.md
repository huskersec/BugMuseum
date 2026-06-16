# Cascade — capstone target (stack-overflow module)

> ⚠️ **Intentionally vulnerable software — for research and study only.** Cascade contains
> deliberately planted memory-safety bugs. Do not run it on production systems or expose it on a
> network, and do not reuse this code. Provided as-is, no warranty; the authors accept **no
> responsibility or liability for any misuse or damage.** The network listener binds loopback
> (`127.0.0.1`) only, by design.

Cascade is a small, realistic Windows configuration-management suite. It exists so you can practice
the skill the [exhibit variants](../) can't teach: not *spotting an idiom in 3 lines*, but **tracing
whether untrusted input actually reaches a dangerous sink across a whole program — and through which
channel.** This is the applied exam for the stack-overflow module.

## What Cascade does

Applications store layered configuration **profiles** in a binary bundle format (`.cprof`):
length-prefixed entries of several types (string, wide string, blob, integer, and `${KEY}`
templates). The suite parses, merges, queries, and serves these profiles.

| Component | Kind | Role |
| --- | --- | --- |
| `cfgcore.dll` | DLL | the engine: parse `.cprof`, merge, look up keys, serve-time helpers |
| `provider_file.dll` / `provider_env.dll` / `provider_reg.dll` | DLLs | source providers (file / environment / registry), loaded at runtime |
| `cfgapply.exe` | CLI | load a `.cprof` file and print its entries |
| `regimport.exe` | CLI | import a profile from a provider (file/env/registry) |
| `cfgsvcd.exe` | service | serve config over a named pipe (local) and TCP (loopback) |

### Input channels (this is the point)

Untrusted data can enter Cascade through several distinct channels. Part of every finding is
deciding **which of these can reach it**:

- **local-file** — a `.cprof` file (or `argv`) handed to a CLI.
- **local-ipc** — a request from another process over the named pipe `\\.\pipe\cascade`.
- **remote-net** — a request over the TCP listener (`127.0.0.1:7420`).

## Build

From an *x64 Native Tools Command Prompt for VS*:

```
powershell -ExecutionPolicy Bypass -File .\build.ps1          # build all four configs
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Disasm  # also emit dumpbin disassembly
```

Four configurations are produced under `build\<config>\`:

| Config | Flags | What it shows |
| --- | --- | --- |
| `od` | `/Od /GS-` | the textbook shape, no mitigations |
| `o2` | `/O2 /GS-` | optimized, no stack cookie |
| `o2gs` | `/O2 /GS` | optimized, stack cookie |
| `o2gscf` | `/O2 /GS /guard:cf` | cookie + Control Flow Guard |

Run a frontend from its config folder (the DLLs sit beside the EXEs), e.g. `cd build\o2gs` then
`.\cfgapply <file.cprof>`, or start `.\cfgsvcd` and talk to it over the pipe/TCP.

## Your task

Audit Cascade for memory-safety vulnerabilities. For each finding, produce:

1. **Location** — `file:line` and the function.
2. **Class & idiom** — what kind of bug, and which exhibit variant it mirrors.
3. **Taint path** — from an input channel to the sink: the call chain, and the condition that makes
   it overflow (e.g. the input length/shape required).
4. **Exposure** — which channel(s) can reach it: `local-file`, `local-ipc`, `remote-net`, or several.
   A sink in a shared DLL is not automatically reachable from every frontend — prove it.
5. **Mitigation behavior** — does each build (`/GS`, `/guard:cf`) detect, prevent, or miss it, and why?

Two skills are being tested at once: recognizing the idiom (the variants taught that), and proving
**reachability** through the layers — parsers, a function-pointer dispatch table, and runtime-loaded
provider plugins. Work it from the source, or strip the source and reverse the built binaries cold.

*(No answer key ships with this exercise.)*
