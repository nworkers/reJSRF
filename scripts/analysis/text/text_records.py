"""Collect the game's dialogue/objective text records and measure them.

A record is a NUL-terminated Shift-JIS string holding the language
separator `$e`: the Japanese pages, `$e`, the English pages (pages split by
`$r`, lines by `$n`). Reports per file group: records, Japanese characters,
distinct Japanese characters (the glyphs a font needs), English letters,
how many language sections each record has, and the control codes used.

usage: python -I text_records.py <game dir> [out.txt]
"""
import collections, glob, os, re, sys

game = sys.argv[1]
out = open(sys.argv[2], "w", encoding="utf-8") if len(sys.argv) > 2 else sys.stdout
media = os.path.join(game, "Media")
groups = {
    "Event/Event/*.dat": "event scenes",
    "Event/TalkEvent/*.bin": "talk events",
    "Mission/*.bin": "missions (.bin)",
    "Mission/*.dat": "missions (.dat)",
}
STR = re.compile(rb"(?:[\x81-\x9f\xe0-\xef][\x40-\x7e\x80-\xfc]|[\x09\x20-\x7e])+(?=\x00)")
all_glyphs = collections.Counter()
for pattern, label in groups.items():
    files = glob.glob(os.path.join(media, pattern))
    records = jp = en = 0
    sections = collections.Counter()
    codes = collections.Counter()
    glyphs = collections.Counter()
    sample = None
    for f in files:
        for m in STR.finditer(open(f, "rb").read()):
            s = m.group()
            if b"$e" not in s:
                continue
            text = s.decode("cp932", "replace")
            records += 1
            sections[len(re.split(r"\$r?\$e\$r?", text.rstrip("$re")))] += 1
            codes.update(re.findall(r"\$[A-Za-z0-9]", text))
            plain = re.sub(r"\$[A-Za-z0-9]", "", text)
            for ch in plain:
                if ord(ch) > 0x7F:
                    jp += 1
                    glyphs[ch] += 1
                elif ch.isalpha():
                    en += 1
            sample = sample or (os.path.relpath(f, media), text[:160])
    all_glyphs.update(glyphs)
    out.write(f"== {label}: {len(files)} files, {records} records, {jp} Japanese chars "
              f"({len(glyphs)} distinct), {en} English letters\n")
    out.write(f"   language sections per record: {dict(sections)}\n")
    out.write(f"   codes: {codes.most_common(20)}\n")
    out.write(f"   sample: {sample}\n")
kana = sum(1 for c in all_glyphs if 0x3040 <= ord(c) <= 0x30FF)
kanji = sum(1 for c in all_glyphs if 0x4E00 <= ord(c) <= 0x9FFF)
out.write(f"\nall groups: {len(all_glyphs)} distinct Japanese characters "
          f"({kana} kana, {kanji} kanji, {len(all_glyphs) - kana - kanji} other full-width)\n")
