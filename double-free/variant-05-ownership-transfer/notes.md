# Variant 05 — ownership-transfer double-free (across a function boundary)

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** a callee frees the argument it was given; the caller, not knowing, frees it again.
**Root cause:** `consume()` has a "takes ownership" contract — it frees the pointer handed to it. `vuln`
calls `consume(buf)`, then frees `buf` again in its own cleanup, because from `vuln`'s view it allocated
`buf` and should release it. Two correct-looking frees, one chunk; the disagreement is about *who owns the
allocation after the call*. This is how double-frees hide in layered code: the free is split across an API
boundary, so neither side looks wrong alone.

```c
static int consume(char *p) { int r = p[0]; free(p); return r; }  /* takes ownership */
...
int r = consume(buf);   /* consume() frees buf */
free(buf);              /* caller frees it too -> double free */
```

Counterpart to UAF variant 05 (free-owner / use-member): same ownership confusion, but the second
operation is a free rather than a use. Fixes: a clear ownership contract (document/transfer), and the
caller nulling `buf` after handing it off.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** in `vuln`'s disassembly, `mov rcx, [buf]; call consume` followed by `mov rcx, [buf]; call free`
  — the same stack slot passed first to `consume` and then to `free`, with no reassignment between. The
  free *inside* `consume` is only visible if you also disassemble `consume` (the canonical dump is `vuln`).
- **`/O2`:** `consume` is a small leaf and may be **inlined** into `vuln` — in which case the two frees
  collapse into the same body and the variant reads like the baseline (two `call free` on one register).
  If not inlined, the tell is `call consume; ...; call free` on the same pointer. Either way: one chunk,
  two releases.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## Double-free behavior / how to observe it
- `vuln.exe AAAA` triggers it unconditionally. Same observability matrix as variant 01.
- Inlining at `/O2` is itself a lesson: ownership-transfer bugs can *vanish into* the caller, so the
  cross-function reasoning has to happen at the source/contract level, not only in the disassembly of one
  function.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- Not-inlined: `consume(buf); free(buf);` — a call whose body frees, then a free of the same pointer.
  Recognizing it requires reading `consume` and seeing the `free(p)` inside its contract.
- Inlined: two `free(buf)` in `vuln`'s body, indistinguishable from the baseline.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
