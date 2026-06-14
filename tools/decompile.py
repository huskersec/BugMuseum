#!/usr/bin/env python3
"""
Best-effort decompilation helper for the Bug Museum (Windows / MSVC targets).

For a given variant directory, this links each build into a small PE, loads it
with angr, decompiles the `vuln` function, and writes dec_O0.c / dec_O2.c /
dec_O2_gs.c. The hand-authored dec_*.c committed to the repo are the reference;
this script regenerates them from real binaries when you want to verify.

USAGE
    python tools/decompile.py <variant_dir> [<variant_dir> ...]
    # or, via the build script:
    powershell -File build.ps1 -Decompile

REQUIREMENTS
    - cl.exe / link.exe on PATH (x64 Native Tools Command Prompt)
    - python -m pip install angr
    angr's decompiler output and API have shifted across versions; treat this as
    a convenience, not a guarantee. If it can't decompile, it falls back to angr's
    function summary so you still get *something* to diff against.

NOTE
    angr works on linked PEs, not .obj files, so this builds tiny .exe targets
    (with main from src.c). Symbols are kept (/Zi /DEBUG) so `vuln` is findable.
"""
import os
import sys
import subprocess

BUILDS = [
    ("dec_O0.c",    ["/Od", "/GS-"]),
    ("dec_O2.c",    ["/O2", "/GS-"]),
    ("dec_O2_gs.c", ["/O2", "/GS"]),
]


def build_exe(src_dir, flags, exe):
    """Compile+link src.c in src_dir into exe (with symbols). Returns path or None."""
    cmd = ["cl.exe", "/nologo", "/Zi"] + flags + ["src.c", "/Fe:" + exe,
           "/link", "/DEBUG", "/INCREMENTAL:NO"]
    r = subprocess.run(cmd, cwd=src_dir, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(f"[!] cl failed ({' '.join(flags)}):\n{r.stdout}\n{r.stderr}\n")
        return None
    return os.path.join(src_dir, exe)


def decompile_vuln(exe_path):
    """Return pseudo-C for `vuln`, or a best-effort summary string."""
    import angr  # imported lazily so the build step doesn't require angr
    proj = angr.Project(exe_path, auto_load_libs=False)
    cfg = proj.analyses.CFGFast(normalize=True, show_progressbar=False)

    func = None
    for f in cfg.kb.functions.values():
        if f.name == "vuln" or f.name.endswith("!vuln"):
            func = f
            break
    if func is None:
        return "/* vuln not found in CFG; symbols stripped? */\n"

    try:
        dec = proj.analyses.Decompiler(func, cfg=cfg.model)
        if dec.codegen and dec.codegen.text:
            return dec.codegen.text
    except Exception as e:  # noqa: BLE001 - angr version drift is expected
        sys.stderr.write(f"[!] decompiler failed, falling back: {e}\n")

    # Fallback: a disassembly-ish summary so the file isn't empty.
    block_lines = []
    for addr in sorted(func.block_addrs):
        b = proj.factory.block(addr)
        block_lines.append(b.capstone.insns and str(b.capstone) or "")
    return "/* angr decompiler unavailable; raw blocks below */\n/*\n" + \
           "\n".join(block_lines) + "\n*/\n"


def process(variant_dir):
    if not os.path.isfile(os.path.join(variant_dir, "src.c")):
        sys.stderr.write(f"[skip] no src.c in {variant_dir}\n")
        return
    print(f"== {os.path.basename(variant_dir.rstrip(os.sep))} ==")
    for out_name, flags in BUILDS:
        exe = "_dec_tmp.exe"
        exe_path = build_exe(variant_dir, flags, exe)
        if not exe_path:
            continue
        try:
            text = decompile_vuln(exe_path)
            header = f"/* angr decompilation  —  cl {' '.join(flags)} */\n\n"
            with open(os.path.join(variant_dir, out_name), "w", encoding="utf-8") as fh:
                fh.write(header + text.rstrip() + "\n")
            print(f"  wrote {out_name}")
        finally:
            for ext in (".exe", ".ilk", ".pdb", ".obj"):
                p = os.path.join(variant_dir, "_dec_tmp" + ext)
                if os.path.exists(p):
                    os.remove(p)


def main(argv):
    if len(argv) < 2:
        sys.stderr.write(__doc__)
        return 2
    for d in argv[1:]:
        process(os.path.abspath(d))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
