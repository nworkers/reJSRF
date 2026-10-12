"""Checks that an XBE is the build a generated variant is lifted from.

The seeds, title bindings and decompiled functions hold addresses of one exact
build (src/game/jsrf/recomp/variants.json). Lifting another build, such as the
Japan-region default.xbe, would silently produce code that does not match
them, so the recompilation scripts stop instead.
"""

import hashlib
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANTS = ROOT / "src" / "game" / "jsrf" / "recomp" / "variants.json"


def check_xbe(xbe, variant="jsrf_us"):
    """Returns None when `xbe` is `variant`'s build, otherwise the reason it is not."""
    variants = json.loads(VARIANTS.read_text(encoding="utf-8"))["variants"]
    if variant not in variants:
        return f"unknown variant {variant} (known: {', '.join(sorted(variants))})"
    expected = variants[variant]
    actual = hashlib.md5(pathlib.Path(xbe).read_bytes()).hexdigest()
    if actual != expected["md5"]:
        return (f"{xbe} is not the {expected['label']} build that {variant} is lifted from "
                f"(MD5 {actual}, expected {expected['md5']}); pass that build with --xbe <path>")
    return None
