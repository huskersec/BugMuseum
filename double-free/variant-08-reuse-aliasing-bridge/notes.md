# Variant 08 — reuse aliasing bridge (the exploitation payoff)

**Bug class:** CWE-415 (Double Free) → controlled indirect call
**Shape (Axis B):** double-free a chunk, then two same-size `malloc`s are handed the *same* chunk; two
"distinct" objects overlap, and writes through one set the function pointer the other calls.
**Root cause:** a chunk on the freelist twice can be allocated twice. After `free(p); free(p);`, the next
two equal-size allocations (`h` and `alias`) may both be backed by that chunk. `h` and `alias` now alias
the same memory: `memcpy(alias, input, ...)` lands attacker bytes over `h->fn` (the first member), and
`h->fn()` then jumps to an attacker-controlled address.

```c
char *p = malloc(sizeof(Handler)); free(p); free(p);   /* on the freelist twice */
Handler *h     = malloc(sizeof(Handler));              /* may be the same chunk... */
char    *alias = malloc(sizeof(Handler));              /* ...as this */
h->fn = legit;
memcpy(alias, input, sizeof(Handler));   /* attacker bytes over h->fn */
h->fn();                                 /* indirect call to attacker address */
```

This is the double-free counterpart of UAF variant 07 (type-confusion-reuse): both turn a temporal bug
into a corrupted **indirect-call target**, and both are where `/guard:cf` finally matters. The lesson the
whole exhibit builds toward — a double-free is not "just" a crash; it is a heap-aliasing primitive.

---

## Recognition signatures (x64 PE / MSVC)
- **`/Od`:** `call malloc; ...; call free; call free` (the double free), two more `call malloc` into
  separate slots, a store of `legit`'s address into `[h+0]`, a `memcpy`/`rep movs` into `alias`, then the
  payoff: `mov rax, [h]; call rax` — an **indirect call through the freed-and-reused chunk's first 8
  bytes**.
- **`/O2`:** allocations and frees keep their order (the compiler can't reorder across `malloc`/`free`);
  the indirect call appears as `call qword ptr [reg]` where `reg` holds `h`. `h->fn = legit` may be a
  direct store of the function's address.
- **`/O2 /GS`:** identical to plain `/O2` — **no cookie** (no stack array to guard).
- **`/O2 /GS /guard:cf`** (the capstone's `o2gscf` build): the indirect call is wrapped by the CFG check —
  `mov rax, [h]; mov rcx, rax; call __guard_dispatch_icall` (or `call [__guard_check_icall_fptr]`). CFG
  validates the target against the bitmap of legal function entries; an overwritten `h->fn` that is not a
  registered entry triggers a fast-fail (`STATUS_STACK_BUFFER_OVERRUN` / CFG violation) **before** the
  jump. This is the only double-free variant `/guard:cf` defends — and only the *call*, not the
  underlying heap corruption.

## Double-free / exploitation behavior — how to observe it
- **Non-deterministic by design.** Whether `h` and `alias` alias depends on the allocator: LFH/segment-heap
  bucket state, quarantine/delayed-free, and prior allocations all matter. When they alias, supplying 8
  bytes of address in `argv[1]` steers `h->fn()`. When they don't, `h->fn` stays `legit` and the call is
  harmless — re-run / vary input length to change heap state.
- **The defenses fire first, in order:** under **Page Heap** the *second free* faults immediately (freed
  page protected). Under the **segment heap / LFH** double-free detection, the second free may raise
  `STATUS_HEAP_CORRUPTION` (`__fastfail`). With **`/guard:cf`**, even if the overlap happens, the indirect
  call is rejected. Only an unhardened release build (`/MD`, no Page Heap, CFG off) reaches the
  attacker-controlled `call`. That layering *is* the lesson.
- For a clean demo of the corruption, pass an 8-byte little-endian address as the first bytes of
  `argv[1]`; expect either a jump to that address (success) or one of the fast-fails above (a defense
  caught it).

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `free(p); free(p);` followed by two `malloc` of equal size and an indirect call `(*h->fn)()` whose
  target was written via a *different* pointer. The decompiler shows the call going through a struct field
  at offset 0 that an aliasing write reached.
- `/guard:cf`: the indirect call is replaced by a `__guard_dispatch_icall` thunk — the visible marker that
  CFG is in play on this site.
