# Phenotype banks & DLC (`.phbank`)

Phenotype loads its 876 factory presets from the compiled binary, then **scans a
user folder for `.phbank` bank files** and appends their presets. This is how
DLC / imported libraries scale to tens of thousands of presets without bloating
the plugin, and how each preset can carry its **own genome sample** for
genuinely different, HQ timbres.

## Where banks live

```
~/Documents/Phenotype/Presets/
    MyPack/
        MyPack.phbank
        samples/
            og_kush.wav
            ...
```

- Drop a bank **folder** (the `.phbank` plus its `samples/`) into that directory
  and the plugin picks it up on the next open, or when you press **Import (＋)**
  / rescan.
- The **Import (＋)** button in the preset bar opens a native chooser; pick a
  `.phbank` file or a DLC folder and it is copied into the library and rescanned.
- Sample paths are **relative to the `.phbank` file**, so keep `samples/`
  alongside it.

## Format

```jsonc
{
  "bank": "MY LIBRARY",          // shown before ">" in preset names
  "format": 1,
  "author": "You",               // optional metadata
  "presets": [
    {
      "name": "Emerald Cathedral",           // "LIB > name" if it has " > ", else bank is prefixed
      "params": {                            // any of the 31 parameter ids, normalised 0..1
        "grainDensity": 0.68,
        "filterCutoff": 0.65,
        "drive": 0.14,
        "outputGain": 0.5
      },
      "sample": "samples/og_kush.wav"        // optional; omit -> built-in wavetable genome
    }
  ]
}
```

- **`params`** — only the ids you set; everything else falls back to its
  default. All values are the normalised `0..1` the plugin uses (see
  `Source/ParameterHub.h` for the id list and what each maps to).
- **`sample`** — optional mono/stereo audio file used as that preset's granular
  genome (first ~8 s, folded to mono, peak-normalised). This is the source of
  HQ / "totally different" timbres. Omit it and the preset plays the internal
  wavetable genome.
- **`outputGain`** — keep loud presets off the soft-clip ceiling. The factory
  bass/lead presets sit around `0.3..0.6`; pads around the `0.8` default.

## Tooling

```bash
# Export the compiled factory to per-library .phbank banks (format template)
python3 tools/export_factory_bank.py [out_dir]      # default dist/banks/Factory

# Build the bundled demo DLC (synthesised HQ samples + a .phbank)
python3 tools/make_demo_dlc.py                       # -> banks/DEMO_DLC/
```

A ready-to-import demo lives in [`banks/DEMO_DLC/`](../banks/DEMO_DLC): copy that
folder into `~/Documents/Phenotype/Presets/` (or use Import) and browse the
`DEMO DLC > …` presets — five are sample-backed, one plays the built-in genome.

## The 10k DLC pack (`ASTRAL 10K`)

A ready pipeline produces a **10,000-preset** pack across 10 themed banks, each
preset a synthesised **HQ 44.1 kHz genome** crossed with a seeded parameter
genotype, QA-levelled so none rides the clipper. Everything is deterministic
(seeded) and regenerable — the built pack is shipped as a zip, not committed.

```bash
# 1. Synthesise the HQ genome palette (~338 loop-safe 44.1 kHz WAVs)
python3 tools/make_dlc_genomes.py                 # -> dlc/samples/*.wav + _genomes.json

# 2. Compose 10 banks x ~1000 presets (plan + QA input)
python3 tools/make_dlc.py build                   # -> dlc/_plan.json, dlc/_qa_in.tsv

# 3. QA: render every preset through the real DSP, recalibrate outputGain
g++ -O2 -std=c++20 -I Source -I Source/dsp tools/qa_dlc.cpp \
    Source/dsp/GranularEngine.cpp -o qa_dlc
./qa_dlc dlc/_qa_in.tsv dlc/_qa_out.tsv dlc

# 4. Write the QA-corrected .phbank banks
python3 tools/make_dlc.py finalize                # -> dlc/<BANK>.phbank
```

Banks: **NEBULA** (pads), **SUBMARINE** (bass), **SOLAR** (leads), **CRYSTAL**
(keys), **RESIN** (plucks), **PULSE** (arps), **AURORA** (textures), **VOID**
(fx), **CINEMATIC**, **EXPERIMENTAL**. Genomes are loop-crossfaded (no click),
sustained timbres — attack/decay is shaped by the engine, not baked in.

To ship: zip `dlc/` (the 10 `.phbank` + shared `samples/`) and drop it in
`~/Documents/Phenotype/Presets/`, or Import any single `.phbank`.

### Authoring your own pack

1. Render your source samples (one genome per timbre) into `samples/`.
2. Emit a `.phbank` whose presets reference them with varied `params`.
3. Validate levels with the offline QA above before release.
4. Ship the folder; users drop it in or Import it.
