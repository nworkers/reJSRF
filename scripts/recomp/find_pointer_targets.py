#!/usr/bin/env python3
"""List JSRF functions reached only through pointers and not lifted.

    python scripts/recomp/find_pointer_targets.py            # list candidates
    python scripts/recomp/find_pointer_targets.py --write    # also write the seed file, then regenerate

--write replaces src/game/jsrf/recomp/seed_pointer_targets.json, which
regenerate.py reads like any other seed file. The seeds are kept as US
addresses and shared by the other builds (variant_fingerprints.py), so they
are found on the USA build only.

The lifted addresses come from this project's generated dispatch table
(lifter.py); the scan and the seed file are rexbox's pointer_targets.py.
"""

import argparse
import pathlib
import sys

from lifter import ROOT, lifted_addresses
from pointer_targets import merge_seeds, print_candidates, scan  # noqa: E402  (rexbox's)
from xbe_variant import check_xbe  # noqa: E402  (rexbox's)

TITLE = ROOT / "src" / "game" / "jsrf"
VARIANT = "jsrf_us"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--xbe", type=pathlib.Path, default=ROOT / "roms" / "Jet Set Radio Future" / "default.xbe")
    parser.add_argument("--dispatch", type=pathlib.Path, default=ROOT / "gen" / VARIANT / "gen" / "recomp_dispatch.c",
                        help="the generated recomp_dispatch.c")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    problem = check_xbe(args.xbe, VARIANT, TITLE / "recomp" / "variants.json")
    if problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    if not args.dispatch.exists():
        print(f"error: {args.dispatch} is missing; run regenerate.py first", file=sys.stderr)
        return 2
    references = scan(args.xbe.read_bytes(), lifted_addresses(args.dispatch))
    print_candidates(references)
    if args.write:
        merge_seeds(TITLE / "recomp" / "seed_pointer_targets.json", references)
    return 0


if __name__ == "__main__":
    sys.exit(main())
