#!/usr/bin/env python3
"""Generate natives-<version>.txt for orange-core from FiveM's public universal crossmap.

orange-core calls natives by their canonical hash and translates them to the
hash the running game build registers through natives-<version>.txt next to
orange-core.dll (format: "0x<canonical> 0x<build hash> [name]" per line,
see shared/NativeCrossmap.h). The translations come from FiveM's
CrossMapping_Universal.h (https://github.com/citizenfx/fivem,
code/components/rage-scripting-five/include): one row per native, one column
per hash reshuffle (build 323 = column 0, then 350, 372, ... 2802, 2944; the
hashes have not changed since 1.0.2944, so the last column serves every newer
build). The first non-zero entry of a row is the canonical hash.

That file is not part of this repository (rage-scripting-five is outside the
LGPL-covered part of FiveM); it is fetched from GitHub on demand. Launcher.exe
does the same automatically when natives-<version>.txt is missing; this
script adds native names (from the alloc8or natives database) and can check
the result against the natives the game actually registered.

Examples:
  python3 tools/natives/crossmap_from_fivem.py --version 1.0.3889.0 --out natives-1.0.3889.0.txt
  python3 tools/natives/crossmap_from_fivem.py --build 3889 --registered natives-1.0.3889.0.registered.txt \\
      --universal CrossMapping_Universal.h --nativedb natives.json --out natives-1.0.3889.0.txt
"""
import argparse
import json
import re
import sys
import urllib.request

UNIVERSAL_URL = "https://raw.githubusercontent.com/citizenfx/fivem/master/code/components/rage-scripting-five/include/CrossMapping_Universal.h"
NATIVEDB_URL = "https://raw.githubusercontent.com/alloc8or/gta5-nativedb-data/master/natives.json"
# FiveM TableBuilder.cpp: the column for a build is the number of these that are <= the build.
VERSIONS = [350, 372, 393, 463, 505, 573, 617, 678, 757, 791, 877, 944, 1011, 1103, 1180,
            1290, 1365, 1493, 1604, 1737, 1868, 2060, 2189, 2372, 2545, 2802, 2944]
HASH = re.compile(r"0[xX]([0-9A-Fa-f]{16})")


def column_for(build):
    return sum(1 for v in VERSIONS if build >= v)


def fetch(source, what):
    if source.startswith("http://") or source.startswith("https://"):
        sys.stderr.write("downloading %s from %s\n" % (what, source))
        request = urllib.request.Request(source, headers={"User-Agent": "GTA-Orange crossmap_from_fivem"})
        with urllib.request.urlopen(request, timeout=60) as response:
            return response.read().decode("utf-8", "replace")
    with open(source, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def parse_rows(text):
    rows = []
    for line in text.splitlines():
        hashes = [int(h, 16) for h in HASH.findall(line)]
        if len(hashes) >= 2:
            rows.append(hashes)
    return rows


def load_registered(path):
    registered = set()
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line or line[0] in ";#":
                continue
            token = line.split()[0]
            try:
                registered.add(int(token, 16))
            except ValueError:
                pass
    return registered


def load_names(source):
    names = {}
    try:
        db = json.loads(fetch(source, "the natives database"))
    except Exception as e:  # names are optional
        sys.stderr.write("warning: natives database not available (%s), names omitted\n" % e)
        return names
    for namespace, natives in db.items():
        for hash_text, native in natives.items():
            try:
                names[int(hash_text, 16)] = "%s::%s" % (namespace, native.get("name", ""))
            except ValueError:
                pass
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", help="game version as client.log prints it, e.g. 1.0.3889.0")
    parser.add_argument("--build", type=int, help="game build number (third version component), e.g. 3889")
    parser.add_argument("--universal", default=UNIVERSAL_URL, help="CrossMapping_Universal.h path or URL (default: FiveM master on GitHub)")
    parser.add_argument("--nativedb", default=NATIVEDB_URL, help="natives.json path or URL for the names, 'none' to skip")
    parser.add_argument("--registered", help="natives-<version>.registered.txt written by orange-core (or the extracted list): drop pairs the game does not register")
    parser.add_argument("--out", help="output file (default: natives-<version>.txt or stdout)")
    args = parser.parse_args()

    build = args.build
    if args.version:
        parts = args.version.split(".")
        if len(parts) < 3 or not parts[2].isdigit():
            parser.error("--version must look like 1.0.3889.0")
        build = build or int(parts[2])
    if not build:
        parser.error("--version or --build is required")
    column = column_for(build)

    rows = parse_rows(fetch(args.universal, "the universal crossmap"))
    if not rows:
        sys.exit("no crossmap rows found in %s" % args.universal)
    names = {} if args.nativedb.lower() == "none" else load_names(args.nativedb)
    registered = load_registered(args.registered) if args.registered else None

    lines = []
    stats = {"rows": len(rows), "translations": 0, "identical": 0, "absent": 0, "unregistered": 0}
    written = set()
    for row in rows:
        canonical = next((h for h in row if h), 0)
        value = row[column] if column < len(row) else 0
        if not canonical or not value:
            stats["absent"] += 1
            continue
        if value == canonical:
            stats["identical"] += 1
            continue
        if registered is not None and value not in registered:
            stats["unregistered"] += 1
            continue
        name = names.get(canonical, "")
        lines.append("0x%016X 0x%016X%s" % (canonical, value, (" " + name) if name else ""))
        written.add(value)
        stats["translations"] += 1

    header = [
        "; natives crossmap for GTA V build %d%s: <canonical hash> <hash of this build> [name]" % (build, (" (%s)" % args.version) if args.version else ""),
        "; generated by tools/natives/crossmap_from_fivem.py from FiveM's CrossMapping_Universal.h (column %d of 28)" % column,
        "; source: https://github.com/citizenfx/fivem, code/components/rage-scripting-five (Cfx.re / Take-Two terms); regenerate instead of redistributing",
        "; natives added after 1.0.2944 keep their canonical hash and need no line here",
    ]
    if registered is not None:
        missing = len(registered - written - set(h for row in rows for h in row))
        header.append("; checked against %d registered natives: %d pairs dropped as unregistered, %d registered natives not in the table (new since the table was made, canonical hash applies)"
                      % (len(registered), stats["unregistered"], missing))
    output = "\n".join(header + lines) + "\n"

    out_path = args.out or ("natives-%s.txt" % args.version if args.version else None)
    if out_path:
        with open(out_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(output)
        sys.stderr.write("wrote %s\n" % out_path)
    else:
        sys.stdout.write(output)
    sys.stderr.write("rows %(rows)d, translations %(translations)d, identical %(identical)d, absent in this build %(absent)d, unregistered %(unregistered)d\n" % stats)


if __name__ == "__main__":
    main()
