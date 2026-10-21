#!/usr/bin/env python3
"""Compare the HLE exports' pop counts with the lifted JSRF originals (rexbox's script with this title's paths).

    python scripts/recomp/check_hle_pops.py          # exit 1 on any mismatch
    python scripts/recomp/check_hle_pops.py --all    # also list the matches

Exports whose original falls through into another function or has an empty
body (jsrf-recomp's inferred names) are checked by hand
(docs/analysis/d3d8-adapter.md).
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "third_party" / "rexbox" / "scripts" / "recomp" / "check_hle_pops.py"

if __name__ == "__main__":
    sys.exit(subprocess.call([sys.executable, str(SCRIPT),
                              "--gen", str(ROOT / "gen" / "jsrf_us"),
                              "--extra-symbols", str(ROOT / "src" / "game" / "jsrf" / "recomp" / "xdk_symbols_extra.json")]
                             + sys.argv[1:]))
