"""Where the US-specific addresses the title code names sit in the JP build.

Functions: every address in the seed files, xdk_symbols_extra.json, the
decompiled sources (`sub_XXXXXXXX`) and the profile is looked up in the JP
.text by its first 12 instructions, with relative branches and absolute image
addresses masked; the nearest match wins.

Data: every image address the decompiled sources name (0x1C0000-0x290000),
plus the language global, is found as a 4-byte operand in the US executable
sections; the JP bytes at the same virtual address give its JP counterpart
(the two builds' code is laid out the same, see the function result).

usage: python -I variant_addresses.py <us.xbe> <jp.xbe>
"""
import collections, glob, json, os, re, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
LO, HI = 0x10000, 0x290000


def load(path):
    x = open(path, "rb").read()
    base = struct.unpack_from("<I", x, 0x104)[0]
    n = struct.unpack_from("<I", x, 0x11C)[0]
    hdr = struct.unpack_from("<I", x, 0x120)[0] - base
    secs = []
    for i in range(n):
        flags, va, vs, raw, rs, nva = struct.unpack_from("<6I", x, hdr + i * 56)
        secs.append((x[nva - base:x.index(b"\0", nva - base)].decode(), va, vs, raw, rs, flags))
    return x, secs


def view(x, secs, va, n):
    for name, v, vs, raw, rs, flags in secs:
        if v <= va < v + rs:
            return x[raw + va - v:raw + va - v + n]
    return None


us, su = load(sys.argv[1])
jp, sj = load(sys.argv[2])
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True


def pattern(va, count=12):
    code = view(us, su, va, 96)
    mask = bytearray(b"\xff" * len(code))
    used = 0
    for k, ins in enumerate(md.disasm(code, va)):
        if k >= count:
            break
        off = ins.address - va
        if ins.mnemonic.startswith("j") or ins.mnemonic == "call":
            if ins.size >= 5:
                mask[off + ins.size - 4:off + ins.size] = b"\0\0\0\0"
            elif ins.size == 2:
                mask[off + 1] = 0
        for op in ins.operands:
            if op.type == X86_OP_IMM and ins.imm_size == 4 and LO <= (op.imm & 0xFFFFFFFF) < HI:
                mask[off + ins.imm_offset:off + ins.imm_offset + 4] = b"\0\0\0\0"
            if op.type == X86_OP_MEM and ins.disp_size == 4 and LO <= (op.mem.disp & 0xFFFFFFFF) < HI:
                mask[off + ins.disp_offset:off + ins.disp_offset + 4] = b"\0\0\0\0"
        used = off + ins.size
    return bytes(code[:used]), bytes(mask[:used])


def find_function(va, window=0x3000):
    pat, mask = pattern(va)
    for delta in sorted(range(-window, window), key=abs):
        cand = view(jp, sj, va + delta, len(pat))
        if cand and len(cand) == len(pat) and all((a ^ b) & m == 0 for a, b, m in zip(pat, cand, mask)):
            return va + delta
    return None


# Functions
recomp = os.path.join(ROOT, "src", "game", "jsrf", "recomp")
decomp = os.path.join(ROOT, "src", "game", "jsrf", "decomp")
functions = {}
for path in glob.glob(os.path.join(recomp, "seed_*.json")):
    for entry in json.load(open(path, encoding="utf-8")):
        functions[int(entry["start"], 16)] = os.path.basename(path)
for entry in json.load(open(os.path.join(recomp, "xdk_symbols_extra.json"), encoding="utf-8"))["symbols"]:
    functions[int(entry["address"], 16)] = "xdk_symbols_extra.json"
data = {0x1FC724: "language index (jsrf_profile.cpp)"}
for path in glob.glob(os.path.join(decomp, "*.c")):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r"sub_([0-9A-F]{8})", text):
        functions[int(m.group(1), 16)] = "decomp/" + os.path.basename(path)
    for m in re.finditer(r"0x(?:00)?([12][0-9A-Fa-f]{5})u?\b", text):
        value = int(m.group(1), 16)
        if 0x1C0000 <= value < HI and value not in functions:
            data.setdefault(value, "decomp/" + os.path.basename(path))
functions[0x1919C0] = "jsrf_profile.cpp (D3D_InitializeDeviceState)"

by_source = collections.defaultdict(collections.Counter)
for va, source in sorted(functions.items()):
    found = find_function(va)
    by_source[source]["missing" if found is None else f"{found - va:+#x}"] += 1
print(f"functions: {len(functions)}")
for source, counts in sorted(by_source.items()):
    print(f"  {source:45s} {dict(counts)}")

# Data
print(f"data: {len(data)}")
for value, source in sorted(data.items()):
    pat = struct.pack("<I", value)
    deltas = collections.Counter()
    for name, va, vs, raw, rs, flags in su:
        if not flags & 4:  # executable sections only
            continue
        body = us[raw:raw + rs]
        i = body.find(pat)
        while i >= 0:
            other = view(jp, sj, va + i, 4)
            deltas[(name, struct.unpack("<I", other)[0] - value)] += 1
            i = body.find(pat, i + 1)
    shown = ", ".join(f"{name} {delta:+#x} x{count}" for (name, delta), count in deltas.most_common(3))
    print(f"  {value:#010x} {source:35s} {shown or 'no operand reference'}")
