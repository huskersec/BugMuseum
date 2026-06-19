# Variant 02 — use-after-free (write)

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** `free(p)` then write `*p`.
**Root cause:** same temporal defect as variant 01, but the dangling access is a **store**. Writing
into a reclaimed chunk overwrites allocator bookkeeping (a freelist link) or the contents of whatever
allocation now owns that memory — a write primitive an attacker can steer by controlling reuse.

```c
char *buf = malloc(32);
free(buf);                 /* lifetime ends */
strncpy(buf, input, 31);   /* use-after-free write */
buf[31] = '\0';
```

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od` & `/O2`:** `call free` with the buffer, then a **write** through the same pointer — here a
  `call strncpy` whose destination (`rcx`) is the freed buffer, plus a `mov byte ptr [buf+1Fh],0`.
  The discriminator vs variant 01 is read-vs-write into the freed chunk.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (not a stack-buffer bug).

## UAF behavior
- A write-UAF is the more exploitable half: under heap grooming the freed chunk is reused by an
  attacker-influenced object, so the stray write lands on *its* fields (e.g. a length or pointer).
- Under `gflags /p /enable vuln.exe /full` the write faults immediately on the freed page; without it
  the corruption is silent until the affected memory is next used.

## Decompiler tells
- `free(buf); strncpy(buf, a0, 31);` — the store target is the just-freed pointer.
