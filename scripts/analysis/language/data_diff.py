"""Dword-level diff of the US and JP JSRF XBEs' data sections.

Each data section is read as a stream of dwords. A dword that lands in the
image (an address) is replaced by a token, so relocated pointers compare
equal; the streams are then aligned with difflib. What is left different
is a real change of an initial value.

usage: python -I data_diff.py <us.xbe> <jp.xbe> [section ...]
"""
import struct, sys, difflib


def load(path):
    x = open(path, "rb").read()
    base = struct.unpack_from("<I", x, 0x104)[0]
    n = struct.unpack_from("<I", x, 0x11C)[0]
    hdr = struct.unpack_from("<I", x, 0x120)[0] - base
    secs = {}
    for i in range(n):
        flags, va, vs, raw, rs, nva = struct.unpack_from("<6I", x, hdr + i * 56)
        name = x[nva - base:x.index(b"\0", nva - base)].decode()
        secs[name] = (va, vs, raw, rs, flags)
    return x, secs


LO, HI = 0x10000, 0x290000


def dwords(x, sec):
    va, vs, raw, rs, _ = sec
    n = rs // 4
    vals = struct.unpack_from(f"<{n}I", x, raw)
    toks = ["A" if LO <= v < HI else v for v in vals]
    return va, vals, toks


def fmt(v):
    f = struct.unpack("<f", struct.pack("<I", v))[0]
    s = f"{v:08X}"
    if 1e-4 < abs(f) < 1e7:
        s += f"({f:g})"
    return s


us, su = load(sys.argv[1])
jp, sj = load(sys.argv[2])
names = sys.argv[3:] or [n for n, s in su.items() if not (s[4] & 4) and n in sj]
for name in names:
    va_a, va_vals, ka = dwords(us, su[name])
    vb_b, vb_vals, kb = dwords(jp, sj[name])
    sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
    blocks = [o for o in sm.get_opcodes() if o[0] != "equal"]
    print(f"== {name}: US {len(ka)} dwords at {va_a:08X}, JP {len(kb)} at {vb_b:08X}, "
          f"differing blocks {len(blocks)}")
    for op, i1, i2, j1, j2 in blocks:
        print(f"  {op:7s} US {va_a + 4 * i1:08X}+{4 * (i2 - i1):X}  JP {vb_b + 4 * j1:08X}+{4 * (j2 - j1):X}")
        for k in range(min(max(i2 - i1, j2 - j1), 12)):
            a = fmt(va_vals[i1 + k]) if i1 + k < i2 else "-"
            b = fmt(vb_vals[j1 + k]) if j1 + k < j2 else "-"
            print(f"      {a:24s} {b}")
