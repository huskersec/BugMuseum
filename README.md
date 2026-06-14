# Bug Museum

A curated collection of classic security bugs, each presented as an *exhibit* you can study from source all the way down to the metal.

The idea: take a textbook vulnerability, compile it at different optimization levels — and in different disguises — then lay everything side by side (source, assembly, decompilation) so you learn to **recognize the same bug across every layer of the stack**, whether you're reading clean C or staring at stripped, optimized disassembly.

## Platform & toolchain

This museum is **Windows-only**. Every exhibit targets **x64 PE** built with **MSVC (`cl.exe`)**, disassembled with **`dumpbin`**, and decompiled with an **angr / pypcode** pipeline. Mitigation flags (`/GS`, `/guard:cf`) and the idioms they emit (`__security_cookie`, `__security_check_cookie`) are the real Windows shapes you'll meet in the wild — not their Linux/GCC equivalents.

> The very first exhibit (`stack-overflow`) was originally built with mingw-GCC and is preserved as a cross-toolchain reference, because recognizing the *same* bug under MSVC vs GCC idioms is itself a skill. Everything else is MSVC.

Run [`build.ps1`](build.ps1) to regenerate authoritative asm/decompilation for any exhibit from a real `cl.exe` toolchain.

## The two axes

A bug class isn't one fixed string of code — it's a *property* that can wear many disguises and survive many transformations. The museum varies each bug along two independent axes:

- **Axis A — compilation & mitigation.** The *same source*, built three ways: `/Od` (debug), `/O2` (release), `/O2 /GS` (release + stack canary). Teaches how the idiom degrades, moves, and mutates as the compiler transforms it.
- **Axis B — source idiom.** The *same bug property*, written many ways: different copy primitives (`strcpy`, `sprintf`, `memcpy`, hand-rolled loops) and, more importantly, **"looks-safe-but-isn't"** disguises (a bound computed from the wrong value, `sizeof` on a pointer, an off-by-one check). Teaches that the bug is the property, not the function name.

Each *variant* (an Axis B point) carries the full Axis A set, so every exhibit is a small grid: idioms × build configurations.

## What's in each variant

| Artifact | File | What it shows |
| --- | --- | --- |
| **Source** | `src.c` | The original, annotated C |
| **Notes** | `notes.md` | Bug class (CWE), root cause, and per-build recognition signatures |
| **ASM — debug** | `asm_O0.txt` | `/Od` — the textbook shape |
| **ASM — release** | `asm_O2.txt` | `/O2`, no stack protector |
| **ASM — hardened** | `asm_O2_gs.txt` | `/O2 /GS` — with stack canary |
| **Decompilation** | `dec_O0.c`, `dec_O2.c`, `dec_O2_gs.c` | angr/pypcode pseudo-C for each build above |

## Exhibits

| Exhibit | Bug class | Description |
| --- | --- | --- |
| [stack-overflow](stack-overflow/) | CWE-121 — Stack-based Buffer Overflow | A fixed `buf[16]` overrun, built out along Axis B: 6 idioms from plain `strcpy` to subtle "looks-safe" disguises |

**Planned exhibits** (each with its own Axis A/B grid): heap overflow · integer-overflow → undersized allocation · signedness bug · use-after-free · double free · out-of-bounds read (info leak) · uninitialized memory use. These are the corruption-class bugs with sharp, teachable binary signatures. (Data-flow-to-sink bugs — command injection, SQLi, path traversal — compile to unremarkable asm and belong in a separate wing, if at all.)

## What this museum teaches — and what it can't

Be clear-eyed about scope. There are two separable skills in vulnerability identification, and this format only teaches one of them well.

**Idiom recognition (local, *taught here*).** "This function does an unbounded write into a fixed buffer." This is genuinely learnable and roughly fixed per bug class. An experienced auditor spots `strcpy(buf, attacker)` instantly, at any optimization level. The museum's whole job is to build this vocabulary — and to show how the idiom mutates across Axis A (compilation) and hides across Axis B (disguised source).

**Reachability & lifetime reasoning (global, *not taught here*).** "Is this input actually attacker-controlled and actually unbounded by the time it reaches this line, across the dozen functions and several files it traveled through?" In real audits this is ~90% of the effort, and **it does not live in the buggy function.** Its difficulty comes from sources a minimal exhibit cannot contain:

1. **Taint distance** — source (network/file/argv) and sink (the copy) are far apart, connected through call chains, callbacks, indirect calls, and vtables. The `strcpy` is obvious; proving its argument is reachable and unbounded is the work.
2. **"Looks-correct" guards** — the code *is* defensive, just wrong: an off-by-one check, a signed/unsigned mismatch, an upstream integer overflow that makes a sufficient-looking check pass. The bug is the *relationship* between a check in one place and a use in another. (Axis B gestures at this in miniature.)
3. **Lifetime & aliasing** — for UAF/double-free the bug isn't a shape you can point at; it's a *temporal* fact about who frees an object and who still holds a pointer, often across threads or re-entrancy. No single function contains it.
4. **State/path reachability** — the bug only fires after a specific sequence of operations puts the program in the right state.

Compiler transformation (Axis A) is a *fifth* source of hardness, but the smallest one for real-world auditing — it obscures the *local* idiom, while the dominant difficulty was never local to begin with.

**The honest bottom line:** a minimal exhibit teaches the *vocabulary* of a bug class — what each idiom looks like at every layer. It fundamentally **cannot** teach the *grammar* — tracing taint, verifying guards, reasoning about lifetimes across a whole program — because by construction it has no "rest of the program." Treat the museum as a place to build fast, reliable local recognition; treat real targets as where you learn the global skill that recognition is a prerequisite for.

## Who this is for

Anyone learning reverse engineering, exploit development, or vulnerability research on Windows who wants to connect "the bug in the code" to "what it actually looks like in a disassembler."
