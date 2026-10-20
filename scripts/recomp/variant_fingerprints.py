#!/usr/bin/env python3
"""Fingerprints of the functions the title code names by US address.

The seeds (src/game/jsrf/recomp/seed_*.json), xdk_symbols_extra.json, the
decompiled sources (`sub_XXXXXXXX`) and the title profile's XDK internals
name functions by their address in the US build. The Japanese build has the
same code at the same addresses (docs/analysis/language.md section 10), so a
Japanese lift shares all of them. This module checks that before lifting.

A fingerprint is a SHA-256 of a function's first 12 instructions with the
relative branch targets and absolute image addresses zeroed, so relocated
data does not change it. Only the hashes are committed
(src/game/jsrf/recomp/function_fingerprints.json), never the code bytes, and
a build can be checked without the US XBE at hand.

Usage:
    python scripts/recomp/variant_fingerprints.py --write [--xbe <US XBE>]
    python scripts/recomp/variant_fingerprints.py [--xbe <XBE>]      # check

--write needs the US build (by MD5); rerun it after adding a seed, an extra
symbol or a decompiled function. regenerate.py runs the check for every
variant other than the US one.
"""

import argparse
import hashlib
import json
import pathlib
import re
import struct
import sys

from xbe_variant import identify_xbe

ROOT = pathlib.Path(__file__).resolve().parents[2]
TITLE_RECOMP_DIR = ROOT / "src" / "game" / "jsrf" / "recomp"
DECOMP_DIR = ROOT / "src" / "game" / "jsrf" / "decomp"
PROFILE = ROOT / "src" / "game" / "jsrf" / "jsrf_profile.cpp"
FINGERPRINTS = TITLE_RECOMP_DIR / "function_fingerprints.json"
DEFAULT_XBE = ROOT / "roms" / "Jet Set Radio Future" / "default.xbe"
INSTRUCTIONS = 12
IMAGE_LO, IMAGE_HI = 0x10000, 0x290000


def named_functions():
    """Address -> where the title code names it."""
    functions = {}
    for path in sorted(TITLE_RECOMP_DIR.glob("seed_*.json")):
        for entry in json.loads(path.read_text(encoding="utf-8")):
            functions[int(entry["start"], 16)] = path.name
    extra = TITLE_RECOMP_DIR / "xdk_symbols_extra.json"
    for entry in json.loads(extra.read_text(encoding="utf-8"))["symbols"]:
        functions[int(entry["address"], 16)] = extra.name
    for path in sorted(DECOMP_DIR.rglob("*.c")):
        for match in re.finditer(r"\bsub_([0-9A-F]{8})", path.read_text(encoding="utf-8")):
            functions[int(match.group(1), 16)] = f"decomp/{path.name}"
    for match in re.finditer(r'\{"(\w+)", 0x([0-9A-Fa-f]{8})\}', PROFILE.read_text(encoding="utf-8")):
        functions[int(match.group(2), 16)] = f"{PROFILE.name} ({match.group(1)})"
    return functions


class Image:
    def __init__(self, path):
        self.data = pathlib.Path(path).read_bytes()
        base = struct.unpack_from("<I", self.data, 0x104)[0]
        count = struct.unpack_from("<I", self.data, 0x11C)[0]
        headers = struct.unpack_from("<I", self.data, 0x120)[0] - base
        self.sections = [struct.unpack_from("<6I", self.data, headers + i * 56)[1:5] for i in range(count)]

    def read(self, va, size):
        for start, _, raw, raw_size in self.sections:
            if start <= va < start + raw_size:
                return self.data[raw + va - start:raw + min(va - start + size, raw_size)]
        return None


def fingerprint(image, va, disassembler):
    """SHA-256 of the masked first instructions at `va`, or None if there is no code."""
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM
    code = image.read(va, 15 * INSTRUCTIONS)
    if not code:
        return None
    masked = bytearray(code)
    used = 0
    for index, insn in enumerate(disassembler.disasm(code, va)):
        if index >= INSTRUCTIONS:
            break
        offset = insn.address - va
        if insn.mnemonic.startswith("j") or insn.mnemonic in ("call", "loop", "jecxz"):
            if insn.size >= 5:
                masked[offset + insn.size - 4:offset + insn.size] = bytes(4)
            elif insn.size == 2:
                masked[offset + 1] = 0
        for operand in insn.operands:
            if (operand.type == X86_OP_IMM and insn.imm_size == 4
                    and IMAGE_LO <= (operand.imm & 0xFFFFFFFF) < IMAGE_HI):
                masked[offset + insn.imm_offset:offset + insn.imm_offset + 4] = bytes(4)
            if (operand.type == X86_OP_MEM and insn.disp_size == 4
                    and IMAGE_LO <= (operand.mem.disp & 0xFFFFFFFF) < IMAGE_HI):
                masked[offset + insn.disp_offset:offset + insn.disp_offset + 4] = bytes(4)
        used = offset + insn.size
    if used == 0:
        return None
    return hashlib.sha256(bytes(masked[:used])).hexdigest()


def disassembler():
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    except ImportError:
        sys.exit("capstone is required (pip install capstone)")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    return md


def write(xbe):
    variant, detail = identify_xbe(xbe)
    if variant != "jsrf_us":
        sys.exit(f"error: --write needs the USA build: {detail if variant is None else f'{xbe} is {detail}'}")
    image, md = Image(xbe), disassembler()
    table = {}
    for va, source in sorted(named_functions().items()):
        value = fingerprint(image, va, md)
        if value is None:
            sys.exit(f"error: no code at 0x{va:08X} ({source})")
        table[f"0x{va:08X}"] = value
    document = {
        "comment": "Generated by scripts/recomp/variant_fingerprints.py --write from the USA build. "
                   "SHA-256 of each named function's first instructions with branch targets and image "
                   "addresses zeroed; hashes only, no code bytes.",
        "instructions": INSTRUCTIONS,
        "functions": table,
    }
    FINGERPRINTS.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {len(table)} fingerprint(s) to {FINGERPRINTS.relative_to(ROOT)}")


def check(xbe):
    """Returns a list of problems; empty when every named function matches."""
    if not FINGERPRINTS.is_file():
        return [f"{FINGERPRINTS.relative_to(ROOT)} is missing; run variant_fingerprints.py --write with the USA build"]
    table = json.loads(FINGERPRINTS.read_text(encoding="utf-8"))["functions"]
    image, md = Image(xbe), disassembler()
    problems = []
    for va, source in sorted(named_functions().items()):
        expected = table.get(f"0x{va:08X}")
        if expected is None:
            problems.append(f"0x{va:08X} ({source}) has no fingerprint; rerun --write with the USA build")
        elif fingerprint(image, va, md) != expected:
            problems.append(f"0x{va:08X} ({source}) does not hold the USA code in this build")
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--xbe", type=pathlib.Path, default=DEFAULT_XBE)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    if args.write:
        write(args.xbe)
        return 0
    variant, detail = identify_xbe(args.xbe)
    if variant is None:
        print(f"error: {detail}", file=sys.stderr)
        return 2
    problems = check(args.xbe)
    for problem in problems:
        print(f"  {problem}")
    print(f"{detail}: {len(named_functions()) - len(problems)} of {len(named_functions())} named function(s) match")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
