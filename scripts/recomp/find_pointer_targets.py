#!/usr/bin/env python3
"""List JSRF functions reached only through pointers and not lifted (rexbox's script with this title's paths).

    python scripts/recomp/find_pointer_targets.py            # list candidates
    python scripts/recomp/find_pointer_targets.py --write    # also write the seed file, then regenerate

--write replaces src/game/jsrf/recomp/seed_pointer_targets.json, which
regenerate.py reads like any other seed file. The seeds are kept as US
addresses and shared by the other builds (variant_fingerprints.py), so they
are found on the USA build only.
"""

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "third_party" / "rexbox" / "scripts" / "recomp"))
import find_pointer_targets  # noqa: E402  (rexbox's; this file runs as __main__)

TITLE = ROOT / "src" / "game" / "jsrf"

if __name__ == "__main__":
    sys.exit(find_pointer_targets.run(
        xbe=ROOT / "roms" / "Jet Set Radio Future" / "default.xbe",
        variant="jsrf_us",
        variants=TITLE / "recomp" / "variants.json",
        dispatch=ROOT / "gen" / "jsrf_us" / "gen" / "recomp_dispatch.c",
        seed_file=TITLE / "recomp" / "seed_pointer_targets.json",
    ))
