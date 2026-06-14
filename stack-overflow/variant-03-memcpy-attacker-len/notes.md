# Variant 03 — `memcpy` with an attacker-controlled length

**Bug class:** CWE-121 (Stack-based Buffer Overflow), via CWE-805 (Buffer Access with Incorrect Length Value)
**Idiom (Axis B):** the write *is* bounded — by a tainted length.
**Disguise:** a `memcpy` with an explicit size argument reads as disciplined code. The defect isn't a missing bound; it's a **present bound that trusts the wrong source**. `len` flows in from outside and is never clamped to `sizeof(buf)`.

```c
void vuln(const char *input, unsigned int len) {
    char buf[16];
    memcpy(buf, input, len);   // bounded — but by the attacker
}
```

This is the museum's smallest taste of real-world hardness #2 ("looks-correct guards"): the bug lives in the *relationship* between where `len` comes from and where it's used, not in any single suspicious-looking call.

---

## Recognition signatures (x64 PE / MSVC)

- **`/Od` & `/O2`:** the size register (**R8/R8D**) is loaded from an incoming parameter and reaches `call memcpy` with **no `cmp`/`jae` against `10h`** in between. The discriminator is *provenance of the size*, not presence of a size.
- **Safe counter-shape** to contrast against: a correct version shows `mov r8d, 10h` (constant) or a `cmp edx,10h; cmova/cmovae` clamp before the call.
- **`/O2 /GS`:** the canary still fires (non-constant copy into a stack buffer). Important nuance: the cookie only guards the **return address on exit** — it does not prevent clobbering adjacent locals before the check, nor an OOB read leak. A canary is a backstop, not a bounds check.

> Auditing takeaway: when you see a sized copy, don't tick the box. Trace the size backward. "Has a length argument" and "is correctly bounded" are different claims, and the binary makes you prove the second one yourself.
