# Variant 07 — realloc input *and* output freed

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** after a successful `realloc`, both the old pointer and the new pointer are freed.
**Root cause:** `realloc(p, n)` releases `p` as a side effect (moving the data to a fresh block if it
can't grow in place) and returns the new pointer. On success, `p` is dead and must not be freed. Here
`vuln` frees **both** `buf` (the input) and `grown` (the output). If realloc moved the block,
`free(buf)` frees an already-released chunk; if it grew in place (`buf == grown`), the two frees hit the
same chunk outright. Either way: double free.

```c
char *grown = realloc(buf, 64);     /* frees+moves buf on success */
if (!grown) { free(buf); return -1; }   /* failure path: buf still valid (correct) */
...
free(buf);                          /* WRONG: realloc already released buf */
free(grown);                        /* the real live block */
```

`realloc` is the one standard call that frees its argument, which is exactly why "free the old, free the
new" is such a recurring mistake. Note the failure branch is *correct* — on `NULL`, realloc keeps the
original, so freeing `buf` there is right. The bug is only on the success path. This is the module's tie
back to the arithmetic/`realloc` thread (cf. UAF variant 04, stale-after-realloc — there the old pointer
is *used*; here it is *freed*).

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** `mov rcx, [buf]; call realloc; mov [grown], rax`, a NULL check (`test rax,rax; je`), then on
  the success path two `call free` — one on `[buf]` (the realloc input) and one on `[grown]` (its output).
  Freeing the **same pointer you passed to realloc** after a successful realloc is the tell.
- **`/O2`:** `buf` and `grown` occupy callee-saved registers; look for `mov rcx, rbx; call realloc; ... ;
  mov rcx, rbx; call free` (free of the realloc input) alongside a free of the result register. The input
  register is consumed by both `realloc` and a later `free`.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## Double-free behavior / how to observe it
- `vuln.exe AAAA` triggers it (realloc succeeds). Whether `buf == grown` or the block moved, the second
  free of `buf` is invalid. Same observability matrix as variant 01.
- Under Page Heap the moved-block case is especially crisp: realloc relocates the data to a new guarded
  page and unmaps the old one, so `free(buf)` faults on a freed page immediately.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `grown = realloc(buf, 64); ... free(buf); free(grown);` — a free of a realloc argument after a
  successful realloc, plus a free of the result. The decompiler shows `buf` reaching `free` despite having
  been passed to `realloc`.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
