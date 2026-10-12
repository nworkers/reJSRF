#!/usr/bin/env python3
"""Compare each XDK HLE export's stack pop count with the lifted original's `ret n`.

An HLE export returns through HleCall::Return(value, n), popping n stack
arguments. If n differs from the original function's `ret n`, the caller's
stack is skewed and the guest runs off into unrelated code with no error at
the call site. The lifted originals (sub_X_gen) carry the truth as
`/* ret N */` comments, and the HLE sources carry n in their Return calls, so
the two can be compared mechanically after every change to either side.

Usage:
    python scripts/recomp/check_hle_pops.py            # exit 1 on any mismatch
    python scripts/recomp/check_hle_pops.py --all      # also list the matches

Entries marked "no ret in lifted body" are functions whose original falls
through into another function or has an empty body (jsrf-recomp's inferred
names); those are checked by hand (docs/analysis/d3d8-adapter.md).
"""

import glob
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
GEN = ROOT / "gen" / "jsrf_us"
HLE_DIRS = [ROOT / "src" / "rexbox" / "hle" / name for name in ("d3d8", "xinput")]


def main():
    show_all = "--all" in sys.argv
    symbols = json.load(open(GEN / "xdk_symbols.json", encoding="utf-8"))
    extra = json.load(open(ROOT / "src/game/jsrf/recomp/xdk_symbols_extra.json", encoding="utf-8"))["symbols"]
    name_by_address = {}
    for symbol in symbols + extra:
        if symbol["kind"] == "function":
            name_by_address.setdefault(int(symbol["address"], 16), symbol["name"])

    rets = {}
    for path in glob.glob(str(GEN / "gen" / "recomp_*.c")):
        source = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
        for match in re.finditer(r"^void (sub_([0-9A-F]{8}))_gen\(void\)\n\{(.*?)^\}", source, re.S | re.M):
            values = set(re.findall(r"/\* ret(?: (\d+))? \*/", match.group(3)))
            rets[int(match.group(2), 16)] = sorted(int(v) if v else 0 for v in values)

    hle = {}
    for path in [p for d in HLE_DIRS for p in glob.glob(str(d / "*.cpp"))]:
        source = pathlib.Path(path).read_text(encoding="utf-8")
        functions = {}
        for match in re.finditer(r"^(?:template <[^>]*>\n)?void (\w+)\(\)\n\{(.*?)^\}", source, re.S | re.M):
            pops = set(int(x) for x in re.findall(r"Return(?:64)?\([^;]*?,\s*(\d+)\);", match.group(2)))
            functions[match.group(1)] = pops
        for match in re.finditer(r'AddExport\("\w+", "(\w+)", &([\w:<>]+)\)', source):
            hle[match.group(1)] = functions.get(match.group(2).split("<")[0], set())

    mismatches = 0
    for address, name in sorted(name_by_address.items(), key=lambda item: item[1]):
        if name not in hle:
            continue
        lifted = rets.get(address)
        pops = hle[name]
        if not lifted:
            print(f"{name:48s} no ret in lifted body; HLE pops {sorted(pops)} (check by hand)")
            continue
        expected = {value // 4 for value in lifted}
        if not pops:
            print(f"{name:48s} lifted ret {lifted}; HLE pops not parsed (helper function?)")
            continue
        ok = pops == expected
        if not ok:
            mismatches += 1
        if show_all or not ok:
            print(f"{name:48s} lifted ret {lifted} -> {sorted(expected)} args; HLE pops {sorted(pops)}"
                  f"{'' if ok else '   <-- MISMATCH'}")
    print(f"{mismatches} mismatch(es)")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
