#!/usr/bin/env python3
"""Regenerate the lifted C for one XBE variant.

Runs the xboxrecomp pipeline (third_party/xboxrecomp, pinned submodule) on the
user's own XBE and writes everything under gen/<variant>/, which is gitignored:
the generated code is derived from the game's bytes and is never committed.

    gen/<variant>/analysis.json      XBE layout
    gen/<variant>/xdk_symbols.json   XDK functions found by rexbox_xdk_symbols
    gen/<variant>/disasm/            functions, xrefs, strings
    gen/<variant>/func-id/           function classification
    gen/<variant>/abi/               calling conventions
    gen/<variant>/hle_overrides.c    overrides routing XDK functions to the HLE
    gen/<variant>/overrides.c        everything the lifter must not emit as is
    gen/<variant>/gen/               lifted C, built by CMake as rejsrf_recomp

Seeds: src/game/jsrf/recomp/*.json files holding a list of {"start": "0x..."}
are function entry points the disassembler misses; each is passed to it.

HLE overrides: rexbox_xdk_symbols (built from tools/) identifies the XDK
library functions in the XBE. For every function of a library named in
src/game/jsrf/recomp/hle_libraries.json, an override of its lifted symbol is
generated that enters the HLE (docs/design/20261012-i006-xdk-symbol-binding.md).
xdk_symbols_extra.json adds functions the database misses.

Decompiled overrides: every `void sub_XXXXXXXX(void)` defined under the
decompilation directories is handed to the lifter as --exclude-manual, so
direct calls and the generated dispatch table link to the hand-written
version. A file that also declares `extern void sub_XXXXXXXX_gen(void);` keeps
the lifted body under that name (docs/design/20261012-i002-recomp-pipeline.md).

Usage:
    python scripts/recomp/regenerate.py
    python scripts/recomp/regenerate.py --xbe <path> --variant jsrf_us --symbols-tool <exe>
"""

import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
XBOXRECOMP = ROOT / "third_party" / "xboxrecomp"
DEFAULT_XBE = ROOT / "roms" / "Jet Set Radio Future" / "default_eng.xbe"
OVERRIDE_DIRS = [ROOT / "src" / "game" / "jsrf" / "decomp"]
TITLE_RECOMP_DIR = ROOT / "src" / "game" / "jsrf" / "recomp"
HLE_LIBRARIES = TITLE_RECOMP_DIR / "hle_libraries.json"
EXTRA_SYMBOLS = TITLE_RECOMP_DIR / "xdk_symbols_extra.json"
SYMBOL_TOOL_CANDIDATES = [
    ROOT / "build" / "win32_x86" / "tools" / "Release" / "rexbox_xdk_symbols.exe",
    ROOT / "build" / "win32_x86" / "tools" / "Debug" / "rexbox_xdk_symbols.exe",
    ROOT / "build" / "tools" / "rexbox_xdk_symbols",
]


def run_step(name, args):
    print(f"=== {name}", flush=True)
    started = time.monotonic()
    result = subprocess.run([sys.executable, "-m"] + [str(a) for a in args], cwd=XBOXRECOMP)
    if result.returncode != 0:
        sys.exit(f"{name} failed with exit code {result.returncode}")
    print(f"    {name}: {time.monotonic() - started:.1f}s", flush=True)


def seed_files():
    """Seed files are the JSON lists in the title's recomp directory."""
    seeds = []
    for path in sorted(TITLE_RECOMP_DIR.glob("*.json")) if TITLE_RECOMP_DIR.is_dir() else []:
        if isinstance(json.loads(path.read_text(encoding="utf-8")), list):
            seeds.append(path)
    return seeds


def find_symbol_tool(explicit):
    if explicit is not None:
        return explicit.resolve()
    for candidate in SYMBOL_TOOL_CANDIDATES:
        if candidate.is_file():
            return candidate
    sys.exit("rexbox_xdk_symbols not found -- build it first "
             "(scripts/build_win32_x86.ps1 -Target rexbox_xdk_symbols) or pass --symbols-tool")


