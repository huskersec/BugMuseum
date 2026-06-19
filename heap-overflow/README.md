# Exhibit: heap-overflow — CWE-122 Heap-based Buffer Overflow

The same *property* as the [stack-overflow](../stack-overflow/) exhibit — an unbounded (or
wrongly-bounded) write past the end of a fixed buffer — but the buffer now lives on the **heap**.
What the overflow corrupts changes: instead of saved RBP / the return address, it walks forward
over **adjacent heap allocations and the allocator's metadata**. Study the same six idioms in this
new memory region and learn what's different about recognizing and mitigating them.

> ⚠️ **Intentionally vulnerable code — for research and study only.** Don't run these on production
> systems or reuse the code. Provided as-is, no warranty; the authors accept **no responsibility or
> liability for any misuse or damage.**

## The grid

**Axis B (rows — source idiom):** how the bug is written. **Axis A (columns — build):** `/Od` debug ·
`/O2` release · `/O2 /GS` hardened.

| # | Variant | The disguise | Sharpest binary tell |
| --- | --- | --- | --- |
| [01](variant-01-strcpy/) | `strcpy(buf, input)` into `malloc` buffer | none — the baseline | dest of the unbounded copy traces to a `call malloc`, not a stack `lea` |
| [02](variant-02-sprintf/) | `sprintf(buf, "%s", input)` | "formatted output" feels deliberate | printf-family call into a heap pointer, format is `"%s"` |
| [03](variant-03-memcpy-attacker-len/) | `memcpy(buf, input, len)` | it *has* a size argument | size register traces to a parameter; no `cmp` against the alloc size |
| [04](variant-04-strncpy-srclen/) | `strncpy(buf, input, strlen(input))` | the "safe" `strncpy` | count is `strlen(source)`; the alloc size never appears |
| [05](variant-05-alloc-size-mismatch/) | `malloc(strlen(input))` then `strcpy` | it *sizes the allocation to the input* | `malloc` arg is `strlen(...)` (no `+1`) feeding a copy that writes the NUL too |
| [06](variant-06-off-by-one-loop/) | manual loop, `i <= n` into heap buffer | no dangerous function at all | `cmp i,N` + `jbe` (≤) not `jb` (<); at `/O2`, a trailing `+1` store past the block |

Read top-to-bottom: the bug is a *property*, not a function name — and it's the same property you
met on the stack, just relocated. Variants 03–05 are the "looks-correct" shapes; the defect lives in
a size's *provenance* (03), *units/operand* (04), or the *allocation arithmetic* (05).

## Files in each variant

| File | What it is |
| --- | --- |
| `src.c` | annotated source, with a `main` that takes input from `argv` so the bug is runnable |
| `notes.md` | root cause + per-build recognition signatures + heap-specific behavior |
| `asm_O0.txt` / `asm_O2.txt` / `asm_O2_gs.txt` | MSVC `dumpbin /disasm:nobytes` of `vuln`, three builds (x64; `*_x86.txt` sit alongside for the 32-bit ABI) |
| `dec_O0.c` / `dec_O2.c` / `dec_O2_gs.c` | angr/pypcode pseudo-C for each build |

## What's different from the stack (the lessons that are new here)

1. **`/GS` doesn't protect heap buffers — and usually isn't even emitted.** The stack cookie guards
   *stack arrays*. These functions have none (the destination is a heap pointer), so MSVC inserts no
   cookie at all: **the `/O2 /GS` build is typically byte-for-byte identical to `/O2`.** Heap overflows
   sail straight past `/GS`. (Compare the stack exhibit, where `/GS` clearly injects a cookie.)
2. **The crash is deferred and non-deterministic.** A stack smash trips on `ret`; a heap overflow
   corrupts the *next chunk's* metadata and typically faults **later**, at a subsequent `free()` or
   `malloc()` when the heap manager validates its bookkeeping — or not at all, depending on layout.
3. **Make it fault reliably with Page Heap.** To turn a heap overflow into an immediate access
   violation at the moment of the bad write (so you can practice cleanly), enable Full Page Heap for
   the exe — a distinctly-Windows technique:
   ```
   gflags /p /enable vuln.exe /full      # then run; disable later with: gflags /p /disable vuln.exe
   ```
   (or use Application Verifier.) Under Page Heap the allocation is placed against a guard page, so
   the first out-of-bounds byte faults exactly where it's written.
4. **What you corrupt is data, not control-flow (directly).** Adjacent allocations and NT/segment-heap
   (and LFH) metadata — the path to control of execution is longer and allocator-dependent.

## Build & run

From **any PowerShell** at the museum root — the script locates the Visual Studio toolchain itself:

```
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe              # x64 (default)
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe -Arch x86    # 32-bit
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Exe -Arch both   # both
```

This regenerates each variant's `asm_*.txt` (the canonical **x64** reference) and builds runnable
executables to `heap-overflow\variant-XX\bin\<arch>\<config>\vuln.exe` (`od` / `o2` / `o2gs`). Then,
e.g.:

```
heap-overflow\variant-01-strcpy\bin\x64\od\vuln.exe ("A"*200)
```

(For a reliable fault at the write site, run it under Page Heap as shown above.)

## Caveat: vocabulary vs grammar

Like the stack variants, each is a tiny function — it teaches fast, local idiom recognition, not the
global reachability/lifetime reasoning real audits demand. See the root
[README](../README.md#what-this-museum-teaches--and-what-it-cant). The module's **capstone** —
**Avalanche**, an in-memory KV cache server that hides these six idioms across genuine functionality —
lives in [capstone/](capstone/). Audit it from source or reverse the built binaries; the withheld
answer key stays out of the tree (`capstone/_solution/`, gitignored).
