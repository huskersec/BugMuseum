# Variant 05 — `malloc(strlen(input))` missing the `+1`

**Bug class:** CWE-122 (Heap-based Buffer Overflow), via CWE-131 (Incorrect Calculation of Buffer
Size) / CWE-193 (Off-by-one)
**Idiom (Axis B):** the allocation is *sized to the input* — but the arithmetic forgets the NUL.
**Disguise:** the strongest in the exhibit. The code makes the gesture we praise — it allocates
exactly what the data needs — so it reads as careful, dynamic, right-sized. But `strlen` excludes the
terminator:

```c
size_t n  = strlen(input);
char  *buf = malloc(n);     /* one byte short */
strcpy(buf, input);         /* writes n + 1 bytes (text + NUL) */
```

Result: a **one-byte heap overflow for any input** — the terminating `\0` lands one byte past the
allocation, onto the next chunk's header. Heap-specific (you don't "allocate" like this on the stack),
and the historically notorious off-by-one that corrupted allocator metadata low bytes.

---

## Recognition signatures (x64 PE / MSVC)
- **The tell:** a `call strlen` whose result flows into `malloc`'s size argument with **no `inc` /
  `add ...,1` / `lea ...,[rax+1]`** in between, followed by a `strcpy` of the same string into that
  allocation. The "+1" that should be there is conspicuously absent.
- **Correct counter-shape:** `lea rcx,[rax+1]` (or `inc rax`) before `call malloc`.
- **`/O2`:** `strlen` may inline, but the missing increment between length and allocation size is the
  invariant tell.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (heap destination).

> Auditing takeaway: when an allocation is computed from a string length, the size must account for
> the terminator (and for element width, for wide strings). `malloc(strlen(s))` is a bug; the unit and
> the `+1` are exactly what the binary makes you verify.

## Heap behavior
- A single OOB NUL is subtle — it may not fault on `free` at all, and is the easiest variant to miss
  by running. Use `gflags /p /enable vuln.exe /full` for a guaranteed fault on the one-byte overrun.
