# Variant 03 — `memcpy` with an attacker-controlled length (heap)

**Bug class:** CWE-122 (Heap-based Buffer Overflow), via CWE-805 (Buffer Access with Incorrect Length)
**Idiom (Axis B):** the write *is* bounded — by a tainted length, into a `malloc` buffer.
**Disguise:** a `memcpy` with an explicit size reads as disciplined code. The defect isn't a missing
bound; it's a **present bound that trusts the wrong source** — `len` flows in from outside and is
never clamped to the 16-byte allocation.

```c
void vuln(const char *input, unsigned int len) {
    char *buf = (char *)malloc(16);
    memcpy(buf, input, len);   // bounded — but by the attacker
    free(buf);
}
```

This is the museum's clearest taste of real-world hardness #2: the bug lives in the *relationship*
between where `len` comes from and the allocation it's used against, not in any single suspicious call.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od` & `/O2`:** after `call malloc`, the size register (**R8/R8D**) is loaded from an incoming
  parameter and reaches `call memcpy` with **no `cmp ...,10h`** against the allocation size in between.
  The discriminator is *provenance of the size*, not presence of a size.
- **Safe counter-shape:** a correct version shows `mov r8d, 10h` (the alloc size) or a `cmp/cmova`
  clamp before the copy.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (heap destination, no stack array). Even if a
  cookie existed, it guards the return address, not adjacent heap chunks.

> Auditing takeaway: a sized copy isn't a safe copy. Trace the size backward to its origin and prove
> it's ≤ the allocation. On the heap there's no `/GS` backstop at all.

## Heap behavior
- A large `len` overruns the chunk and smashes the next allocation's header; fault is deferred to
  `free`/next `malloc`. `gflags /p /enable vuln.exe /full` faults immediately at the write.
- Trigger: `vuln.exe <data> <len>` with `len > 16` (e.g. `vuln.exe AAAA... 200`).
