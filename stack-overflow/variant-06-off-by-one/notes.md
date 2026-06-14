# Variant 06 — hand-rolled loop, off-by-one (`<=`)

**Bug class:** CWE-121 (Stack-based Buffer Overflow) via CWE-193 (Off-by-one Error)
**Idiom (Axis B):** no library call at all — a manual copy loop that even references the buffer size, undone by a single wrong comparison.
**Disguise:** there's nothing to grep for. No `strcpy`, no `memcpy`, no tainted length. The loop looks size-aware (`16` is right there). The only defect is `<=` where `<` was meant, so the index reaches `16` and writes `buf[16]` — exactly one byte past the end.

```c
void vuln(const char *input) {
    char buf[16];
    int i;
    for (i = 0; i <= 16; i++)   /* off-by-one: should be i < 16 */
        buf[i] = input[i];
}
```

A one-byte overflow is not "almost safe" — it's the historically devastating *poison null byte* / saved-register-LSB class.

---

## Recognition signatures (x64 PE / MSVC)

- **`/Od`:** a byte-copy loop whose guard is `cmp i,10h` followed by **`jbe`/`jle`** (≤) instead of **`jb`/`jl`** (<). The entire vulnerability is one mnemonic — the boundary predicate. This is the same "read the flag the jump tests, not just *that* there's a check" discipline as the signedness class.
- **`/O2`:** the loop is erased and the off-by-one re-emerges as a **16-byte vector store + one trailing single-byte store** = 17 bytes. That **"16 + 1"** shape is the release-build fingerprint of a one-past-the-end copy.
- **`/O2 /GS`:** the stray byte typically lands on the **low byte of the cookie**, so /GS catches this particular off-by-one on return. (The dangerous historical cases were precisely the ones that perturb length/alignment metadata *without* changing the checked cookie value.)

> Auditing takeaway: bounds bugs don't require a dangerous function. A correct-looking manual loop hides the defect in its terminating condition. At `/O0` interrogate the predicate (`jb` vs `jbe`); at `/O2` look for the lone "+1" store hanging off a block copy.
