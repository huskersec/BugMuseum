# Variant 01 — double-free (baseline)

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** the simplest temporal double-free — `free(p)` then `free(p)` again, straight line.
**Root cause:** the pointer outlives its first free, and is freed a second time. The first `free` returns
the chunk to the allocator; the second `free` pushes that same chunk onto the freelist a second time,
corrupting the allocator's bookkeeping (a freelist can end up pointing the chunk at itself, or two later
allocations can be handed the same address).

```c
char *buf = malloc(32);
strncpy(buf, input, 31); buf[31] = '\0';
int r = buf[0];            /* legitimate use, before any free */
free(buf);                 /* lifetime ends; chunk -> freelist */
free(buf);                 /* freed again -> freelist corruption */
```

This is the reference point for the exhibit. Contrast it with the UAF baseline: there the second access
*reads* the dead object (corrupts/leaks **user** data); here the second access *frees* it (corrupts the
**allocator**). Same dangling pointer, different sink — and the double-free's blast radius is the whole
heap, not one buffer.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** the buffer pointer lives in a stack slot; you see `mov rcx, [buf]; call free` **twice**, with
  the slot never zeroed between the two calls (no `mov qword ptr [buf], 0`). Two `call free` reaching the
  same pointer with no reassignment is the whole bug.
- **`/O2`:** the pointer is held in a non-volatile register (e.g. `rbx`/`rsi`) across both calls; the tell
  is `mov rcx, rbx; call free` appearing twice with `rbx` unchanged in between.
- **`/O2 /GS`:** identical to `/O2` — **no cookie**. `/GS` guards stack arrays; nothing here is one.
  Double-free has no `/GS` mitigation.

## Double-free behavior / how to observe it
- **Release CRT (`/MD`):** often **no immediate crash** — the second free quietly corrupts the freelist;
  the failure surfaces later (a wild write, a crash in an unrelated `malloc`), making it hard to trace.
- **Debug CRT (`/MDd`):** the heap is validated and the second free trips a debug assertion / reports an
  invalid heap pointer.
- **Page Heap / Application Verifier** (`gflags /p /enable vuln.exe /full`): the freed page is protected, so
  the second free faults immediately, exactly at the dangling release.
- **Windows segment heap / LFH:** may detect the freelist inconsistency and raise `STATUS_HEAP_CORRUPTION`
  (fast-fail, `__fastfail`) — a clean abort rather than silent corruption.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- All three builds: `free(buf); free(buf);` — two releases of the same pointer with no intervening
  assignment. The decompiler shows no `buf = ...` between them, which is exactly the signature.
- `/O2/GS`: identical to `/O2`; **no `__security_cookie`** in the body.
