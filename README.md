# Sleng Teng ST-40 — VST/AU Emulator

A mathematically accurate, **PCM-free** emulation of a legendary 1981
mini rhythm keyboard, built in C++ with the [JUCE](https://juce.com) framework.
Every sound generator — melodic voices and the analog rhythm section alike —
is synthesized in real time from DSP first principles. No samples are used.

> The **ST-40** is named after the **Sleng Teng** riddim: the keyboard's factory
> "Rock" rhythm + bass line is where that legendary sound was born. That preset
> is modelled here from scratch, unbranded.

## Features

* **Melodic CV synthesis (§3)** — 8-voice pool of dual-pulse DCO voices with
  per-preset duty cycles, fixed inter-oscillator detune (chorused fatness), a
  static 1-pole analog LPF, and the two-stage (linear-attack /
  exponential-release) VCA envelope. Oldest-note voice stealing.
* **Analog rhythm section (§4)** — Twin-T resonant kick, parallel body+noise
  snare, and choke-grouped closed/open hi-hats, all driven by a shared LFSR
  noise source and impulse excitation.
* **Sleng Teng engine (§5)** — hardcoded 2-bar / 16-steps-per-bar sequencer,
  free-running clock, dedicated monophonic square-wave bass, and the
  Synchro / Fill-in transport state machine.
* **Auto-Chord (§5, §7)** — one-finger Major / Minor / 7th / Minor-7th
  detection. See [`docs/CHORD_TRUTH_TABLE.md`](docs/CHORD_TRUTH_TABLE.md).
* **APVTS + MIDI CC map (§6)** — all parameters automatable and CC-controllable.
* **Native WebView GUI** — the control surface is authored in HTML/CSS/JS
  (`ui/index.html`) and rendered in a JUCE 8 `WebBrowserComponent` with native
  integration. Each parameter is bridged via a typed relay + attachment, so the
  web widgets, the APVTS, and host automation stay in sync. The UI (including
  JUCE's own front-end JS library) is embedded as `BinaryData` and served by an
  in-process resource provider — no web server, no network access.

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

JUCE 8.0.4 is fetched automatically. Reuse a local checkout with
`-DJUCE_DIR=/path/to/JUCE`. Produces VST3, AU (macOS), and a Standalone build.

The GUI uses the platform native WebView, so building with `JUCE_WEB_BROWSER=1`
requires:

* **macOS** — WKWebView (system, no extra dependency).
* **Windows** — the WebView2 runtime (pre-installed on Win 10/11).
* **Linux** — WebKitGTK dev package, e.g. `libwebkit2gtk-4.1-dev`.

## Parameter / MIDI CC map (§6)

| Control | CC | Parameter ID | Range |
| :-- | :-: | :-- | :-- |
| Main Volume | 7 | `master_vca_gain` | 0.0 – 1.0 |
| Rhythm Volume | 12 | `rhythm_bus_gain` | 0.0 – 1.0 |
| Bass Volume | 13 | `bass_osc_gain` | 0.0 – 1.0 |
| Tempo | — | `host_bpm` | 40 – 240 BPM |
| Vibrato | 1 | `global_lfo_on` | Off / On (5.5 Hz) |
| Sustain | 64 | `env_release_mult` | Short / Long (3×) |
| Mode | 85 | `kbd_mode` | Off / Play / Chord |
| Rhythm Select | 86 | `active_rhythm_idx` | 0 – 5 |
| Preset | PC | `active_patch_idx` | 0 – 21 |

## Layout

```
ui/
  index.html                WebView control surface (HTML/CSS/JS)
  js/juce/                  JUCE front-end JS library (embedded)
Source/
  PluginProcessor.{h,cpp}   top-level: MIDI, split logic, mixing
  PluginEditor.{h,cpp}      WebView host: relays + parameter attachments
  Parameters.{h,cpp}        APVTS layout (§6)
  dsp/
    DCOVoice.h              melodic voice (§3.1)
    OnePoleLPF.h            static analog LPF (§3.2)
    TwoStageEnvelope.h      CV VCA envelope (§3.3)
    ExpDecay.h              percussion VCA
    Biquad.h                RBJ biquad (BPF/HPF/LPF)
    LFSRNoise.h             shared noise source (§4)
    VoiceAllocator.h        8-voice pool (§2)
    TwinTResonator.{h,cpp}  kick (§4.1)
    SnareDrum.{h,cpp}       snare (§4.2)
    HiHat.{h,cpp}           hi-hat + choke (§4.3)
    SlengTengBass.{h,cpp}   mono bass (§5.1)
    RhythmEngine.{h,cpp}    sequencer + state machine (§5)
    AutoChord.{h,cpp}       chord detection (§5, §7)
    Presets.h               the 22 tone presets
docs/
  ARCHITECTURE.md
  CHORD_TRUTH_TABLE.md
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full signal flow and
module map.
