# Variant 01 — `strcpy` into a heap buffer (the baseline)

**Bug class:** CWE-122 (Heap-based Buffer Overflow)
**Idiom (Axis B):** plain `strcpy(buf, input)` where `buf` came from `malloc` — the canonical,
undisguised heap case.
**Root cause:** `strcpy` copies until the source NUL with no destination bound; the 16-byte heap
allocation overruns forward into adjacent heap data and the next chunk's allocator metadata.

```c
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    strcpy(buf, input);   // no size — copies until input's NUL
    free(buf);
}
```

This is the reference point for the exhibit; it's the stack baseline relocated to the heap. The
recognition skill transfers — what changes is *where the destination lives* and *what gets smashed*.

---

## Recognition signatures (x64 PE / MSVC)

### `/Od` debug
- `mov ecx, 10h ; call malloc` → the destination is a **heap pointer**, saved to a local slot.
- `mov rcx, <buf> ; mov rdx, <input> ; call strcpy` with **no size argument**.
- `mov rcx, <buf> ; call free`.
> Fingerprint: the dest of the unbounded copy traces to a **`call malloc`**, not a stack `lea`.
> That single difference (heap pointer vs `lea rcx,[rsp+..]`) is how you tell a heap overflow from a
> stack overflow at the binary level — the copy primitive looks identical.

### `/O2` release
- Same three calls, registers tightened (input/buf kept in non-volatile regs across the calls).

### `/O2 /GS` — **no cookie appears**
- `vuln` has **no stack array** for `/GS` to guard (the overflowed buffer is on the heap), so MSVC
  emits **no `__security_cookie` logic at all** — this build is identical to `/O2`.
> This is the headline heap lesson: `/GS` is a *stack* mitigation. It does nothing for a heap
> overflow, and here it isn't even instrumented. Contrast the stack exhibit's variant 01, where the
> same `strcpy` triggers a cookie.

## Heap behavior / how to observe it
- The bad write corrupts the **next chunk header**; the process often survives the write and faults
  **later** at `free(buf)` (or a subsequent `malloc`) when the heap validates metadata — or appears
  to "work" entirely, depending on layout.
- For a deterministic fault at the write site: `gflags /p /enable vuln.exe /full`, then run. Page Heap
  backs the allocation against a guard page so the first OOB byte raises an access violation.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `/Od` & `/O2`: `buf = malloc(16); strcpy(buf, a0); free(buf);` — dest is a heap pointer.
- `/O2/GS`: identical; **no `__security_cookie`** in the body (nothing on the stack to guard).
