# Variant 02 — guarded-but-not-nulled (the decorative guard)

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** a double-free wearing a `if (p) free(p)` guard that protects nothing.
**Root cause:** developers add `if (p) free(p)` believing it prevents a double-free. `free()` does not
modify `p`, so after the first guarded free the pointer is still its old non-NULL value — the second
`if (p)` is true and frees again. The guard is real only if the pointer is **nulled after freeing**
(`p = NULL`), which turns a repeat free into the harmless `free(NULL)`.

```c
if (buf) free(buf);    /* guarded free #1 — buf is NOT nulled */
/* ... */
if (buf) free(buf);    /* buf still non-NULL -> guard passes -> double free */
```

This is the "looks-defensive" disguise: the code reads as protection against exactly this bug, which is
why it survives review. The lesson is the fix discipline itself — *null-after-free*, the same discipline
Revenant used to avoid UAF.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** two `cmp qword ptr [buf], 0` / `je` guards, each followed by `call free` on `[buf]`. The tell
  is that **`buf` is never stored back** (no `mov qword ptr [buf], 0`) between or after the frees — both
  branches are live on the same value.
- **`/O2`:** the compiler keeps `buf` in a register; you may see a single `test rbx, rbx; je` before each
  `call free`, with `rbx` unchanged across both. The optimizer will **not** fold the two frees away — it
  can't prove `free` leaves the pointer dead, and the guards are not redundant from its view.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## Double-free behavior / how to observe it
- Same observable outcomes as variant 01 (silent in release, asserted in debug, faulted under Page Heap),
  with the extra cognitive point that the `test`/`jz` guards make the asm *look* careful. A reviewer must
  notice the missing `mov [buf], 0` — the guard checks a value that nothing ever clears.
- If you mentally add `buf = NULL;` after the first free, the second `if (buf)` becomes provably false and
  (at `/O2`) the compiler may drop the dead second free entirely. The presence of a live second free is
  the proof the null was missing.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `if (buf) free(buf);` appears twice with no `buf = 0` between — the decompiler shows the guard but no
  assignment that would make it meaningful.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
