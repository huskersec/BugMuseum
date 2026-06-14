# Bug Museum

A small collection of classic security bugs, each presented as an *exhibit* you can study from source all the way down to the metal.

The idea: take a textbook vulnerability, compile it at different optimization levels, and lay everything side by side — source, assembly, and decompilation — so you can learn to **recognize the same bug across every layer of the stack**, whether you're reading clean C or staring at stripped, optimized disassembly.

## What's in each exhibit

Every bug ships with:

| Artifact | File | What it shows |
| --- | --- | --- |
| **Source** | `src.c` | The original, annotated C |
| **Notes** | `notes.md` | Bug class (CWE), root cause, and recognition signatures |
| **ASM — debug** | `asm_O0.txt` | Unoptimized (`-O0`) — the textbook shape |
| **ASM — release** | `asm_O2.txt` | Optimized (`-O2`), no stack protector |
| **ASM — hardened** | `asm_O2_gs.txt` | Optimized (`-O2`) with stack-canary (`/GS`) |
| **Decompilation** | `dec_O0.c`, `dec_O2.c`, `dec_O2_gs.c` | Decompiler output for each build above |

The goal is pattern recognition: the *same* bug looks different at `-O0` vs `-O2`, with and without compiler mitigations — but the underlying idiom stays the same once you learn to spot it.

## Exhibits

| Exhibit | Bug class | Description |
| --- | --- | --- |
| [stack-overflow-1](stack-overflow-1/) | CWE-121 — Stack-based Buffer Overflow | Classic unbounded `strcpy` into a fixed `buf[16]`, walking up the frame toward saved RBP / return address |

*More exhibits to come.*

## Who this is for

Anyone learning reverse engineering, exploit development, or vulnerability research who wants to connect "the bug in the code" to "what it actually looks like in a disassembler."
