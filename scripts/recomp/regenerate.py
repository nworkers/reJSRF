#!/usr/bin/env python3
"""Regenerate the lifted C for the Jet Set Radio Future build of an XBE (gen/<variant>/).

A wrapper over rexbox's scripts/recomp/regenerate.py (the pipeline: XDK
identification, parse, disasm, func_id, abi, HLE and decompiled overrides,
lift) that adds what only this title knows and fills in its paths:

* The build: the XBE (default.xbe, or the one --xbe names) is identified by
  its MD5 (src/game/jsrf/recomp/variants.json, jsrf_variant.py), never by its
  file name, and the output goes to gen/<variant>/. --variant, when given,
  must agree. An unsupported build stops the run.
* The shared addresses: the seeds and decompiled functions name US
  addresses; for any other build, variant_fingerprints.py first checks that
  they hold the same code there (docs/design/20261020-i108-japanese-variant.md),
  and the run stops if not.
* The lifter: this project's own xboxrecomp pin (third_party/xboxrecomp, the
  fork's rejsrf branch), so the generated code matches the ABI header the
  build compiles rexbox_recomp against.

The generated code is derived from the game's bytes and is never committed.

Usage:
    python scripts/recomp/regenerate.py
    python scripts/recomp/regenerate.py --xbe <path> --symbols-tool <exe>
"""

import argparse
import pathlib
import sys

from jsrf_variant import identify_xbe
from variant_fingerprints import check as check_fingerprints

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "third_party" / "rexbox" / "scripts" / "recomp"))
import regenerate as rexbox_regenerate  # noqa: E402  (rexbox's; this file runs as __main__)

TITLE = ROOT / "src" / "game" / "jsrf"
DEFAULT_XBE = ROOT / "roms" / "Jet Set Radio Future" / "default.xbe"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0], add_help=False)
    parser.add_argument("--xbe", type=pathlib.Path, default=DEFAULT_XBE)
    parser.add_argument("--variant", default=None,
                        help="expected variant (jsrf_us, jsrf_jp); found from the XBE's content when left out")
    args, rest = parser.parse_known_args()
    if "-h" in rest or "--help" in rest:
        parser.print_help()
        print("\nrexbox's options (passed through):")
        return rexbox_regenerate.run(["--help"])

    xbe = args.xbe.resolve()
    if not xbe.is_file():
        sys.exit(f"no XBE at {xbe} -- put your own dump under roms/ (docs/guides/)")
    variant, detail = identify_xbe(xbe)
    if variant is None:
        sys.exit(f"error: {detail}")
    if args.variant is not None and args.variant != variant:
        sys.exit(f"error: {xbe} is the {detail} build ({variant}), not {args.variant}")
    print(f"=== variant: {variant} ({detail})", flush=True)
    if variant != "jsrf_us":
        problems = check_fingerprints(xbe)
        if problems:
            sys.exit("error: the US-addressed seeds and decompiled functions do not fit this build:\n  "
                     + "\n  ".join(problems))
        print("    named functions: all hold the USA code at the same address", flush=True)

    return rexbox_regenerate.run(
        ["--xbe", str(xbe), "--variant", variant] + rest,
        title_dir=TITLE / "recomp",
        override_dirs=[TITLE / "decomp"],
        gen_root=ROOT / "gen",
        xboxrecomp=ROOT / "third_party" / "xboxrecomp",
        symbol_tool_candidates=[
            ROOT / "build" / "win32_x86" / "third_party" / "rexbox" / "tools" / "Release" / "rexbox_xdk_symbols.exe",
            ROOT / "build" / "win32_x86" / "third_party" / "rexbox" / "tools" / "Debug" / "rexbox_xdk_symbols.exe",
            ROOT / "build" / "third_party" / "rexbox" / "tools" / "rexbox_xdk_symbols",
        ],
        root=ROOT,
    )


if __name__ == "__main__":
    sys.exit(main())
