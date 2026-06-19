# Exhibit: use-after-free — CWE-416 Use-After-Free

A different *kind* of bug from the overflow exhibits. Stack and heap overflows are **spatial** — you
write past the end of a buffer. A use-after-free is **temporal**: the pointer outlives the object.
Nothing goes out of bounds; instead a chunk is `free`d and then read, written, or called *after* its
lifetime has ended — by which point the allocator may have handed that memory to something else.

That shift is the whole point of this module: you stop reasoning about a *size* and start reasoning
about a *lifetime* — when an object dies, and every pointer that still refers to it. The bug is rarely
on one line; it's the relationship between a `free` and a later use, often far apart, often through a
*different* pointer.

> ⚠️ **Intentionally vulnerable code — for research and study only.** Don't run these on production
> systems or reuse the code. Provided as-is, no warranty; the authors accept **no responsibility or
> liability for any misuse or damage.**

## The grid

**Axis B (rows — temporal shape):** *how* the dangling pointer is created and used.
**Axis A (columns — build):** `/Od` debug · `/O2` release · `/O2 /GS` hardened.

| # | Variant | The shape | Sharpest binary tell |
| --- | --- | --- | --- |
| [01](variant-01-uaf-read/) | UAF read | freed, then read | a deref-**read** `[reg]` after that reg/slot was passed to `free` |
| [02](variant-02-uaf-write/) | UAF write | freed, then written | a deref-**write** `[reg]` after `free` — corrupts whatever reused the chunk |
| [03](variant-03-dangling-alias/) | dangling alias | freed via one pointer, used via another | `free` of one pointer, later use of a *second* pointer (global/field) holding the same address |
| [04](variant-04-stale-after-realloc/) | stale after realloc | `realloc` may move+free the old block; the old pointer is used | a use of the **input** pointer to `realloc` after the call |
| [05](variant-05-free-owner-use-member/) | free owner, use member | a destructor frees an owned sub-object; a saved pointer to it is used | `free` of a struct/child, then use of a pointer derived from it |
| [06](variant-06-free-on-error-path/) | free on a branch, use on the tail | the `free` looks like correct cleanup; a missing early-out lets the freed pointer reach the shared tail | `free` inside a branch, then a deref on the merged path |
| [07](variant-07-type-confusion-reuse/) | type-confusion reuse | free A, allocate B into the same chunk, call through the dangling A | `free` → `malloc` (same size) → **indirect call** `[reg+0]` through the stale pointer |

Read top-to-bottom: the bug is a *property of lifetime*, not a function. 03–06 are the "looks-correct"
shapes — the `free` is reasonable in isolation; the defect is that a pointer to the dead object is
still reachable. 07 is the exploitation bridge (the dangling pointer's reused bytes become a call
target).

## Files in each variant

| File | What it is |
| --- | --- |
| `src.c` | annotated source, with a `main` that takes input from `argv` so the bug is runnable |
| `notes.md` | root cause + per-build recognition signatures + UAF-specific behavior |
| `asm_O0.txt` / `asm_O2.txt` / `asm_O2_gs.txt` | MSVC `dumpbin /disasm:nobytes` of `vuln`, three builds (x64; `*_x86.txt` alongside for the 32-bit ABI) |
| `dec_O0.c` / `dec_O2.c` / `dec_O2_gs.c` | angr/pypcode pseudo-C for each build |

## What's different from the overflows (the lessons that are new here)

1. **There is no compile-time mitigation.** `/GS` guards stack buffers — irrelevant here, so the
   `/O2 /GS` build is identical to `/O2` (no cookie). `/guard:cf` does nothing for most of these; it
   bites **only** variant 07, where the dangling pointer's reused bytes become an indirect-call target
   and CFG rejects the non-valid destination. UAF's real defenses are *runtime* and *discipline*, not a
   compiler flag.
2. **The defense is runtime tooling — distinctly Windows here.** Enable Full Page Heap so freed
   allocations are placed on guard pages and never reused, turning a UAF into an immediate fault:
   ```
   gflags /p /enable vuln.exe /full      (or use Application Verifier)
   ```
   Without it, a UAF often **does not crash** — it silently returns stale or attacker-influenced bytes,
   which is worse than a crash. (Modern allocators add quarantine/delayed-free and LFH randomization,
   so reuse timing is non-deterministic.)
3. **Recognition is temporal, not size-based.** Spot a `free`/`HeapFree` and then track that pointer:
   is it read, written, or called afterward — directly, via an alias, or via a struct it owned? At
   `/Od` the pointer lives in a stack slot; at `/O2` in a non-volatile register across the `free`.
4. **The fix isn't bounds — it's ownership.** Null-after-free, clear single ownership, don't retain
   aliases past a free, and recheck pointers after `realloc`.

## Build & run

From **any PowerShell** at the museum root (the script locates the VS toolchain itself):

```
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe              # x64 (default)
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe -Arch both   # x64 + x86
```

This regenerates each variant's asm (`asm_*.txt` x64; `asm_*_x86.txt` x86) and builds runnable
executables to `use-after-free\variant-XX\bin\<arch>\<config>\vuln.exe`. Then, e.g.:

```
use-after-free\variant-01-uaf-read\bin\x64\od\vuln.exe AAAA
```

For a deterministic fault at the moment of the dangling access, run under Page Heap (above).

## Caveat: vocabulary vs grammar

Each variant is a small function — it teaches fast, local recognition of a temporal idiom. It can't
teach the *global* lifetime reasoning real UAF hunting demands (who frees, who still holds a pointer,
under what interleaving). See the root [README](../README.md#what-this-museum-teaches--and-what-it-cant).
The module's **capstone** — **Revenant**, a session/handle-manager server that hides these temporal
idioms across genuine session bookkeeping — lives in [capstone/](capstone/). Audit it from source or
reverse the built binaries; the withheld answer key stays out of the tree (`capstone/_solution/`,
gitignored).