def identify_symbols(tool, xbe, out_dir):
    print("=== xdk symbols", flush=True)
    target = out_dir / "xdk_symbols.json"
    result = subprocess.run([str(tool), str(xbe), "--json", str(target)], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"rexbox_xdk_symbols failed: {result.stderr.strip()}")
    symbols = json.loads(target.read_text(encoding="utf-8"))
    if EXTRA_SYMBOLS.is_file():
        known = {int(s["address"], 16) for s in symbols}
        for extra in json.loads(EXTRA_SYMBOLS.read_text(encoding="utf-8"))["symbols"]:
            if int(extra["address"], 16) not in known:
                symbols.append(dict(extra, call="unknown", params=0, build=0))
    print(f"    {len(symbols)} symbol(s)")
    return symbols


def write_hle_overrides(symbols, functions_json, out_dir):
    """One override per function of an HLE library that the disassembler found."""
    config = json.loads(HLE_LIBRARIES.read_text(encoding="utf-8")) if HLE_LIBRARIES.is_file() else {}
    libraries = set(config.get("libraries", []))
    single = {(f["library"], f["name"]) for f in config.get("functions", [])}
    starts = {int(f["start"], 16) for f in json.loads(functions_json.read_text(encoding="utf-8"))}
    chosen = {}
    skipped = []
    for symbol in symbols:
        if symbol["kind"] != "function":
            continue
        if symbol["library"] not in libraries and (symbol["library"], symbol["name"]) not in single:
            continue
        address = int(symbol["address"], 16)
        if address not in starts:
            skipped.append(f"{symbol['library']} {symbol['name']} 0x{address:08X}")
            continue
        chosen.setdefault(address, symbol)  # first name wins for aliases

    lines = [
        "/* Generated by scripts/recomp/regenerate.py from xdk_symbols.json. Not committed.",
        " * Each function of an HLE library enters rexbox_hle_enter; the lifted body",
        " * stays available as sub_X_gen. */",
        "",
        "#include \"rexbox/recomp/recomp_hle_entry.h\"",
        "",
    ]
    for address, symbol in sorted(chosen.items()):
        tag = f"{address:08X}"
        lines += [
            f"extern void sub_{tag}_gen(void);",
            f"static RexboxHleSlot slot_{tag} = {{0x{tag}u, \"{symbol['library']}\", \"{symbol['name']}\", sub_{tag}_gen, 0, 0}};",
            f"void sub_{tag}(void)",
            "{",
            f"    rexbox_hle_enter(&slot_{tag});",
            "}",
            "",
        ]
    # Identified XDK variables, for HLE modules that receive their addresses
    # (XInput's device types are &g_DeviceType_*). Absolute-looking values below
    # the image are structure offsets the database reports as variables; they
    # are kept, the binding is by name.
    variables = sorted({(s["library"], s["name"], int(s["address"], 16))
                        for s in symbols if s["kind"] == "variable"}, key=lambda v: (v[2], v[1]))
    lines += [
        "const RexboxXdkVariable rexbox_xdk_variables[] = {",
    ] + [f"    {{\"{lib}\", \"{name}\", 0x{addr:08X}u}}," for lib, name, addr in variables] + [
        "    {0, 0, 0},",
        "};",
        "",
    ]
    missing = sorted(single - {(s["library"], s["name"]) for s in chosen.values()})
    for library, name in missing:
        skipped.append(f"{library} {name} (selected function not identified)")
    target = out_dir / "hle_overrides.c"
    target.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"    hle overrides: {len(chosen)} function(s) of {sorted(libraries)} and {len(single)} selected; "
          f"{len(skipped)} skipped; {len(variables)} XDK variable(s)")
    for item in skipped:
        print(f"      skipped {item}")
    return target


