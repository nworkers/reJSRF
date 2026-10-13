"""Turn a REXBOX_SAMPLE histogram into function names (diagnostic).

The app writes "<module> <offset hex> <samples> [<caller offset hex>]" lines
(platform/sampler.h); the caller is the executable's code found on the stack
of a sample outside the executable.
Offsets in rejsrf.exe are resolved with the linker map (rejsrf.map, written
next to the executable by MSVC builds). Offsets in system DLLs are resolved
to the nearest export below them, read from the DLL in --dll-dir (default
the 32-bit system folder); other modules are grouped by name.

Usage: python scripts/perf/resolve_samples.py <samples.txt> <rejsrf.map> [--top N]
"""

import argparse
import bisect
import collections
import os
import re
import struct
import sys

MAP_LINE = re.compile(r"^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+(f\s+)?(?:i\s+)?(\S+)\s*$")
BASE_LINE = re.compile(r"Preferred load address is ([0-9a-fA-F]+)")


def short_name(name):
    """A readable form of a decorated MSVC name."""
    if name.startswith("?"):
        # ?Name@scope@...: keep the function and its innermost named scopes.
        parts = name[1:].split("@")
        scopes = [p for p in parts[1:] if p and not p.startswith("?") and not p.startswith("Y") and p[0].isalpha()]
        return "::".join(reversed(scopes[:3])) + "::" + parts[0] if scopes else parts[0]
    if name.startswith("_"):
        name = name[1:]
    return name.split("@")[0]


def group_of(where):
    """The library a map entry comes from (lib:object -> lib)."""
    library = where.split(":")[0]
    if library.startswith("rejsrf_recomp"):
        return "lifted title code"
    return library


def load_map(path):
    base = 0x400000
    entries = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = BASE_LINE.search(line)
            if m:
                base = int(m.group(1), 16)
                continue
            m = MAP_LINE.match(line)
            if m and m.group(3):
                entries.append((int(m.group(2), 16) - base, m.group(1), m.group(4)))
    entries.sort()
    return entries


def load_exports(path):
    """(rva, name) of a PE file's named exports, sorted; empty if unreadable."""
    try:
        data = open(path, "rb").read()
    except OSError:
        return []
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    directories = optional + (96 if magic == 0x10B else 112)
    export_rva = struct.unpack_from("<I", data, directories)[0]
    table = optional + struct.unpack_from("<H", data, pe + 20)[0]

    def offset(rva):
        for i in range(sections):
            va, raw_size, raw = struct.unpack_from("<I I I", data, table + i * 40 + 12)
            size = max(struct.unpack_from("<I", data, table + i * 40 + 8)[0], raw_size)
            if va <= rva < va + size:
                return rva - va + raw
        return None

    if export_rva == 0 or offset(export_rva) is None:
        return []
    e = offset(export_rva)
    count_names = struct.unpack_from("<I", data, e + 24)[0]
    functions, names, ordinals = (offset(r) for r in struct.unpack_from("<III", data, e + 28))
    result = []
    for i in range(count_names):
        name_at = offset(struct.unpack_from("<I", data, names + i * 4)[0])
        name = data[name_at:data.index(b"\0", name_at)].decode("ascii", "replace")
        ordinal = struct.unpack_from("<H", data, ordinals + i * 2)[0]
        result.append((struct.unpack_from("<I", data, functions + ordinal * 4)[0], name))
    result.sort()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("samples")
    parser.add_argument("map")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--callers", type=int, default=20,
                        help="also list the top N (DLL function, caller in the executable) pairs")
    windows = os.environ.get("SystemRoot", r"C:\Windows")
    parser.add_argument("--dll-dir", default=os.path.join(windows, "SysWOW64"))
    args = parser.parse_args()
    exports = {}

    entries = load_map(args.map)
    starts = [e[0] for e in entries]

    def exe_function(offset):
        i = bisect.bisect_right(starts, offset) - 1
        if i < 0:
            return None
        _, name, where = entries[i]
        return short_name(name), group_of(where)

    total = 0
    by_function = collections.Counter()
    by_group = collections.Counter()
    by_caller = collections.Counter()
    with open(args.samples, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.split()
            module, offset, count = fields[0], int(fields[1], 16), int(fields[2])
            caller = exe_function(int(fields[3], 16)) if len(fields) > 3 else None
            total += count
            if module.lower().endswith("rejsrf.exe"):
                found = exe_function(offset)
                if found:
                    by_function[found] += count
                    by_group[found[1]] += count
                    continue
            if module not in exports:
                exports[module] = load_exports(os.path.join(args.dll_dir, module))
            table = exports[module]
            i = bisect.bisect_right([e[0] for e in table], offset) - 1 if table else -1
            name = module + "!" + table[i][1] if i >= 0 else module
            by_function[(name, module)] += count
            by_group[module] += count
            by_caller[(name, caller[0] if caller else "?")] += count
    if total == 0:
        print("no samples")
        return 1
    print(f"{total} samples\n")
    print("By library or module:")
    for group, count in by_group.most_common():
        print(f"  {100.0 * count / total:5.1f}%  {count:7}  {group}")
    print(f"\nTop {args.top} functions:")
    for (name, group), count in by_function.most_common(args.top):
        print(f"  {100.0 * count / total:5.1f}%  {count:7}  {name}  [{group}]")
    if args.callers and by_caller:
        print(f"\nTop {args.callers} DLL functions by caller in the executable:")
        for (name, caller), count in by_caller.most_common(args.callers):
            print(f"  {100.0 * count / total:5.1f}%  {count:7}  {name}  <-  {caller}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
