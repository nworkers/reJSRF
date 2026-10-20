"""Tells which supported build an XBE is, from its content.

The variant is decided by the file's MD5 (src/game/jsrf/recomp/variants.json),
never by its name: a dump's default.xbe may be either build, and names such as
default_eng.xbe are whatever the person who dumped the disc chose.

The seeds, title bindings and decompiled functions hold addresses of the US
build. The Japanese build shares them (docs/analysis/language.md section 10),
which regenerate.py checks with variant_fingerprints.py before lifting it.
"""

import hashlib
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANTS = ROOT / "src" / "game" / "jsrf" / "recomp" / "variants.json"


def known_variants():
    return json.loads(VARIANTS.read_text(encoding="utf-8"))["variants"]


def identify_xbe(xbe):
    """Returns (variant, label) for a supported build, or (None, reason)."""
    variants = known_variants()
    actual = hashlib.md5(pathlib.Path(xbe).read_bytes()).hexdigest()
    for name, entry in variants.items():
        if entry["md5"] == actual:
            return name, entry["label"]
    known = ", ".join(f"{entry['label']} {entry['md5']}" for entry in variants.values())
    return None, f"{xbe} (MD5 {actual}) is not a supported Jet Set Radio Future build (known: {known})"


def check_xbe(xbe, variant="jsrf_us"):
    """Returns None when `xbe` is `variant`'s build, otherwise the reason it is not."""
    variants = known_variants()
    if variant not in variants:
        return f"unknown variant {variant} (known: {', '.join(sorted(variants))})"
    found, detail = identify_xbe(xbe)
    if found is None:
        return detail
    if found != variant:
        return f"{xbe} is the {detail} build, not {variants[variant]['label']} ({variant})"
    return None
