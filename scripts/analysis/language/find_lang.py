"""Locate the language machinery of the US JSRF XBE.

Finds the language-suffixed asset strings, the pointer tables that hold
them, and the code that indexes those tables. Reads the user's XBE and the
local disassembly output (gen/jsrf_us/disasm), never writes either.

usage (from the repo root): python -I find_lang.py <xbe>
"""
import json, re, struct, sys, bisect

xbe = open(sys.argv[1], "rb").read()
base = struct.unpack_from("<I", xbe, 0x104)[0]
n = struct.unpack_from("<I", xbe, 0x11C)[0]
hdr = struct.unpack_from("<I", xbe, 0x120)[0] - base
secs = []
for i in range(n):
    flags, va, vs, raw, rs, name_va = struct.unpack_from("<6I", xbe, hdr + i * 56)
    name = xbe[name_va - base:xbe.index(b"\0", name_va - base)].decode()
    secs.append((name, va, vs, raw, rs))


def va2off(va):
    for name, sva, vs, raw, rs in secs:
        if sva <= va < sva + rs:
            return raw + va - sva
    return None


def off2va(off):
    for name, sva, vs, raw, rs in secs:
        if raw <= off < raw + rs:
            return sva + off - raw, name
    return None, None


def cstr(va):
    o = va2off(va)
    if o is None:
        return None
    e = xbe.index(b"\0", o)
    return xbe[o:e].decode("latin-1")


# 1. language-suffixed strings
lang_re = re.compile(rb"[\x20-\x7e]*(?:_(?:eng|jpn|frn|ger|spa|ita)|_(?:Eng|Jpn|Frn|Ger|Spa|Ita))[\x20-\x7e]*\x00")
strs = {}
for m in lang_re.finditer(xbe):
    va, sec = off2va(m.start())
    strs[va] = m.group()[:-1].decode()
print(f"{len(strs)} language-suffixed strings")
for va, s in sorted(strs.items())[:40]:
    print(f"  {va:08X} {s}")

# 2. pointer tables: dwords anywhere that equal one of those string addresses
refs = {}
for name, sva, vs, raw, rs in secs:
    data = xbe[raw:raw + rs]
    for off in range(0, len(data) - 3, 4):
        v = struct.unpack_from("<I", data, off)[0]
        if v in strs:
            refs.setdefault(v, []).append((sva + off, name))
print("\n== dword references to those strings (tables) ==")
tables = {}
for v, where in sorted(refs.items()):
    for w, sec in where:
        tables.setdefault(w & ~0x3, []).append(v)
        print(f"  {w:08X} {sec:6s} -> {v:08X} {strs[v]}")

# 3. code xrefs from the disassembly
xr = json.load(open("gen/jsrf_us/disasm/xrefs.json"))
funcs = json.load(open("gen/jsrf_us/disasm/functions.json"))
fb = sorted((int(f["start"], 16), int(f["end"], 16)) for f in funcs)
fs = [a for a, _ in fb]


def owner(a):
    i = bisect.bisect_right(fs, a) - 1
    return fs[i] if i >= 0 and a < fb[i][1] else None


targets = set(strs) | {w for v in refs for w, _ in refs[v]}
# also the first slot of each contiguous run of table words
print("\n== code references to the strings or table slots ==")
seen = set()
for r in xr:
    t = int(r["to"], 16)
    if t in targets or any(0 <= t - w < 0x40 for w in targets if w not in strs):
        f = int(r["from"], 16)
        key = (f, t)
        if key in seen:
            continue
        seen.add(key)
        o = owner(f)
        print(f"  {f:08X} (in {o:08X}) -> {t:08X} {r['type']} {strs.get(t, '')}" if o else
              f"  {f:08X} (no func) -> {t:08X} {r['type']}")