def collect_overrides(out_dir, hle_overrides):
    """Concatenate the HLE overrides and every decompiled source for the lifter's scan."""
    sources = sorted(p for d in OVERRIDE_DIRS if d.is_dir() for p in d.rglob("*.c"))
    target = out_dir / "overrides.c"
    with target.open("w", encoding="utf-8", newline="\n") as out:
        out.write("/* Generated by scripts/recomp/regenerate.py: scan input only, not compiled. */\n")
        for source in [hle_overrides] + sources:
            label = source.relative_to(ROOT).as_posix()
            out.write(f"\n/* ---- {label} ---- */\n")
            out.write(source.read_text(encoding="utf-8"))
    print(f"    overrides: HLE + {len(sources)} decompiled source file(s)")
    return target


def check_submodule_clean():
    status = subprocess.run(["git", "-C", str(XBOXRECOMP), "status", "--porcelain"],
                            capture_output=True, text=True)
    if status.stdout.strip():
        print("WARNING: the pipeline left files inside third_party/xboxrecomp:", file=sys.stderr)
        print(status.stdout, file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--xbe", type=pathlib.Path, default=DEFAULT_XBE)
    parser.add_argument("--variant", default="jsrf_us")
    parser.add_argument("--symbols-tool", type=pathlib.Path, default=None,
                        help="path to rexbox_xdk_symbols (default: look in build/)")
    parser.add_argument("--split", type=int, default=250,
                        help="functions per generated file (smaller files compile in parallel)")
    args = parser.parse_args()

    xbe = args.xbe.resolve()
    if not xbe.is_file():
        sys.exit(f"no XBE at {xbe} -- put your own dump under roms/ (docs/guides/)")
    if not (XBOXRECOMP / "tools").is_dir():
        sys.exit("third_party/xboxrecomp is empty -- run: git submodule update --init")
    tool = find_symbol_tool(args.symbols_tool)

    out = (ROOT / "gen" / args.variant).resolve()
    gen_dir = out / "gen"
    out.mkdir(parents=True, exist_ok=True)
    # Stale split files from a run with a different function count would
    # otherwise be globbed into the build.
    if gen_dir.exists():
        shutil.rmtree(gen_dir)

    symbols = identify_symbols(tool, xbe, out)
    run_step("parse", ["tools.xbe_parser", xbe, "--json", out / "analysis.json", "--quiet"])
    seeds = seed_files()
    seed_args = [arg for seed in seeds for arg in ("--seed-functions", seed)]
    print(f"    seeds: {len(seeds)} file(s)")
    run_step("disasm", ["tools.disasm", xbe, "--analysis-json", out / "analysis.json",
                        "--output", out / "disasm", "--force"] + seed_args)
    run_step("func_id", ["tools.func_id", xbe,
                         "--functions", out / "disasm" / "functions.json",
                         "--strings", out / "disasm" / "strings.json",
                         "--xrefs", out / "disasm" / "xrefs.json",
                         "--output", out / "func-id"])
    run_step("abi", ["tools.abi_analysis", xbe, "--disasm-dir", out / "disasm",
                     "--func-id-dir", out / "func-id", "--output-dir", out / "abi"])

    hle_overrides = write_hle_overrides(symbols, out / "disasm" / "functions.json", out)
    overrides = collect_overrides(out, hle_overrides)
    run_step("lift", ["tools.recomp", xbe, "--all", "--split", args.split,
                      "--disasm-dir", out / "disasm", "--func-id-dir", out / "func-id",
                      "--abi-dir", out / "abi", "--gen-dir", gen_dir, "--output-dir", out / "recomp-summary",
                      "--exclude-manual", overrides])
    # The overrides are compiled with the generated code they replace.
    shutil.copyfile(hle_overrides, gen_dir / "rexbox_hle_overrides.c")

    check_submodule_clean()
    print(f"done: {gen_dir}")


if __name__ == "__main__":
    main()
