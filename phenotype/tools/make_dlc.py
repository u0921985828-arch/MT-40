#!/usr/bin/env python3
"""Compose the 10k DLC: 10 themed .phbank banks (~1000 presets each), every
preset a genome sample (matched to the theme's role) crossed with a seeded
parameter genotype, under an evocative name. Deterministic (seeded) so it is
fully reproducible.

Two phases so the C++ QA harness can recalibrate levels in between:

    python3 tools/make_dlc.py build      # -> dlc/_plan.json + dlc/_qa_in.tsv
    ./qa_dlc dlc/_qa_in.tsv dlc/_qa_out.tsv        # renders + recalibrates
    python3 tools/make_dlc.py finalize   # -> dlc/<BANK>.phbank (QA-corrected)

Requires numpy only for the seeded RNG convenience; no plugin code ships this."""

import json, os, sys, random

HERE = os.path.dirname(__file__)
DLC = os.path.abspath(os.path.join(HERE, "..", "dlc"))

# 31 params in kDefs order (Source/Parameters.h) with their engine defaults.
PARAMS = [
    ("caudal", .5), ("soilDensity", .5), ("saturation", .9), ("grainDensity", .4),
    ("grainSize", .3), ("position", .5), ("spray", .2), ("pitchA", .5), ("pitchB", .5),
    ("crossBlend", .5), ("modDepth", .5), ("outputGain", .8), ("arpOn", 0.), ("arpRate", .4),
    ("arpMode", 0.), ("arpSync", 0.), ("scaleType", 0.), ("filterCutoff", 1.), ("filterReso", .12),
    ("filterType", 0.), ("filterMod", 0.), ("drive", .1), ("unison", 0.), ("unisonDetune", .25),
    ("stereoWidth", .5), ("delayMix", 0.), ("delayTime", .35), ("delayFb", .35), ("reverbMix", 0.),
    ("reverbSize", .5), ("reverbDamp", .4),
]
PIDX = {p: i for i, (p, _) in enumerate(PARAMS)}

