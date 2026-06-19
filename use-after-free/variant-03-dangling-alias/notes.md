# Variant 03 — dangling alias (freed via one pointer, used via another)

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** two pointers name the same allocation; one frees it, the other is used after.
**Disguise:** the `free(buf)` looks completely fine on its own — `buf` is never touched again. The
defect is that a *second* pointer (`g_last`) outlived the object. This is the lesson the baseline
can't teach: pointer **copies share a lifetime**, and the dangerous use can be arbitrarily far from
the free, in unrelated code.

```c
static char *g_last;
...
g_last = buf;          /* alias retained */
free(buf);             /* freed through buf */
printf("%s\n", g_last);/* used through the dangling alias */
```

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od` & `/O2`:** a store of the buffer pointer into a **global/field** (`mov [g_last], rbx`),
  then `call free` on that pointer, then a **load from the same global** that is dereferenced
  (`mov rdx, [g_last]` feeding `printf`). The freed register and the used value are the same address,
  linked by the store into the alias.
- **The auditing tell:** don't stop at "`buf` is freed and not reused." Ask *who else holds this
  address* — track every assignment of the pointer, not just the variable that was freed.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## UAF behavior
- The use is through a global that survives the function, so in a real program the dangling read can
  happen in a totally different call. Page Heap (`gflags /p /enable vuln.exe /full`) faults at the
  `g_last` dereference.

## Decompiler tells
- `g_last = buf; free(buf); printf("%s", g_last);` — the freed pointer and the printed pointer are
  the same object reached by two names.
