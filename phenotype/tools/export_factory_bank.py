#!/usr/bin/env python3
"""Export the compiled factory presets (Source/Presets.h) to .phbank JSON banks,
one per library — a real-data template for DLC authors, and a way to ship the
factory as data too.

    python3 tools/export_factory_bank.py [out_dir]     # default: dist/banks/Factory

Parameter-only (no samples): the factory plays the built-in genome."""

import json, os, re, sys, collections

HERE = os.path.dirname(__file__)
PRESETS_H = os.path.join(HERE, "..", "Source", "Presets.h")

def parse():
    line_re = re.compile(r'\{\s*"([^"]+)"\s*,\s*\{\s*\{(.*?)\}\s*\}\s*\}')
    kv_re = re.compile(r'\{"([a-zA-Z]+)",([0-9.]+)f\}')
    out = []
    for line in open(PRESETS_H):
        m = line_re.search(line)
        if not m:
            continue
        name = m.group(1)
        if " > " not in name:
            continue
        params = {k: float(v) for k, v in kv_re.findall(m.group(2))}
        out.append((name, params))
    return out

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "dist", "banks", "Factory")
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)

    presets = parse()
    by_lib = collections.OrderedDict()
    for name, params in presets:
        lib, rest = name.split(" > ", 1)
        by_lib.setdefault(lib, []).append((rest, params))

    total = 0
    for lib, items in by_lib.items():
        bank = {"bank": lib, "format": 1, "author": "Phenotype Factory",
                "presets": [{"name": rest, "params": p} for rest, p in items]}
        fn = os.path.join(out_dir, lib.replace(" ", "_") + ".phbank")
        with open(fn, "w") as f:
            json.dump(bank, f, indent=1)
        total += len(items)
        print(f"  {lib:<12} {len(items):>4}  -> {os.path.relpath(fn)}")
    print(f"exported {total} presets across {len(by_lib)} banks to {out_dir}")

if __name__ == "__main__":
    main()
