# Avalanche — capstone target (heap-overflow module)

> ⚠️ **Intentionally vulnerable software — for research and study only.** Avalanche contains
> deliberately planted memory-safety bugs. Do not run it on production systems or expose it on a
> network, and do not reuse this code. Provided as-is, no warranty; the authors accept **no
> responsibility or liability for any misuse or damage.** The TCP listener binds loopback
> (`127.0.0.1`) only, by design.

Avalanche is a small in-memory key→value cache server. It is the applied exam for the
[heap-overflow exhibit](../): the six idioms you studied as tiny functions are here hidden inside
genuine functionality, on the heap, where you must prove **reachability** — does untrusted input
actually reach a sink, and through which channel?

## What Avalanche does

Clients `SET`/`GET`/`DEL` keys whose values are arbitrary byte blobs; an operator can restore the
store from a snapshot file. Every entry and value is a heap allocation, and repeated same-size `SET`s
naturally groom the heap — so an overflow tends to land on an adjacent entry. That is the realistic
heap-exploitation setup, arising from ordinary use.

| Component | Kind | Role |
| --- | --- | --- |
| `cachecore.dll` | DLL | the store (hash table, inline small-value buffer), operations, serve-time helpers |
| `codec_raw.dll` / `codec_hex.dll` | DLLs | value codecs, selected per-request and loaded at runtime |
| `cached.exe` | server | serves the cache over a named pipe (local) and TCP (loopback) |
| `cacheload.exe` | CLI | restores the store from a snapshot (`.avl`) file |

### Input channels (decide which can reach each finding)

- **local-file** — a snapshot file handed to `cacheload`.
- **local-ipc** — a request from another process over the named pipe `\\.\pipe\avalanche`.
- **remote-net** — a request over the TCP listener (`127.0.0.1:7000`).

## Build

From **any PowerShell** (the script locates the VS toolchain itself):

```
powershell -ExecutionPolicy Bypass -File .\build.ps1                  # x64 (default)
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch x86        # 32-bit
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Arch both -Disasm
```

Configurations land under `build\<arch>\<config>\` (`od` / `o2` / `o2gs` / `o2gscf`). The DLLs sit
beside the EXEs, so run from a config folder, e.g. `cd build\x64\o2gs`, then start `.\cached.exe`, or
`.\cacheload.exe <snapshot.avl>`.

> **Heap overflows crash *later*, not at the write** — usually at a subsequent `free`/allocation, when
> the heap validates its metadata, or not at all. To force an immediate fault at the bad write while
> you practice, enable Full Page Heap for the binary under test:
> ```
> gflags /p /enable cached.exe /full      # disable later: gflags /p /disable cached.exe
> ```
> Also note: `/GS` does nothing here — there are no stack buffers to guard. That's a heap lesson, not
> a gap.

## Your task

Audit Avalanche for memory-safety vulnerabilities. For each finding, produce:

1. **Location** — `file:line` and the function.
2. **Class & idiom** — what kind of heap bug, and which exhibit variant it mirrors.
3. **Taint path** — from an input channel to the sink: the call chain (transport → dispatch → codec →
   store, etc.) and the input shape that triggers it.
4. **Exposure** — which channel(s) reach it: `local-file`, `local-ipc`, `remote-net`, or several. A
   sink in the shared DLL is not automatically reachable from every frontend — prove it.
5. **Consequence & mitigation** — what gets corrupted (adjacent entry? the entry's own pointers?
   allocator metadata?), and what each build (`/GS`, `/guard:cf`) does or doesn't do about it.

Work it from the source, or strip the source and reverse the built binaries. Either way, the heap is
the lesson: the bug fires quietly and the damage shows up somewhere else.

*(No answer key ships with this exercise.)*
