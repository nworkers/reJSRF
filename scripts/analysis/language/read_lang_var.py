"""Read the language global's initial value in each XBE, and find the
matching global in the JP build by comparing the code that reads it.

usage: python -I read_lang_var.py <us.xbe> <jp.xbe>
"""
import struct, sys


def load(path):
    x = open(path, "rb").read()
    base = struct.unpack_from("<I", x, 0x104)[0]
    n = struct.unpack_from("<I", x, 0x11C)[0]
    hdr = struct.unpack_from("<I", x, 0x120)[0] - base
    secs = []
    for i in range(n):
        flags, va, vs, raw, rs, nva = struct.unpack_from("<6I", x, hdr + i * 56)
        name = x[nva - base:x.index(b"\0", nva - base)].decode()
        secs.append((name, va, vs, raw, rs))
    return x, secs


def rd32(x, secs, va):
    for name, sva, vs, raw, rs in secs:
        if sva <= va < sva + vs:
            if va < sva + rs:
                return struct.unpack_from("<I", x, raw + va - sva)[0], name
            return 0, name + "(bss)"
    return None, None


us, su = load(sys.argv[1])
jp, sj = load(sys.argv[2])
LANG_US = 0x1FC724
print("US [0x1FC724] =", rd32(us, su, LANG_US))

# In US, `mov ecx, 0x1fc724 ; call 0x43700` is b9 24 c7 1f 00 e8 ...
# Find each US site, then the JP bytes at the matching .text offset window
# to read JP's `mov ecx, imm32`.
pat_us = b"\xb9" + struct.pack("<I", LANG_US)
sites = []
i = -1
while True:
    i = us.find(pat_us, i + 1)
    if i < 0:
        break
    sites.append(i)
print("US sites:", len(sites))
from collections import Counter
cands = Counter()
for off in sites[:60]:
    ctx = us[off + 5:off + 12]          # the call that follows (rel32 differs) -> use bytes before
    before = us[off - 6:off]
    # search JP near the same offset (code moved by at most a few KiB)
    for d in range(-0x3000, 0x3000):
        j = off + d
        if jp[j - 6:j] == before and jp[j] == 0xB9:
            cands[struct.unpack_from("<I", jp, j + 1)[0]] += 1
            break
print("JP candidates for the language global:", [(hex(a), c) for a, c in cands.most_common(5)])
for a, _ in cands.most_common(2):
    print(f"JP [{a:#x}] =", rd32(jp, sj, a))
# Section headers side by side
for (n1, v1, s1, r1, z1), (n2, v2, s2, r2, z2) in zip(su, sj):
    print(f"  {n1:8s} US va {v1:08X} vs {s1:08X} raw {z1:08X} | JP va {v2:08X} vs {s2:08X} raw {z2:08X}")
