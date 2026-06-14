# Variant 01 — `strcpy` (the baseline)

**Bug class:** CWE-121 (Stack-based Buffer Overflow)
**Idiom (Axis B):** plain `strcpy(buf, input)` — the canonical, undisguised case.
**Root cause:** `strcpy` copies until the source NUL with no destination bound; `buf[16]` overruns up the frame toward the saved return address.

```c
void vuln(const char *input) {
    char buf[16];
    strcpy(buf, input);   // no size — copies until input's NUL
}
```

This is the reference point for the whole exhibit. Every other variant is this same *property* (unbounded write into a fixed stack buffer) wearing a different disguise.

---

## Recognition signatures (x64 PE / MSVC)

### `/Od` debug — the textbook shape
- **No frame pointer.** MSVC `/Od` is rsp-relative; there is no `push rbp` (this differs from GCC `-O0`, which sets one up).
- First arg arrives in **RCX** (Win64 ABI), homed to the shadow slot `[rsp+8]`.
- `lea rcx,[rsp+0x20]` → **address of a stack local taken as dest**.
- `call strcpy` with **no size argument**.
> Fingerprint: `lea` of a stack local into arg0 + a copy primitive + no length = unbounded write.

### `/O2` release — no cookie
- Same `strcpy`, no bound. Argument stays in registers (no home/reload). The named `buf` is gone; the decompiler shows `v0` / Ghidra would show `local_28`.
> Lesson: same bug, reasoned in rsp offsets, not symbolic names. (A truly dead buffer can be eliminated at `/O2` — see `asm_O2.txt`.)

### `/O2 /GS` — the MSVC-default hardened shape
- `mov rax,[__security_cookie]; xor rax,rsp; mov [rsp+0x28],rax` → **cookie XOR rsp, stored above buf**.
- copy happens.
- `mov rcx,[rsp+0x28]; xor rcx,rsp; call __security_check_cookie` → **canary verified via a call**.
> Fingerprint: a guard load + `xor rsp` before the buffer op, and a reload + `xor rsp` + `call __security_check_cookie` after it.
> The cookie's mere presence is intel: the compiler decided this frame held an overflow-able buffer.

---

## MSVC vs GCC — recognize the pattern, not the names

The same bug looks different across toolchains. This variant ships both so you can train on the contrast:

| | MSVC (`/GS`, this dir) | mingw-GCC (`mingw-reference/`) |
| --- | --- | --- |
| Frame at `/Od` | rsp-relative, no `rbp` | `push rbp; mov rbp,rsp` |
| Cookie symbol | `__security_cookie` | `__stack_chk_guard` |
| Cookie transform | XOR with `rsp` | none (raw guard value) |
| Verify on exit | `call __security_check_cookie` | inline `sub; jne __stack_chk_fail` |

The *idiom shape* (load guard → use buffer → verify guard) is the stable, recognizable pattern across both. The symbol names and the verify mechanism differ.

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `/Od`:    `strcpy(&v0, a0)` — clean, dest is a stack local.
- `/O2`:    identical body, frame math gone.
- `/O2/GS`: the canary appears explicitly as `v1 = __security_cookie ^ (ulonglong)&v0; … __security_check_cookie(v1 ^ …)`.
