"""Structural diff of the US and JP JSRF XBEs.

Every code section is decoded linearly with capstone. An operand value that
lands in the image (an address) is replaced by a class token: same section
-> 'A', so that relocated data addresses compare equal. What is left
different is a real change of code or constants.

Also maps the US .data/.bss addresses to the JP ones by pairing the
operands of matching instructions, and lists the pairs that are not a
plain constant shift.

usage: python -I code_diff.py <us.xbe> <jp.xbe>
"""
import struct, sys, difflib
from collections import Counter
from capstone import Cs, CS_ARCH_X86, CS_MODE_32


def load(path):
    x = open(path, "rb").read()
    base = struct.unpack_from("<I", x, 0x104)[0]
    n = struct.unpack_from("<I", x, 0x11C)[0]
    hdr = struct.unpack_from("<I", x, 0x120)[0] - base
    secs = []
    for i in range(n):
        flags, va, vs, raw, rs, nva = struct.unpack_from("<6I", x, hdr + i * 56)
        name = x[nva - base:x.index(b"\0", nva - base)].decode()
        secs.append((name, va, vs, raw, rs, flags))
    return x, secs


us, su = load(sys.argv[1])
jp, sj = load(sys.argv[2])
LO, HI = 0x10000, 0x290000
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.skipdata = True


def stream(x, secs, name):
    s = next(s for s in secs if s[0] == name)
    _, va, vs, raw, rs, _ = s
    out = []
    for insn in md.disasm(x[raw:raw + rs], va):
        b = insn.bytes
        toks = insn.mnemonic + " " + insn.op_str
        vals = []
        # replace hex values that are image addresses
        parts = []
        for tok in toks.replace(",", " , ").replace("[", " [ ").replace("]", " ] ").split():
            if tok.startswith("0x") and ":" not in tok:
                v = int(tok, 16)
                if LO <= v < HI:
                    vals.append(v)
                    tok = "A"
            parts.append(tok)
        out.append((insn.address, " ".join(parts), vals))
    return out


total_real = 0
pairs = Counter()
for name in [s[0] for s in su if s[5] & 4]:          # executable
    a = stream(us, su, name)
    b = stream(jp, sj, name)
    ka = [t for _, t, _ in a]
    kb = [t for _, t, _ in b]
    sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
    real = []
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            for k in range(i2 - i1):
                for va, vb in zip(a[i1 + k][2], b[j1 + k][2]):
                    if va != vb:
                        pairs[(va, vb)] += 1
            continue
        real.append((op, a[i1:i2], b[j1:j2]))
    n_us = sum(i2 - i1 for op, i1, i2, j1, j2 in sm.get_opcodes() if op != "equal")
    n_jp = sum(j2 - j1 for op, i1, i2, j1, j2 in sm.get_opcodes() if op != "equal")
    print(f"{name:8s} insns US {len(a):7d} JP {len(b):7d}  differing blocks {len(real):4d}  insns US {n_us} JP {n_jp}")
    total_real += len(real)
    for op, ra, rb in real[:25]:
        print(f"   {op}: US {ra[0][0]:08X}" if ra else f"   {op}: US -", f"JP {rb[0][0]:08X}" if rb else "JP -")
        for _, t, v in ra[:6]:
            print(f"      - {t}  {[hex(x) for x in v]}")
        for _, t, v in rb[:6]:
            print(f"      + {t}  {[hex(x) for x in v]}")

# Address map: group by delta
print("\n== address pairs (US -> JP) by delta ==")
deltas = Counter()
for (va, vb), c in pairs.items():
    deltas[vb - va] += c
for d, c in deltas.most_common(12):
    print(f"   delta {d:+#x}: {c} operand uses")
print("\n== pairs with an unusual delta (fewer than 50 uses of that delta) ==")
rare = {d for d, c in deltas.items() if c < 50}
for (va, vb), c in sorted(pairs.items()):
    if vb - va in rare:
        print(f"   {va:08X} -> {vb:08X} ({vb - va:+#x}) x{c}")
