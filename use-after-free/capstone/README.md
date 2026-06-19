# Revenant — capstone target (use-after-free module)

> ⚠️ **Intentionally vulnerable software — for research and study only.** Revenant contains
> deliberately planted memory-safety bugs. Do not run it on production systems or expose it on a
> network, and do not reuse this code. Provided as-is, no warranty; the authors accept **no
> responsibility or liability for any misuse or damage.** The TCP listener binds loopback
> (`127.0.0.1`) only, by design.

Revenant is a small **session server**. Clients `OPEN` a session (a heap object with an owned buffer
and a command handler), get back a numeric **handle**, reference the session by that handle on later
`USE`/`STAT` calls, and `CLOSE` it. It is the applied exam for the [use-after-free exhibit](../): the
temporal idioms you studied are hidden here inside genuine session bookkeeping, where a freed object
can still be reachable through a handle, a cache, a queued operation, or a stale pointer.

## What Revenant does

| Component | Kind | Role |
| --- | --- | --- |
| `sesscore.dll` | DLL | the session store: a handle table (id → object), OPEN/USE/CLOSE/STAT, a most-recent fast path, a deferred-write batch |
| `handler_echo.dll` / `handler_calc.dll` | DLLs | per-session command handlers, chosen at OPEN and loaded at runtime |
| `sessiond.exe` | server | serves sessions over a named pipe (local) and TCP (loopback) |
| `sessreplay.exe` | CLI | replays a recorded op-log (`.rvn`) file |

### Input channels (decide which can reach each finding)

- **local-file** — an op-log handed to `sessreplay`.
- **local-ipc** — requests over the named pipe `\\.\pipe\revenant`.
- **remote-net** — requests over the TCP listener (`127.0.0.1:7100`).

The protocol references sessions by handle; the handle `0xFFFF` means "the most-recent session."

## Build

From **any PowerShell** (the script locates the VS toolchain itself):

```
powershell -ExecutionPolicy Bypass -File .\build.ps1                  # x64 (default)
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch x86        # 32-bit
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch both -Disasm
```

Configurations land under `build\<arch>\<config>\` (`od` / `o2` / `o2gs` / `o2gscf`). DLLs sit beside
the EXEs, so run from a config folder, e.g. `cd build\x64\o2gs`, then start `.\sessiond.exe`, or
`.\sessreplay.exe <oplog.rvn>`.

> **UAF crashes are non-deterministic** — a freed session often still holds its old bytes, so a
> dangling use can *silently succeed* with stale data (worse than a crash). To force an immediate
> fault at the dangling access while you practice, enable Full Page Heap on the binary under test:
> ```
> gflags /p /enable sessiond.exe /full      # disable later: gflags /p /disable sessiond.exe
> ```
> Also note: `/GS` does nothing here (no stack buffers to guard); `/guard:cf` only bites the one place
> a freed object's handler pointer gets called.

## Your task

Audit Revenant for use-after-free vulnerabilities. For each finding, produce:

1. **Location** — `file:line` and the function.
2. **The lifetime story** — where the object is freed, and how a pointer to it is still reachable
   afterward (a handle? a cache? a queued op? a saved pointer across `realloc`? a connection's cached
   pointer?).
3. **Taint path** — from an input channel to the dangling use: the call chain and the request/op
   sequence that triggers it.
4. **Exposure** — which channel(s) reach it: `local-file`, `local-ipc`, `remote-net`, or several.
5. **Consequence & mitigation** — read, write, or call through freed memory? What does each build
   (`/GS`, `/guard:cf`) do or not do about it?

The obvious handle path is maintained correctly — CLOSE deregisters cleanly. The bugs are in the
references nobody invalidated. Work it from the source, or reverse the built binaries.

*(No answer key ships with this exercise.)*
