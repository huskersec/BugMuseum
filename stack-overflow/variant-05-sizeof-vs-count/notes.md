# Variant 05 — `wcsncpy(buf, input, sizeof(buf))` (bytes vs. count)

**Bug class:** CWE-121 (Stack-based Buffer Overflow), via CWE-131 / CWE-135 (Incorrect Buffer/String Size Calculation)
**Idiom (Axis B):** a bound that *uses `sizeof`* and is still wrong, because of a units mismatch.
**Disguise:** the strongest in the exhibit. The code performs the exact ritual we tell auditors to look for — it bounds the copy with `sizeof(buf)`. But `wcsncpy` counts in **`wchar_t` units** while `sizeof` yields **bytes**:

```c
void vuln(const wchar_t *input) {
    wchar_t buf[16];                     // 16 wide chars = 32 bytes
    wcsncpy(buf, input, sizeof(buf));    // sizeof(buf) == 32 BYTES -> permits 32 wide chars = 64 bytes
}
```

Result: up to a **2× overflow** of a 32-byte buffer. The correct bound is `_countof(buf)` (`== sizeof(buf)/sizeof(buf[0]) == 16`). On Windows — where `wchar_t` UTF-16 strings and `_countof` are everywhere — this is a classic, frequently-shipped overrun.

---

## Recognition signatures (x64 PE / MSVC)

- **The tell:** a wide-string copy (`wcsncpy`, `wcscpy_s`, `StringCchCopyW`, …) whose count operand is the **constant `20h` (32)** while the destination is a `wchar_t[16]`. The bound *equals the byte-size*, which is the fingerprint of `sizeof`-where-`_countof`-was-needed. A correct call shows `r8 = 10h` (16).
- **Why it's hard at the binary level:** the count looks like a clean compile-time constant; nothing in the dataflow is "tainted" or "unchecked." You only see the bug after **typing the destination** (element width × element count). Tools that don't model element width miss it.
- **`/O2 /GS`:** the cookie sits directly above the buffer and *will* trip on return — but only after 32 stray bytes have already overwritten it and its neighbors. Detection on exit, never prevention.

> Auditing takeaway: `sizeof` is correct for *byte* APIs (`memcpy`, `memset`) and wrong for *element-count* APIs (`wcsncpy`, `_countof`-style). Match the unit of the size to the unit the function consumes. The bug is dimensional analysis, not a missing check.
