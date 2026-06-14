# Variant 02 — `sprintf("%s")`

**Bug class:** CWE-121 (Stack-based Buffer Overflow)
**Idiom (Axis B):** unbounded copy via a format conversion, not a string primitive.
**Disguise:** `sprintf` reads as *formatted, deliberate* output, so reviewers relax. But `"%s"` of an attacker-controlled string has no field-width cap — it expands to a raw, unbounded copy into `buf[16]`, identical in effect to variant-01's `strcpy`.

```c
void vuln(const char *input) {
    char buf[16];
    sprintf(buf, "%s", input);   // unbounded — same as strcpy(buf, input)
}
```

The fix is the same family as everywhere else: bound the write (`snprintf(buf, sizeof buf, "%s", input)`), and even then mind truncation semantics.

---

## Recognition signatures (x64 PE / MSVC)

- **`/Od`:** dest stack local in **RCX**, format literal `"%s"` in **RDX**, attacker data in **R8** (first vararg). A printf-family call whose arg0 is `lea [rsp+...]` and whose format is `"%s"` = unbounded copy.
- **`/O2`:** registers tightened; *may* be canonicalized toward a `strcpy`-style copy by the optimizer — a reminder that the primitive in the binary needn't match the primitive in source.
- **`/O2 /GS`:** identical canary wrapper to variant-01 — the mitigation responds to the buffer, not the copy primitive.

> Cross-variant lesson: don't pattern-match on the function name (`strcpy`). Pattern-match on the **property**: a fixed stack buffer as the destination of a length-unbounded write. `sprintf("%s")`, `strcat`, `gets`, `scanf("%s")`, and a hand loop all satisfy it.
