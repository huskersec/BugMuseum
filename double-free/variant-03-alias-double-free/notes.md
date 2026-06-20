# Variant 03 — alias double-free (freed through two pointers)

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** the chunk has two owners — a local and a retained global alias — and each frees it.
**Root cause:** `vuln` caches the last-processed buffer in a global (`g_last`) and frees the previous one
on each call, *and also* frees the buffer in its own cleanup. The same address now lives in two homes;
both are treated as owners. The double-free straddles two calls: call N frees `buf` and stores its
address in `g_last`; call N+1 does `free(g_last)` on that already-freed chunk.

```c
static char *g_last;
...
free(g_last);   /* releases the PREVIOUS call's buffer... */
g_last = buf;   /* ...then aliases this one */
free(buf);      /* ...and frees it now too -> next call double-frees it */
```

This is the double-free counterpart of UAF variant 03 (dangling-alias): same two-homes-one-chunk setup,
but the second operation is a *free*, not a use. The fix is single ownership and clearing the alias
(`g_last = NULL` when its target dies).

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** `free` is called on the **global** `g_last` (`mov rcx, [g_last]; call free`) and, separately,
  on the local `buf`; `g_last` is then assigned `buf` (`mov [g_last], rax`). The tell is a `free` of a
  global/static pointer plus a store of a just-allocated pointer into that same global — the alias.
- **`/O2`:** the global access stays (`g_last` is observable state, not register-promotable across the
  call); look for `mov rcx, qword ptr [g_last]; call free` and a later `mov qword ptr [g_last], rbx`. The
  aliasing is visible as the same value reaching both `free(buf)` and a store to `g_last`.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## Double-free behavior / how to observe it
- Needs **two invocations** to fire (the binary's `main` calls `vuln` twice when given two args):
  `vuln.exe AAAA BBBB`. The first call seeds `g_last`; the second's `free(g_last)` is the double free.
- Single-call runs look clean — a reminder that double-free reachability can be a *state/sequence* fact
  across calls, not a single-shot line (exactly the "grammar" the capstone will stress).

## Decompiler tells (angr/pypcode ≈ Ghidra)
- A module-level global is freed and then reassigned from a fresh allocation; the same pointer is also
  freed locally. The decompiler shows `free(g_last); g_last = buf; free(buf);` — two owners, one chunk.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