# ---------------------------------------------------------------------------
#  Themes: preferred genome roles + parameter archetype (ranges or fixed).
#  A range (lo,hi) is sampled uniform; a scalar is fixed. Unlisted params take
#  the engine default. Ranges are kept in musically-safe zones; the QA pass
#  tightens outputGain per preset so nothing rides the clipper.
# ---------------------------------------------------------------------------
R = lambda a, b: (a, b)
THEMES = [
    ("NEBULA",     "Nebula Pads",        ["pad", "texture"], {
        "grainSize": R(.55, .95), "grainDensity": R(.5, .85), "caudal": R(.2, .5),
        "soilDensity": R(.55, .85), "modDepth": R(.4, .8), "crossBlend": R(.3, .7),
        "unison": R(.3, .7), "unisonDetune": R(.2, .5), "filterCutoff": R(.5, .85),
        "stereoWidth": R(.7, 1.), "reverbMix": R(.3, .7), "reverbSize": R(.5, .9),
        "reverbDamp": R(.3, .6), "outputGain": R(.5, .66)}),
    ("SUBMARINE",  "Submarine Bass",     ["bass"], {
        "grainSize": R(.2, .42), "grainDensity": R(.45, .7), "pitchA": R(.35, .5),
        "pitchB": R(.35, .5), "crossBlend": R(.35, .65), "filterCutoff": R(.32, .55),
        "filterReso": R(.2, .55), "drive": R(.15, .45), "unison": R(.0, .3),
        "stereoWidth": R(.2, .5), "outputGain": R(.42, .58)}),
    ("SOLAR",      "Solar Leads",        ["lead"], {
        "grainSize": R(.3, .55), "grainDensity": R(.4, .7), "caudal": R(.35, .7),
        "crossBlend": R(.35, .65), "unison": R(.3, .8), "unisonDetune": R(.2, .5),
        "filterCutoff": R(.55, .9), "filterReso": R(.15, .5), "drive": R(.1, .4),
        "stereoWidth": R(.4, .8), "delayMix": R(.15, .45), "delayFb": R(.2, .5),
        "reverbMix": R(.1, .35), "outputGain": R(.5, .64)}),
    ("CRYSTAL",    "Crystal Keys",       ["key"], {
        "grainSize": R(.35, .6), "grainDensity": R(.45, .7), "caudal": R(.3, .6),
        "crossBlend": R(.35, .65), "unison": R(.1, .5), "filterCutoff": R(.55, .9),
        "filterReso": R(.12, .4), "stereoWidth": R(.5, .85), "delayMix": R(.1, .35),
        "reverbMix": R(.25, .55), "reverbSize": R(.4, .75), "outputGain": R(.5, .66)}),
    ("RESIN",      "Resin Plucks",       ["pluck", "arp"], {
        "grainSize": R(.18, .38), "grainDensity": R(.5, .8), "caudal": R(.5, .85),
        "soilDensity": R(.3, .6), "crossBlend": R(.35, .65), "filterCutoff": R(.5, .85),
        "filterReso": R(.2, .55), "drive": R(.1, .35), "unison": R(.2, .5),
        "stereoWidth": R(.5, .85), "delayMix": R(.15, .45), "delayFb": R(.2, .5),
        "reverbMix": R(.1, .4), "outputGain": R(.5, .64)}),
    ("PULSE",      "Pulse Arps",         ["arp", "pluck"], {
        "arpOn": 1.0, "arpRate": R(.3, .7), "arpMode": R(0., 1.), "arpSync": R(0., 1.),
        "scaleType": R(0., 1.), "grainSize": R(.18, .35), "grainDensity": R(.5, .8),
        "caudal": R(.5, .85), "crossBlend": R(.35, .65), "filterCutoff": R(.5, .88),
        "filterReso": R(.2, .55), "stereoWidth": R(.4, .8), "delayMix": R(.15, .4),
        "delayFb": R(.2, .5), "outputGain": R(.5, .62)}),
    ("AURORA",     "Aurora Textures",    ["texture", "pad"], {
        "grainSize": R(.6, .98), "grainDensity": R(.5, .9), "spray": R(.3, .7),
        "caudal": R(.15, .45), "soilDensity": R(.55, .9), "modDepth": R(.5, .9),
        "crossBlend": R(.25, .75), "position": R(.2, .8), "filterCutoff": R(.4, .8),
        "stereoWidth": R(.75, 1.), "reverbMix": R(.4, .8), "reverbSize": R(.6, .95),
        "outputGain": R(.48, .62)}),
    ("VOID",       "Void FX",            ["fx", "texture", "key"], {
        "grainSize": R(.3, .9), "grainDensity": R(.4, .85), "spray": R(.3, .8),
        "position": R(.1, .9), "modDepth": R(.4, .9), "crossBlend": R(.2, .8),
        "filterCutoff": R(.35, .85), "filterReso": R(.2, .6), "filterMod": R(.2, .7),
        "stereoWidth": R(.6, 1.), "delayMix": R(.2, .5), "delayFb": R(.25, .55),
        "reverbMix": R(.35, .75), "reverbSize": R(.5, .95), "outputGain": R(.46, .6)}),
    ("CINEMATIC",  "Cinematic",          ["pad", "texture", "key"], {
        "grainSize": R(.6, .98), "grainDensity": R(.45, .8), "caudal": R(.15, .4),
        "soilDensity": R(.6, .92), "modDepth": R(.4, .85), "crossBlend": R(.3, .7),
        "unison": R(.3, .7), "filterCutoff": R(.45, .8), "stereoWidth": R(.75, 1.),
        "reverbMix": R(.45, .85), "reverbSize": R(.65, .98), "reverbDamp": R(.25, .55),
        "outputGain": R(.46, .6)}),
    ("EXPERIMENTAL", "Experimental",     ["pad", "key", "lead", "pluck", "bass", "arp", "texture", "fx"], {
        "grainSize": R(.15, .95), "grainDensity": R(.35, .9), "spray": R(.1, .7),
        "caudal": R(.15, .85), "soilDensity": R(.25, .9), "position": R(.1, .9),
        "modDepth": R(.2, .9), "crossBlend": R(.15, .85), "unison": R(.0, .7),
        "filterCutoff": R(.35, .95), "filterReso": R(.12, .6), "filterMod": R(.0, .6),
        "drive": R(.1, .45), "stereoWidth": R(.3, 1.), "delayMix": R(.0, .45),
        "reverbMix": R(.1, .7), "outputGain": R(.46, .62)}),
]

ADJ = ["Whispering", "Frozen", "Radiant", "Velvet", "Hollow", "Astral", "Molten", "Silken",
       "Crimson", "Emerald", "Obsidian", "Lucid", "Distant", "Fractured", "Golden", "Nocturnal",
       "Drifting", "Ancient", "Electric", "Ghostly", "Weightless", "Shimmering", "Hidden", "Boreal",
       "Sacred", "Liquid", "Phantom", "Woven", "Aching", "Endless", "Twilight", "Solar",
       "Tidal", "Marble", "Cobalt", "Dusklit", "Vaporous", "Spectral", "Coral", "Mercurial",
       "Quiet", "Restless", "Opaline", "Cinder", "Verdant", "Static", "Lunar", "Amber",
       "Wandering", "Breathing", "Crystalline", "Feral", "Serene", "Rusted", "Dreaming", "Iron",
       "Pale", "Deepwater", "Glowing", "Untamed"]
