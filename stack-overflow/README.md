# Exhibit: stack-overflow-1 — CWE-121 Stack Buffer Overflow

One bug class, studied as a grid. Every variant is the **same property** — an unbounded (or wrongly-bounded) write into a fixed stack buffer that can reach the saved return address — wearing a different disguise, and each is built three ways.

## The grid

**Axis B (rows — source idiom):** how the bug is written. From the undisguised baseline to defects that look defensive.
**Axis A (columns — build):** how it's compiled. `/Od` debug · `/O2` release · `/O2 /GS` hardened.

| # | Variant | The disguise | Sharpest binary tell |
| --- | --- | --- | --- |
| [01](variant-01-strcpy/) | `strcpy(buf, input)` | none — the baseline | `lea` of a stack local into arg0 + a copy with no length |
| [02](variant-02-sprintf/) | `sprintf(buf, "%s", input)` | "formatted output" feels deliberate | printf-family call, dest is a stack local, format is `"%s"` |
| [03](variant-03-memcpy-attacker-len/) | `memcpy(buf, input, len)` | it *has* a size argument | size register traces to a parameter, no `cmp ...,10h` |
| [04](variant-04-strncpy-srclen/) | `strncpy(buf, input, strlen(input))` | the "safe" `strncpy` | count is `strlen(source)`; `sizeof(dest)` never appears |
| [05](variant-05-sizeof-vs-count/) | `wcsncpy(buf, input, sizeof(buf))` | it *uses* `sizeof(buf)` | count constant `20h` (bytes) for a `wchar_t[16]`; want `_countof` |
| [06](variant-06-off-by-one/) | manual loop, `i <= 16` | no dangerous function at all | `cmp i,10h` + `jbe` (≤) not `jb` (<); at `/O2`, a `16 + 1` store |

Read top-to-bottom and the lesson lands: **the bug is not a function name.** It's a property that survives every primitive and hides behind code that looks careful. Variants 03–06 in particular are the "looks-correct guard" shapes — the bug lives in a size's *provenance*, *units*, or *predicate*, not in any single suspicious call.

## Files in each variant

| File | What it is |
| --- | --- |
| `src.c` | annotated source |
| `notes.md` | root cause + per-build recognition signatures |
| `asm_O0.txt` / `asm_O2.txt` / `asm_O2_gs.txt` | MSVC `dumpbin /disasm:nobytes` of `vuln`, three builds |
| `dec_O0.c` / `dec_O2.c` / `dec_O2_gs.c` | angr/pypcode pseudo-C for each build |

> `variant-01-strcpy/mingw-reference/` preserves the original mingw-GCC artifacts beside the MSVC ones, so you can train on the MSVC-vs-GCC idiom contrast (`__security_cookie` + `xor rsp` + `call __security_check_cookie` vs `__stack_chk_guard` + inline `sub; jne __stack_chk_fail`). See that variant's `notes.md` for the table.

## A note on the builds

MSVC enables `/GS` **by default**, so the "release, no cookie" column is built with `/GS-` to expose the raw idiom; the hardened column uses `/GS`. The artifacts here are idiomatically faithful but hand-authored — run [`../build.ps1`](../build.ps1) from an *x64 Native Tools Command Prompt* to regenerate byte-exact asm (and, optionally, decompilation) from a real `cl.exe`.

## Caveat: this teaches the *vocabulary*, not the *grammar*

Each variant is a 3-line function, so it trains fast, reliable **local recognition** — the necessary first skill. It cannot teach **reachability and lifetime reasoning** (is the input actually attacker-controlled and unbounded by the time it arrives, across a whole program?), which is where real audits spend most of their effort. See the root [README](../README.md#what-this-museum-teaches--and-what-it-cant). The planned **capstone** for this module is a single, realistic application that scatters these six idioms across genuine functionality — the bridge from vocabulary to grammar.
