# Variant 02 — `sprintf("%s")` into a heap buffer

**Bug class:** CWE-122 (Heap-based Buffer Overflow)
**Idiom (Axis B):** unbounded copy via a format conversion into a `malloc` buffer.
**Disguise:** `sprintf` reads as deliberate, formatted output, so reviewers relax. But `"%s"` of an
attacker string has no field-width cap — it expands to a raw, unbounded write into the 16-byte heap
allocation, identical in effect to variant-01's `strcpy`.

```c
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    sprintf(buf, "%s", input);   // unbounded — same as strcpy(buf, input)
    free(buf);
}
```

Fix: bound it (`snprintf(buf, 16, "%s", input)`), and size the allocation to the real need.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od` & `/O2`:** `call malloc` → heap pointer in **RCX** (arg0), format literal `"%s"` in **RDX**,
  attacker data in **R8** (first vararg), then `call sprintf`, then `call free`. A printf-family call
  whose arg0 is a heap pointer and whose format is `"%s"` = unbounded heap copy.
- **`/O2 /GS`:** identical to `/O2` — **no cookie** (no stack array to guard). See variant-01 / the
  exhibit README for why `/GS` is irrelevant to heap overflows.

> Cross-variant lesson: match on the **property** (a fixed heap buffer as the destination of a
> length-unbounded write), not the function name. `sprintf("%s")`, `strcat`, `strcpy`, and a hand
> loop all satisfy it.

## Heap behavior
- Corrupts the adjacent chunk; fault is deferred to `free`/next allocation. Use `gflags /p /enable
  vuln.exe /full` for an immediate fault at the write.