NOUN = ["Aurora", "Cathedral", "Horizon", "Ember", "Mirage", "Glacier", "Halo", "Requiem",
        "Tide", "Bloom", "Cascade", "Monolith", "Reverie", "Vessel", "Nebula", "Sanctum",
        "Pulse", "Lantern", "Meridian", "Cinder", "Prism", "Hollow", "Anthem", "Vertex",
        "Fathom", "Oracle", "Spire", "Lagoon", "Comet", "Verdict", "Chapel", "Willow",
        "Zenith", "Abyss", "Filament", "Grove", "Torrent", "Beacon", "Echo", "Frost",
        "Marrow", "Canopy", "Undertow", "Ascent", "Relic", "Storm", "Chrysalis", "Drift",
        "Aperture", "Solstice", "Vellum", "Mantle", "Current", "Threshold", "Aurelia", "Basin",
        "Corona", "Delta", "Ridge", "Voyage"]

PER_BANK = 1000


def load_genomes():
    with open(os.path.join(DLC, "_genomes.json")) as f:
        man = json.load(f)
    by_role = {}
    for name, m in man.items():
        by_role.setdefault(m["role"], []).append(name)
    for r in by_role:
        by_role[r].sort()
    return by_role


def sample(rng, spec):
    if isinstance(spec, tuple):
        return round(rng.uniform(spec[0], spec[1]), 4)
    return float(spec)


def build_plan():
    by_role = load_genomes()
    plan = []
    for bi, (prefix, bankname, roles, arch) in enumerate(THEMES):
        pool = []
        for r in roles:
            pool += by_role.get(r, [])
        pool.sort()
        rng = random.Random(1000 + bi)
        # unique evocative names for this bank
        combos = [f"{a} {n}" for a in ADJ for n in NOUN]
        rng.shuffle(combos)
        names = combos[:PER_BANK]
        for i in range(PER_BANK):
            grng = random.Random((bi << 20) ^ (i * 2654435761 & 0xFFFFF))
            genome = pool[i % len(pool)]
            params = {}
            for p, spec in arch.items():
                params[p] = sample(grng, spec)
            row = [params.get(pn, dv) for pn, dv in PARAMS]
            plan.append({
                "bank": bankname, "prefix": prefix,
                "name": f"{prefix} > {names[i]}",
                "genome": genome, "params": row,
            })
    return plan


def cmd_build():
    plan = build_plan()
    with open(os.path.join(DLC, "_plan.json"), "w") as f:
        json.dump(plan, f)
    with open(os.path.join(DLC, "_qa_in.tsv"), "w") as f:
        for idx, pr in enumerate(plan):
            vals = "\t".join(f"{v:.4f}" for v in pr["params"])
            f.write(f"{idx}\tsamples/{pr['genome']}.wav\t{vals}\n")
    print(f"built {len(plan)} presets across {len(THEMES)} banks -> dlc/_plan.json, dlc/_qa_in.tsv")


def cmd_finalize():
    with open(os.path.join(DLC, "_plan.json")) as f:
        plan = json.load(f)
    # QA output: idx <tab> newOutputGain <tab> clip% <tab> peak <tab> rms <tab> flags
    gains, flags = {}, {}
    qpath = os.path.join(DLC, "_qa_out.tsv")
    if os.path.exists(qpath):
        with open(qpath) as f:
            for line in f:
                c = line.rstrip("\n").split("\t")
                if len(c) < 6:
                    continue
                gains[int(c[0])] = float(c[1])
                flags[int(c[0])] = c[5]
    banks = {}
    dropped = 0
    for idx, pr in enumerate(plan):
        fl = flags.get(idx, "ok")
        if fl not in ("ok", ""):            # silent / nonfinite -> drop (safety)
            dropped += 1
            continue
        params = {pn: pr["params"][i] for i, (pn, _) in enumerate(PARAMS)}
        if idx in gains:
            params["outputGain"] = round(gains[idx], 4)
        # emit only non-default params to keep the bank compact
        defaults = {pn: dv for pn, dv in PARAMS}
        slim = {k: v for k, v in params.items() if abs(v - defaults[k]) > 1e-6}
        banks.setdefault(pr["prefix"], {"bank": pr["bank"], "presets": []})
        banks[pr["prefix"]]["presets"].append({
            "name": pr["name"], "params": slim, "sample": f"samples/{pr['genome']}.wav",
        })
    total = 0
    for prefix, b in banks.items():
        out = {"bank": b["bank"], "format": 1, "author": "Phenotype", "presets": b["presets"]}
        with open(os.path.join(DLC, f"{prefix}.phbank"), "w") as f:
            json.dump(out, f)
        total += len(b["presets"])
        print(f"  {prefix}.phbank: {len(b['presets'])} presets")
    print(f"finalised {total} presets ({dropped} dropped by QA)")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(DLC, exist_ok=True)
    if cmd == "build":
        cmd_build()
    elif cmd == "finalize":
        cmd_finalize()
    else:
        print("usage: make_dlc.py [build|finalize]")
