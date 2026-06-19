# Variant 01 — use-after-free (read), the baseline

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** the simplest temporal bug — `free(p)` then read `*p`.
**Root cause:** the pointer outlives the object. Once `free` returns, the chunk belongs to the
allocator again; reading it yields whatever is there now — old data, a freelist link the allocator
wrote, or bytes a later allocation placed there.

```c
char *buf = malloc(32);
strncpy(buf, input, 31); buf[31] = '\0';
free(buf);                 /* lifetime ends */
return buf[0];             /* use-after-free read */
```

This is the reference point for the exhibit: no out-of-bounds access at all — the defect is purely
*when* the access happens relative to the object's death.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** `call free` with the buffer pointer, then — after the call — a **deref-read** of the same
  stack slot (`movzx eax, byte ptr [rax]` where `rax` was just reloaded from `buf`).
- **`/O2`:** the pointer lives in a non-volatile register (e.g. `rbx`) across the `free`; the tell is a
  `mov/movzx ..., byte ptr [rbx]` **after** `call free` consumed `rbx`. Reading a pointer you just
  passed to `free` is the whole bug.
- **`/O2 /GS`:** identical to `/O2` — **no cookie**. `/GS` guards stack arrays; this isn't one. UAF has
  no compile-time mitigation (see the exhibit README).

## UAF behavior / how to observe it
- Frequently **no crash** — `buf[0]` just returns a stale byte. Under `gflags /p /enable vuln.exe /full`
  the freed page is protected and the read faults immediately, exactly at the dangling access.
- To *see* the data take on another allocation's contents, the capstone reuses the chunk; here the
  point is simply: the read happens after the lifetime ended.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `/Od` & `/O2`: `free(buf); return buf[0];` — the use plainly follows the free.
- `/O2/GS`: identical; **no `__security_cookie`** in the body.
