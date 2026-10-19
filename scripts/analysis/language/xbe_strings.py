"""Compare strings of the US and JP JSRF XBEs, mapped to virtual addresses.

usage: python -I xbe_strings.py <us.xbe> <jp.xbe>
"""
import re, struct, sys
from collections import Counter


def sections(data):
    base = struct.unpack_from("<I", data, 0x104)[0]
    n = struct.unpack_from("<I", data, 0x11C)[0]
    hdr = struct.unpack_from("<I", data, 0x120)[0] - base
    out = []
    for i in range(n):
        flags, va, vs, raw, rs, name_va = struct.unpack_from("<6I", data, hdr + i * 56)
        name_off = name_va - base
        name = data[name_off:data.index(b"\0", name_off)].decode()
        out.append((name, va, vs, raw, rs))
    return out


def strings(data, minlen=4):
    secs = sections(data)
    res = {}
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % minlen, data):
        off = m.start()
        for name, va, vs, raw, rs in secs:
            if raw <= off < raw + rs:
                res[va + off - raw] = (name, m.group().decode())
                break
    return res


us = open(sys.argv[1], "rb").read()
jp = open(sys.argv[2], "rb").read()
su, sj = strings(us), strings(jp)
vu = Counter(s for _, s in su.values())
vj = Counter(s for _, s in sj.values())
only_us = sorted(set(vu) - set(vj))
only_jp = sorted(set(vj) - set(vu))
print(f"strings: US {len(su)}  JP {len(sj)}  only US {len(only_us)}  only JP {len(only_jp)}")
print("== only in US ==")
for s in only_us[:80]:
    print("  ", repr(s)[:100])
print("== only in JP ==")
for s in only_jp[:80]:
    print("  ", repr(s)[:100])
print("== file-name-like strings (both) ==")
names = sorted({s for s in vu | vj if re.search(r"\.(bin|dat|adx|xpr|wav|afs|sfd|dds|tbn|txt)\b", s, re.I)
                or re.search(r"[\\/](media|font|event|mission|title|misc)", s, re.I)})
for s in names[:120]:
    print(f"   {'U' if s in vu else ' '}{'J' if s in vj else ' '} {s[:100]}")
print("== language-ish ==")
for s in sorted(vu | vj):
    if re.search(r"lang|japan|english|_jp|_us|_en\b|_e\.|_j\.|eng|jpn|region", s, re.I):
        print(f"   {'U' if s in vu else ' '}{'J' if s in vj else ' '} {s[:100]}")
