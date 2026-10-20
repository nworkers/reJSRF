"""List every read of the language index in the US build and classify what
the code does with it right after the read.

A read is `mov ecx, 0x1FC724` followed, at once or a few instructions
later, by `call 0x43700` (the getter, `mov eax, [ecx]; ret`). The next instructions tell whether the value is
used at once (compared, indexed, passed on) or stored somewhere (cached).
Needs gen/jsrf_us/disasm (made by the recompile pipeline).

usage: python -I lang_reads.py [gen/jsrf_us/disasm/asm/text.asm]
"""
import re, sys
from collections import Counter

path = sys.argv[1] if len(sys.argv) > 1 else "gen/jsrf_us/disasm/asm/text.asm"
INS = re.compile(r"^\s+0x([0-9A-F]{8})\s+[0-9a-f]+\s+(\S+)\s*(.*?)\s*(;.*)?$")
func = None
rows = []           # (function, address, [next instructions])
lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
insns = []
for ln in lines:
    if ln.startswith("; Function: "):
        func = ln.split(": ", 1)[1].strip()
        continue
    m = INS.match(ln)
    if m:
        insns.append((func, int(m.group(1), 16), m.group(2), m.group(3)))
for k, (f, a, mn, op) in enumerate(insns):
    if mn != "mov" or op.lower() != "ecx, 0x1fc724":
        continue
    # The getter call usually follows at once; the compiler sometimes
    # schedules a few unrelated instructions in between.
    for c in range(k + 1, min(k + 8, len(insns))):
        if insns[c][2] == "call" and insns[c][3].lower() == "0x43700":
            nxt = [f"{m} {o}" for _, _, m, o in insns[c + 1:c + 5]]
            rows.append((f, a, nxt))
            break
        if insns[c][2] == "call" or insns[c][3].lower().startswith("ecx"):
            rows.append((f, a, ["(no getter call)"] + [f"{m} {o}" for _, _, m, o in insns[k + 1:k + 4]]))
            break


def classify(nxt):
    first = nxt[0]
    if re.match(r"mov (dword ptr )?\[0x[0-9a-f]+\], eax", first):
        return "store to global"
    if re.match(r"mov (dword ptr )?\[e[a-z]{2}( [+-] 0x[0-9a-f]+)?\], eax", first):
        return "store to object"
    if first.startswith(("cmp eax", "test eax", "dec eax", "sub eax")):
        return "compare/switch"
    if first.startswith("push eax"):
        return "argument"
    if "eax*" in first or "eax +" in first or "+ eax" in first:
        return "index"
    return "other"


kinds = Counter()
by_func = Counter()
for f, a, nxt in rows:
    c = classify(nxt)
    kinds[c] += 1
    by_func[f] += 1
    print(f"{a:08X} {f:16s} {c:16s} | " + " ; ".join(nxt[:3]))
print(f"\nreads: {len(rows)} in {len(by_func)} functions")
for c, n in kinds.most_common():
    print(f"  {c:16s} {n}")
