# Variant 04 — free on the error path + shared cleanup

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** an error branch frees, then falls through to the common `cleanup:` tail that frees again.
**Root cause:** the `goto cleanup` idiom centralizes resource release at one label. An error branch does
its own `free(buf)` and then `goto cleanup`, but the cleanup tail *also* frees `buf`. The error path
should have nulled `buf` (or skipped the tail free) before merging. Each free is locally sensible — local
cleanup, then canonical cleanup — and that is why this is the most common real double-free in the wild.

```c
if (buf[0] == '!') {
    free(buf);          /* local cleanup on the error path... */
    goto cleanup;       /* ...falls into the shared tail */
}
...
cleanup:
    free(buf);          /* ...which frees buf a second time on error */
```

Fires only on the error condition (`input[0] == '!'`) — the happy path frees exactly once. The bug lives
in the *relationship* between a branch and the join it flows into, not on any single line.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** a `call free` on `[buf]` **inside** the conditional block, and a second `call free` on `[buf]`
  at the merged `cleanup:` label that both the error and success paths reach. The error block contains no
  `mov qword ptr [buf], 0` before the `jmp` to cleanup — the missing null is the defect.
- **`/O2`:** the compiler keeps `buf` in a callee-saved register and may lay the two frees out as a
  conditional free followed by an unconditional one on the fall-through. The tell: one `call free` is
  control-dependent on the branch, the other dominates the return; both consume the same register.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## Double-free behavior / how to observe it
- `vuln.exe !AAA` triggers it; `vuln.exe AAA` is clean. The error path is the only one that double-frees,
  so reachability depends on the input selecting the error branch.
- Same observability matrix as variant 01 (silent release / debug assert / Page Heap fault / segment-heap
  fast-fail).

## Decompiler tells (angr/pypcode ≈ Ghidra)
- Two `free(buf)` calls on different control-flow paths that converge: one guarded by the error condition,
  one on the path all branches reach. A decompiler that reconstructs the `goto` shows the error block
  flowing into the same `cleanup` that the normal path uses, with `free(buf)` in both.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
