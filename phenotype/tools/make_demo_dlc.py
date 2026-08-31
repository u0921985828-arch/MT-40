#!/usr/bin/env python3
"""Generate a demo Phenotype DLC bank: a few HQ genome samples (synthesised
WAVs) plus a .phbank that references them.  Drop the produced folder into
~/Documents/Phenotype/Presets (or use the plugin's Import button) and the
sample-backed presets play genuinely different timbres.

    python3 tools/make_demo_dlc.py            # -> banks/DEMO_DLC/

No third-party deps (stdlib wave/struct/math)."""

import json, math, os, struct, wave, sys

SR = 22050
DUR = 2.0

def write_wav(path, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    norm = 0.89 / peak
    with wave.open(path, "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * norm)) * 32767)) for s in samples))

def saw(f, t):           # bandlimited-ish additive saw
    return sum((1.0 / k) * math.sin(2 * math.pi * f * k * t) for k in range(1, 18))

def gen_resin_saw():     # fat detuned triple saw
    n = int(SR * DUR); out = []
    for i in range(n):
        t = i / SR
        s = saw(110, t) + 0.8 * saw(110 * 1.006, t) + 0.8 * saw(110 * 0.994, t)
        out.append(s * (0.6 + 0.4 * math.sin(2 * math.pi * 0.5 * t)))
    return out

def gen_crystal_fm():    # glassy FM bell
    n = int(SR * DUR); out = []
    for i in range(n):
        t = i / SR
        env = math.exp(-2.2 * (t % 1.0))
        mod = 3.0 * env * math.sin(2 * math.pi * 440 * 2.01 * t)
        out.append(env * math.sin(2 * math.pi * 440 * t + mod))
    return out

def gen_terp_pluck():    # square-ish pluck body
    n = int(SR * DUR); out = []
    for i in range(n):
        t = i / SR
        env = math.exp(-3.5 * (t % 0.5))
        s = sum((1.0 / k) * math.sin(2 * math.pi * 220 * k * t) for k in range(1, 20, 2))
        out.append(env * s)
    return out

# A reusable pad/lead/bass parameter body; the genome sample supplies the timbre.
PAD  = {"grainDensity":0.66,"grainSize":0.8,"caudal":0.3,"soilDensity":0.7,"modDepth":0.7,
        "crossBlend":0.5,"unison":0.5,"unisonDetune":0.3,"filterCutoff":0.7,"drive":0.12,"stereoWidth":0.85}
BASS = {"grainDensity":0.55,"grainSize":0.34,"pitchA":0.4,"pitchB":0.4,"crossBlend":0.5,
        "filterCutoff":0.42,"filterReso":0.4,"drive":0.3,"unison":0.15,"stereoWidth":0.3,"outputGain":0.5}
PLUCK= {"grainDensity":0.7,"grainSize":0.22,"caudal":0.6,"soilDensity":0.4,"crossBlend":0.5,
        "filterCutoff":0.66,"filterReso":0.45,"drive":0.2,"unison":0.3,"stereoWidth":0.7}

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "banks", "DEMO_DLC")
    out_dir = os.path.abspath(out_dir)
    samp_dir = os.path.join(out_dir, "samples")
    os.makedirs(samp_dir, exist_ok=True)

    write_wav(os.path.join(samp_dir, "resin_saw.wav"),   gen_resin_saw())
    write_wav(os.path.join(samp_dir, "crystal_fm.wav"),  gen_crystal_fm())
    write_wav(os.path.join(samp_dir, "terp_pluck.wav"),  gen_terp_pluck())

    bank = {
        "bank": "DEMO DLC",
        "format": 1,
        "author": "Phenotype",
        "presets": [
            {"name": "Resin Saw Pad",    "params": PAD,   "sample": "samples/resin_saw.wav"},
            {"name": "Resin Saw Bass",   "params": BASS,  "sample": "samples/resin_saw.wav"},
            {"name": "Crystal FM Key",   "params": PAD,   "sample": "samples/crystal_fm.wav"},
            {"name": "Crystal FM Pluck", "params": PLUCK, "sample": "samples/crystal_fm.wav"},
            {"name": "Terpene Pluck",    "params": PLUCK, "sample": "samples/terp_pluck.wav"},
            {"name": "Built-in Genome",  "params": PAD},   # no sample -> internal genome
        ],
    }
    with open(os.path.join(out_dir, "Demo.phbank"), "w") as f:
        json.dump(bank, f, indent=2)
    print("wrote", out_dir, "->", len(bank["presets"]), "presets,", 3, "samples")

if __name__ == "__main__":
    main()
