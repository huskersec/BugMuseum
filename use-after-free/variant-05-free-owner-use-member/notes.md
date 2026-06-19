# Variant 05 — free the owner, use a member

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** a destructor frees an owned sub-allocation; a saved pointer into it is used after.
**Disguise:** the use site calls `buffer_free(b)` — a perfectly ordinary destroy — and then uses `p`.
Nothing local says `free(p)`; the free happens *inside the destructor*, on a member. You only see the
bug by knowing that destroying the owner releases the part `p` points into. This is the
object-lifetime version of the dangling-pointer trap, and it's the most realistic.

```c
char *p = b->data;     /* alias into an owned allocation */
buffer_free(b);        /* frees b->data (and b) */
printf("%s\n", p);     /* p now dangles */
```

---

## Recognition signatures (x64 PE / MSVC)
- **The tell (at the use site):** a pointer is loaded from a structure field (`p = b->data`), then the
  owner is passed to a destroy routine (`call buffer_free`), then the saved pointer is used. The `free`
  is in the callee — so recognizing this requires following what the destructor releases, not just
  scanning for `free` near the use.
- **`/Od` & `/O2`:** `p` (the saved `b->data`) lives in a slot/register across `call buffer_free`; the
  deref of `p` afterward is the UAF.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## UAF behavior
- Because the free is non-local (inside the destructor), this is exactly the kind of UAF that survives
  code review: the use site looks clean. Page Heap faults at the `p` dereference.

## Decompiler tells
- `p = b->data; buffer_free(b); printf("%s", p);` — the dereferenced pointer was a member of the
  object just destroyed. (To confirm, read `buffer_free`: it does `free(b->data); free(b);`.)
