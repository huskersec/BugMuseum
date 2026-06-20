# Variant 06 — shared owned member (two destructors, one buffer)

**Bug class:** CWE-415 (Double Free)
**Shape (Axis B):** two owner structs share one sub-buffer; running each destructor frees it twice.
**Root cause:** a `Blob` owns its `->data` buffer, and `blob_destroy()` frees `->data` (and nulls the
field). Two Blobs are built, but the second is made to *share* the first's buffer (`b.data = a.data`)
rather than copy it. Both Blobs now believe they own that buffer, so destroying each frees it. The
destructor is correct for an owning Blob; the defect is shared (not copied) ownership.

```c
b.data = a.data;        /* share, don't copy -> two owners */
...
blob_destroy(&a);       /* free(a.data); a.data = NULL; */
blob_destroy(&b);       /* free(b.data) — same chunk -> double free */
```

Subtlety worth dwelling on: `blob_destroy` *does* null the field it frees. But `a.data` and `b.data` are
two distinct pointer variables that happen to hold the same address — nulling one cannot null the other.
**Null-after-free protects the owner you cleared, never an alias.** This is the structural reason shared
ownership defeats the usual hygiene.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** `b.data = a.data` is a `mov` copying one struct field to another (same value, two slots). Each
  `blob_destroy` does `mov rcx, [obj+0]; call free` — a free of the `->data` member at offset 0 — reached
  through two different `Blob` addresses. The tell: two `free [base+0]` where the two bases were assigned
  the same pointer.
- **`/O2`:** `blob_destroy` (small) likely **inlines**; the two member-frees become two `call free` on the
  same register-held data pointer. The `b.data = a.data` copy may be optimized to reuse the register
  directly, making the shared origin explicit.
- **`/O2 /GS`:** identical to `/O2` — no cookie. (`/GS` covers stack arrays; the `Blob`s here hold pointers,
  not in-struct char arrays, so even the local structs draw no cookie.)

## Double-free behavior / how to observe it
- `vuln.exe AAAA` triggers it unconditionally. Same observability matrix as variant 01.
- The field-nulling in `blob_destroy` is a deliberate red herring: it looks like correct hygiene and would
  prevent a *self* double-free, but does nothing against the alias.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- A pointer field copied between two structs (`b->data = a->data`), then both structs' `data` freed. The
  decompiler shows two `free(x->data)` (or two inlined `free(...)`) reaching the same value.
- `/O2/GS`: identical to `/O2`; no `__security_cookie`.
