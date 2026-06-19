# Variant 04 — `strncpy(buf, input, strlen(input))` (heap)

**Bug class:** CWE-122 (Heap-based Buffer Overflow)
**Idiom (Axis B):** the bounded-looking primitive with the count computed from the wrong operand.
**Disguise:** `strncpy` is widely (mis)taught as "the safe strcpy." The `n` argument *looks* like a
guard, but here it's `strlen(input)` — the length of the **source**, not the 16-byte allocation.
Whenever `strlen(input) >= 16`, the copy overruns the heap chunk. `strncpy(d, s, strlen(s))` is just
`strcpy(d, s)` with more instructions.

```c
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    strncpy(buf, input, strlen(input));   // wrong operand: should be the allocation size
    free(buf);
}
```

---

## Recognition signatures (x64 PE / MSVC)
- **The tell:** after `call malloc`, a `call strlen` whose result feeds the **count register (R8)** of
  the following `strncpy`, where the measured pointer is the *same* pointer used as the copy source.
  The allocation size / immediate `10h` never appears.
- **`/O2`:** `strlen` may inline to a scan loop, but the dataflow `len(source) → count` is unchanged.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (heap destination). The "n" primitive's safe
  appearance doesn't matter; nothing guards the heap here.

> Auditing takeaway: `strncpy`/`snprintf`/`memcpy` are only as safe as the *number* you hand them.
> The reliable check is "is the size operand the allocation size (or provably ≤ it)?" — read it from
> the data flow, not the function's reputation.

## Heap behavior
- Overruns the chunk; deferred fault at `free`/next alloc. `gflags /p /enable vuln.exe /full` for an
  immediate fault.
