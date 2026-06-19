# Variant 06 — hand-rolled loop, off-by-one (`<=`) into a heap buffer

**Bug class:** CWE-122 (Heap-based Buffer Overflow) via CWE-193 (Off-by-one)
**Idiom (Axis B):** no library call at all — a manual copy loop into a `malloc` buffer, undone by a
single wrong comparison.
**Disguise:** nothing to grep for. No `strcpy`/`memcpy`, no tainted length. The loop looks size-aware
(`16` is right there). The only defect is `<=` where `<` was meant, so the index reaches `16` and
writes `buf[16]` — one byte past the 16-byte allocation.

```c
char *buf = malloc(16);
for (int i = 0; i <= 16; i++)   /* off-by-one: should be i < 16 */
    buf[i] = input[i];
```

A one-byte heap overflow is not "almost safe" — the stray byte lands on the next chunk's metadata.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** after `call malloc`, a byte-copy loop into the heap pointer whose guard is `cmp i,10h`
  followed by **`jbe`/`jle`** (≤) instead of **`jb`/`jl`** (<). The whole bug is one mnemonic — the
  boundary predicate.
- **`/O2`:** the loop is erased and the off-by-one re-emerges as a **16-byte vector store + one
  trailing single-byte store** into the heap buffer = 17 bytes. That "16 + 1" shape is the
  release-build fingerprint.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (heap destination). On the stack this off-by-one
  would clip the cookie's low byte and be caught on return; on the heap there is no such backstop.

> Auditing takeaway: bounds bugs don't require a dangerous function, and the heap removes the safety
> net. At `/O0` interrogate the predicate (`jb` vs `jbe`); at `/O2` look for the lone "+1" store
> hanging off a block copy into a malloc'd pointer.

## Heap behavior
- Single OOB byte onto the next chunk header; may not fault on `free`. `gflags /p /enable vuln.exe
  /full` for a guaranteed fault at the write.
