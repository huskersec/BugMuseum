# Stack Buffer Overflow

**Bug class:** CWE-121 (Stack-based Buffer Overflow)
**Mirrors:** classic SEH/return-address overwrite targets (OSED stack-smash work)
**Root cause:** `strcpy` copies until the source NUL with no destination-size bound; `buf[16]` overruns up the frame toward saved RBP / return address.

---

## Recognition signatures (x64 PE / mingw-GCC)

### Debug (-O0) — the textbook shape
- Frame pointer set up: `push rbp; mov rbp,rsp`
- First arg arrives in **RCX** (Win64 ABI), spilled to home space `[rbp+0x10]`
- `lea rax,[rbp-0x10]` → **address of a stack local taken as dest**
- `mov rcx,rax` → dest in arg0
- `call strcpy` with **no size argument**
> Fingerprint: `lea`+`<copy primitive>`+`no length` = unbounded write into a stack local.

### Release (-O2, no cookie)
- Frame pointer omitted; local is now **RSP-relative** (`lea rcx,[rsp+0x20]`)
- Same `strcpy`, no bound. Named `buf` is gone — decompiler shows `v0` / Ghidra would show `local_28`.
> Lesson: the same bug, but you now reason in RSP offsets, not RBP. No symbolic local name.

### Release (-O2 + /GS cookie) — the MSVC-default shape
- `mov rbx,[__stack_chk_guard]; mov rax,[rbx]; mov [rsp+0x38],rax` → **cookie loaded onto frame**
- copy happens
- `mov rax,[rsp+0x38]; sub rax,[rbx]; jne fail` → **cookie checked on exit**
> Fingerprint: a load-from-guard before the buffer op + a compare-and-`jne`-to-`__stack_chk_fail` after it.
> Presence of the cookie is itself intel: the compiler decided this frame held an overflow-able buffer.

---

## Decompiler tells (angr/pypcode ≈ Ghidra)
- `-O0`:   `strcpy(&v0, a0)` — clean, dest is a stack local.
- `-O2`:   identical body, frame math gone.
- `-O2+GS`: the canary appears explicitly as `v1 = __stack_chk_guard … if (v1 != __stack_chk_guard) __stack_chk_fail()`.
  In **Ghidra** this typically renders as `local_10 = __security_cookie ^ (ulonglong)&local_28;` style on MSVC, and the locals are named `local_NN`. The structure (load guard → use buffer → verify guard) is the stable, recognizable pattern across both.

## Note on real MSVC vs mingw
mingw-GCC uses `__stack_chk_guard` / `__stack_chk_fail`. Real MSVC `/GS` uses `__security_cookie`, `__security_check_cookie`, and XORs the cookie with RSP. The *idiom shape* is the same; the symbol names differ. Recognize the pattern, not the names.
