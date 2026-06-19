# Variant 04 — stale pointer after `realloc`

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** `realloc` frees/moves the old block; the old pointer is used afterward.
**Disguise:** `realloc` *looks* like it grows the buffer in place. But it may allocate a new block,
copy, and free the old one — returning the new address. Code that keeps using the **input** pointer
(or any saved copy of it) after a relocating `realloc` is using freed memory.

```c
char *grown = realloc(buf, bigger);
if (!grown) { free(buf); return; }
strcat(buf, more);   /* BUG: `buf` may have been freed by realloc; use `grown` */
```

The correct code uses only `grown` after the call (and treats `buf` as invalid).

---

## Recognition signatures (x64 PE / MSVC)
- **The tell:** `call realloc` whose **input** pointer is then used after the call, while the return
  value lives in a different register/slot. You'll see the realloc result saved (e.g. to `grown`), a
  NULL check, then a deref/`call` (here `strcat`) on the *original* pointer rather than the result.
- **`/Od` & `/O2`:** the original `buf` and the result `grown` occupy distinct slots/registers; the
  use after `realloc` reads the wrong one (the realloc argument).
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## UAF behavior
- Whether it faults depends on whether `realloc` moved — allocator- and size-dependent, so it can
  appear to "work" for small growth and fail intermittently. Page Heap forces relocation on every
  `realloc`, making the use-after-free deterministic.

## Decompiler tells
- `grown = realloc(buf, n); ... strcat(buf, more);` — the post-realloc use names `buf` (the freed
  input), not `grown`.
