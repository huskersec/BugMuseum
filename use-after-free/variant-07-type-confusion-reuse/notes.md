# Variant 07 — type-confusion reuse (UAF → indirect call)

**Bug class:** CWE-416 (Use After Free), reaching CWE-843 (Type Confusion) / control-flow hijack
**Shape (Axis B):** free an object with a function pointer; allocate a same-size object the attacker
fills; call through the dangling pointer.
**Why it's the capstone-tier one:** the previous variants corrupt or leak *data*. This one converts a
UAF into an **indirect call to an attacker-chosen address** — the bridge from memory-safety bug to
code execution.

```c
free(h);                              /* h dangling */
char *reuse = malloc(sizeof *h);      /* same size -> likely h's chunk */
memcpy(reuse, input, ...);            /* attacker controls offset 0 = the fn pointer */
h->fn(h->name);                       /* call through the dangling, now-attacker, pointer */
```

The reuse is what makes it exploitable: free A, allocate B of the same size class, and B's bytes are
read back through A's stale type — here A's `fn` field is now whatever B (the attacker) wrote.

---

## Recognition signatures (x64 PE / MSVC)
- **The tell:** `free(h)` → `malloc(<same size>)` → fill → an **indirect call** through the freed
  object: `mov rax,[h]; ... call rax` (load a function pointer out of a freed-then-reused chunk and
  call it). The `free`→same-size-`malloc`→`call [freed]` sequence is the fingerprint.
- **`/Od` & `/O2`:** `h` is held across `free`; the call target is loaded from `[h]` (offset 0) after
  the chunk was reused.
- **`/O2 /GS`:** identical to `/O2` — no cookie. **This is the one variant where `/guard:cf` matters:**
  with CFG (the capstone's `o2gscf` build), the `call rax` is preceded by a guard check that faults on
  a target that isn't a registered valid call destination — so attacker bytes can't redirect it to
  arbitrary code (though CFG does not fix the UAF itself).

## UAF behavior
- Deterministic-ish reuse: a same-size `malloc` right after the `free` very often returns the same
  chunk (especially in the LFH bucket), so `h->fn` reads the attacker's first 8 bytes. Under Page Heap
  the freed page isn't reused and the load from `[h]` faults instead — which is the point of Page Heap.

## Decompiler tells
- `free(h); reuse = malloc(32); memcpy(reuse, input, ...); h->fn(h->name);` — an indirect call whose
  target is a field of an object freed two lines earlier and since reallocated.
