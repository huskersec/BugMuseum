# Variant 06 — free on a branch, use on the shared tail

**Bug class:** CWE-416 (Use After Free)
**Shape (Axis B):** a `free` reached on one control-flow path, then a use on the merged path.
**Disguise:** the "looks-correct guard" analog for the temporal class. The `free(buf)` is ordinary
cleanup. The bug is that the reject path falls through cleanup *and then logs `buf`* — so on that one
path the buffer is used after it was freed. Each line is reasonable alone; only the path that frees
*and* reaches the log is wrong.

```c
cleanup:
    free(buf);
    if (rc == 0)
        printf("rejected: '%s'\n", buf);   /* UAF only when we arrived via the reject path */
```

Trigger: an **empty** argument (`vuln.exe ""`) takes the reject path → `rc == 0` → the log derefs the
freed `buf`.

---

## Recognition signatures (x64 PE / MSVC)
- **The tell:** a `call free`, then a *conditional* deref of the same pointer further down (a `cmp`/`jne`
  guarding a `printf`/use of `buf`). The use is gated, so it only triggers on the path that set the
  guard — exactly the subtlety that defeats a quick read.
- **`/Od` & `/O2`:** `buf` is kept in a slot/register across `free`; the post-free `printf("%s", buf)`
  under `if (rc == 0)` is the UAF.
- **`/O2 /GS`:** identical to `/O2` — no cookie.

## UAF behavior
- Path-dependent: most inputs never hit it (they set `rc != 0` and skip the log). The empty-input path
  does. This is the kind of UAF that passes tests and review. Page Heap faults at the log's deref.

## Decompiler tells
- `free(buf); if (rc == 0) printf("%s", buf);` — a freed pointer used under a condition on the
  cleanup tail.
