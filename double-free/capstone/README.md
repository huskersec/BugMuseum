# Encore — capstone target (double-free module)

> ⚠️ **Intentionally vulnerable software — for research and study only.** Encore contains deliberately
> planted memory-safety bugs. Do not run it on production systems or expose it on a network, and do not
> reuse this code. Provided as-is, no warranty; the authors accept **no responsibility or liability for
> any misuse or damage.** The TCP listener binds loopback (`127.0.0.1`) only, by design.

Encore is a small **render/print spooler**. Clients `SUBMIT` a job (a document plus a target format),
get back a numeric **job id**, then `RENDER`/`APPEND`/`CANCEL` it by that id; the server keeps a job
table and a ready queue that a render pump drains. It is the applied exam for the
[double-free exhibit](../): the idioms you studied are hidden here inside genuine job bookkeeping, where
a job (or its buffer) can be released down more than one path — and sometimes is released twice.

This is the temporal sibling of [Revenant](../../use-after-free/capstone/). There the lesson was a
pointer outliving its object; here it is a *second* free reaching the same object — corrupting the
allocator's own freelist rather than user data.

## What Encore does

| Component | Kind | Role |
| --- | --- | --- |
| `spoolcore.dll` | DLL | the job store: a job table (id → job) + ready queue, submit/render/append/cancel/recycle/dup, a most-recent fast path, and the render pump |
| `filter_raster.dll` / `filter_text.dll` / `filter_pdfish.dll` | DLLs | per-job render filters, chosen at SUBMIT by format and loaded at runtime |
| `spoold.exe` | server | serves jobs over a named pipe (local) and TCP (loopback) |
| `spoolc.exe` | CLI | replays a recorded job-batch (`.enc`) file |

### Input channels (decide which can reach each finding)

- **local-file** — a job-batch handed to `spoolc`.
- **local-ipc** — requests over the named pipe `\\.\pipe\encore`.
- **remote-net** — requests over the TCP listener (`127.0.0.1:7200`).

Not every operation is offered on every channel: some are local-only conveniences, others are
server-side or network-only features, and one render format is offered only by the CLI. Part of the
exercise is working out **which channel can reach each bug**. The protocol references jobs by id; id
`0xFFFF` means "the most-recent job."

## Build

From **any PowerShell** (the script locates the VS toolchain itself):

```
powershell -ExecutionPolicy Bypass -File .\build.ps1                  # x64 (default)
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch x86        # 32-bit
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch both -Disasm
```

Configurations land under `build\<arch>\<config>\` (`od` / `o2` / `o2gs` / `o2gscf`). DLLs sit beside
the EXEs, so run from a config folder, e.g. `cd build\x64\o2gs`, then start `.\spoold.exe`, or
`.\spoolc.exe <batch.enc>`.

> **Double-free behaves differently from use-after-free.** A UAF is often silent; a double-free is often
> *caught*. The debug CRT asserts on it; the Windows segment heap / LFH may raise a heap-corruption
> fast-fail; and in a plain release build the second free can corrupt the freelist silently until a
> *later* allocation crashes. For a deterministic fault at the moment of the second free, enable Full
> Page Heap on the binary under test:
> ```
> gflags /p /enable spoold.exe /full      # disable later: gflags /p /disable spoold.exe
> ```
> Also note: `/GS` does nothing here (no stack buffers to guard); `/guard:cf` only bites the one place a
> reused object's filter pointer gets called.

## Your task

Audit Encore for double-free vulnerabilities. For each finding, produce:

1. **Location** — `file:line` and the function.
2. **The ownership story** — which object (a job? a document buffer? a metadata buffer?) is freed, and
   which *second* path frees it again (a cancel vs. a retire? a cache/alias? a `realloc`? a consuming
   filter? a shared member of a duplicated job?).
3. **Taint path** — from an input channel to the second free: the call chain and the request/op sequence
   that triggers it.
4. **Exposure** — which channel(s) reach it: `local-file`, `local-ipc`, `remote-net`, or several.
5. **Consequence & mitigation** — what does the second free corrupt, and what does each build (`/GS`,
   `/guard:cf`) and the runtime heap (debug CRT, segment heap, Page Heap) do or not do about it?

There is one correct free path, and it is maintained correctly — retiring a job by its id is a safe
no-op the second time. The bugs are in the paths that free *around* it. Work it from the source, or
reverse the built binaries.

*(No answer key ships with this exercise.)*
