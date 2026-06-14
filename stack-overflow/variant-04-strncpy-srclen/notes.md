# Variant 04 — `strncpy(buf, input, strlen(input))`

**Bug class:** CWE-121 (Stack-based Buffer Overflow)
**Idiom (Axis B):** the bounded-looking primitive with the bound computed from the wrong operand.
**Disguise:** `strncpy` is widely (mis)taught as "the safe strcpy." The `n` argument *looks* like a guard, but here it's `strlen(input)` — the length of the **source**, not the capacity of the **destination**. Whenever `strlen(input) >= 16`, the copy overruns. `strncpy(d, s, strlen(s))` is just `strcpy(d, s)` with extra instructions.

```c
void vuln(const char *input) {
    char buf[16];
    strncpy(buf, input, strlen(input));   // wrong operand: should be sizeof(buf)-1
}
```

(There's a second latent bug even when sized correctly: `strncpy` doesn't NUL-terminate on truncation. The overflow is the headline here.)

---

## Recognition signatures (x64 PE / MSVC)

- **The tell:** a `call strlen` whose result feeds the **count register (R8)** of the following sized copy, where the measured pointer is the *same* pointer used as the copy **source**. `sizeof(dest)` / the immediate `10h` never appears.
- **`/O2`:** `strlen` may inline into a scan loop, but the dataflow `len(source) → count` is unchanged — there is no `cmp ...,10h` clamp.
- **`/O2 /GS`:** canary fires anyway; the `n`-primitive's "safe" appearance doesn't fool the compiler's buffer analysis.

> Auditing takeaway: `strncpy`/`snprintf`/`memcpy` are only as safe as the *number you hand them*. The reliable check is "is the size operand `sizeof(dest)` (or a value provably ≤ it)?" — answer it from the data flow, not from the function's reputation.
