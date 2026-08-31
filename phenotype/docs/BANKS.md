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

## Authoring a 10k pack

1. Gather / render your source samples (one genome per timbre) into `samples/`.
2. Emit a `.phbank` whose presets reference them with varied `params` (a
   generator script is the practical way to produce thousands).
3. Validate levels with the offline QA (`tools/` — render each preset through
   the DSP core and check nothing rides the soft-clipper) before release.
4. Ship the folder; users drop it in or Import it.
