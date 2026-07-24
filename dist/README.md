# Artifacts ST-40 — Standalone web emulator (with sound)

`st40-emulator-sound.html` is a **self-contained, single-file** build of the
ST-40 panel that produces **real sound in the browser** — no samples, no
external files, no network. The plugin's DSP (dual-pulse DCOs, one-pole LPF,
two-stage envelope, auto-chord accompaniment, analog rhythm section with
Twin-T kick / BPF+LFSR snare / LFSR hi-hat, and the mono bass line) is ported
to JavaScript and rendered live via Web Audio.

## Use it

Open `st40-emulator-sound.html` in any modern browser (double-click is fine)
and **tap the on-screen keys** (or press a key) once — browsers require a user
gesture before audio can start. The hint chip disappears when sound is live.

- Numbered strip (1–22): select tone.
- Bass zone (left keys, below the split): plays the auto-chord accompaniment;
  the rhythm/bass follows the detected chord root.
- **synchro / fill-in** (orange) and **start/stop** (beige) drive the rhythm
  transport; the tempo lamp flashes at the current BPM.
- Knobs: master volume, tempo, rhythm/bass volume. Switches: sustain, vibrato,
  tone-memory (1–4) and play/set mode.

## Audio path

The page prefers an **AudioWorklet**; if the worklet module can't load — which
is the case when the file is opened directly from disk (`file://`), where
browsers block Blob-URL worklets — it transparently falls back to a
**ScriptProcessorNode** running the same DSP on the main thread. Result: it
makes sound both locally (`file://`) and when served over http(s).

## Rebuild

Regenerate from the canonical UI and DSP with:

```
python3 tools/build_web_sound.py
```

This reads `ui/index.html`, embeds the ported DSP, and writes
`dist/st40-emulator-sound.html`.
