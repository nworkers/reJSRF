#!/usr/bin/env python3
"""Regenerate the lifted C for the Jet Set Radio Future build of an XBE (gen/<variant>/).

A wrapper over this project's pipeline (lifter.py: XDK identification,
parse, disasm, func_id, abi, HLE and decompiled overrides, lift) that adds what only this title knows and fills in its paths:

* The build: the XBE (default.xbe, or the one --xbe names) is identified by
  its MD5 (src/game/jsrf/recomp/variants.json, jsrf_variant.py), never by its
  file name, and the output goes to gen/<variant>/. --variant, when given,
  must agree. An unsupported build stops the run.
* The shared addresses: the seeds and decompiled functions name US
  addresses; for any other build, variant_fingerprints.py first checks that
  they hold the same code there (docs/design/20261020-i108-japanese-variant.md),
  and the run stops if not.
* The lifter: this project's own xboxrecomp pin (third_party/xboxrecomp, the
  fork's rejsrf branch). The generated code carries its ABI header, which
  rexbox's side of the ABI compiles against.

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

import lifter

ROOT = lifter.ROOT

TITLE = ROOT / "src" / "game" / "jsrf"
DEFAULT_XBE = ROOT / "roms" / "Jet Set Radio Future" / "default.xbe"


def symbol_tool_candidates():
    """Where a build of this host put rexbox_xdk_symbols, the first existing one used.

    Only this host's builds: a checkout shared between Windows and WSL holds
    both, and a Windows .exe started from Linux (WSL's interop) cannot open
    the /mnt/... paths it is given, failing as if the XBE were not one."""
    tools = ROOT / "build"
    if sys.platform == "win32":
        return [tools / "win32_x86" / "third_party" / "rexbox" / "tools" / config / "rexbox_xdk_symbols.exe"
                for config in ("Release", "Debug")]
    # The Linux presets (scripts/build_linux.sh), then a hand-made build folder.
    return [tools / f"linux-{arch}-{config}" / "third_party" / "rexbox" / "tools" / "rexbox_xdk_symbols"
            for arch in ("x64", "x86") for config in ("release", "debug")] + [
        tools / "third_party" / "rexbox" / "tools" / "rexbox_xdk_symbols"]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0], add_help=False)
    parser.add_argument("--xbe", type=pathlib.Path, default=DEFAULT_XBE)
    parser.add_argument("--variant", default=None,
                        help="expected variant (jsrf_us, jsrf_jp); found from the XBE's content when left out")
    args, rest = parser.parse_known_args()
    if "-h" in rest or "--help" in rest:
        parser.print_help()
        print("\nthe pipeline's options (passed through):")
        return lifter.run(["--help"])

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

    return lifter.run(
        ["--xbe", str(xbe), "--variant", variant] + rest,
        title_dir=TITLE / "recomp",
        override_dirs=[TITLE / "decomp"],
        gen_root=ROOT / "gen",
        xboxrecomp=ROOT / "third_party" / "xboxrecomp",
        symbol_tool_candidates=symbol_tool_candidates(),
        root=ROOT,
    )


if __name__ == "__main__":
    sys.exit(main())
