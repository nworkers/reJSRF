#!/usr/bin/env python3
"""Compare the HLE exports' pop counts with the lifted JSRF originals.

    python scripts/recomp/check_hle_pops.py          # exit 1 on any mismatch
    python scripts/recomp/check_hle_pops.py --all    # also list the matches

Exports whose original falls through into another function or has an empty
body (jsrf-recomp's inferred names) are checked by hand
(docs/analysis/d3d8-adapter.md).

The `ret n` of each original is read from this project's lifted code
(lifter.py); the comparison and the HLE side are rexbox's hle_pops.py.
"""

import argparse
import pathlib
import sys

from lifter import ROOT, lifted_rets
from hle_pops import compare_pops, hle_pop_counts, xdk_function_names  # noqa: E402  (rexbox's)

TITLE = ROOT / "src" / "game" / "jsrf"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--gen", type=pathlib.Path, default=ROOT / "gen" / "jsrf_us", help="<gen root>/<variant>")
    parser.add_argument("--all", action="store_true", help="also list the matches")
    args = parser.parse_args()
    names = xdk_function_names(args.gen / "xdk_symbols.json", TITLE / "recomp" / "xdk_symbols_extra.json")
    lines, mismatches = compare_pops(names, lifted_rets(args.gen / "gen"), hle_pop_counts(), args.all)
    print("\n".join(lines))
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
