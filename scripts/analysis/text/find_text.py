"""Find which files hold the game's text: runs of Shift-JIS Japanese (with
kana) and runs of plain English, per file under Media/ and in the XBE.

usage: python -I find_text.py <game dir> [out.txt]
Reads only; prints a per-file count and a few decoded samples.
"""
import os, re, sys

KANA = re.compile(rb"(?:\x82[\x9f-\xf1]|\x83[\x40-\x96])")
# A Shift-JIS run: double-byte characters, with ASCII spaces/punctuation allowed.
SJIS = re.compile(rb"(?:[\x81-\x9f\xe0-\xef][\x40-\x7e\x80-\xfc]|[\x20-\x7e]){6,}")
ENG = re.compile(rb"[A-Za-z][A-Za-z ,.'!?\-]{15,}")

game = sys.argv[1]
out = open(sys.argv[2], "w", encoding="utf-8") if len(sys.argv) > 2 else sys.stdout
paths = [os.path.join(game, "default.xbe")]
for root, dirs, files in os.walk(os.path.join(game, "Media")):
    if "Z_ADX" in root:
        continue
    paths += [os.path.join(root, f) for f in files]
rows = []
for p in paths:
    data = open(p, "rb").read()
    jp = [m for m in SJIS.finditer(data) if len(KANA.findall(m.group())) >= 3]
    en = list(ENG.finditer(data))
    if jp or len(en) >= 5:
        rows.append((len(jp), len(en), os.path.relpath(p, game), jp[:3], en[:2]))
rows.sort(key=lambda r: -r[0])
for n_jp, n_en, rel, jp, en in rows:
    out.write(f"{n_jp:6d} jp {n_en:6d} en  {rel}\n")
    for m in jp:
        out.write(f"        jp {m.start():#x}: {m.group().decode('cp932', 'replace')[:60]}\n")
    for m in en:
        out.write(f"        en {m.start():#x}: {m.group().decode('ascii')[:60]}\n")
