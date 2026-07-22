# Casio MT-40 — DSP Emulation Architecture

A real-time, **sample-free** emulation of the 1981 Casio MT-40 (Casiotone
MT-40). Every generator is synthesized from DSP first principles — there is no
PCM playback anywhere in the signal path.

## Signal flow

```
                         ┌────────────────────────── Melodic pool (8 voices) ──────────────────────────┐
 MIDI ──► split logic ──►│  DCOVoice ×8                                                                 │
          (§2, §5)       │   ├─ pulse osc 1 (duty, phase acc)                                           │
                         │   ├─ pulse osc 2 (duty, +detune)  ─► Σ ─► 1-pole analog LPF ─► 2-stage VCA ──┼─► ×0.5 ┐
                         │   └─ VoiceAllocator: oldest-note stealing                                    │       │
                         └─────────────────────────────────────────────────────────────────────────────┘       │
                                                                                                                 ├─► ×master ─► out
                         ┌──────────────── Rhythm bus (analog models) ────────────────┐                          │
 Casio Chord ──► root ──►│  RhythmEngine (sequencer + Synchro/Fill state machine)     │                          │
 (§5, truth table)       │   ├─ TwinT_Resonator  (kick,  §4.1)                        │                          │
                         │   ├─ SnareDrum        (body + LFSR wires, §4.2) ──► ×rhythmGain ─────────────────────┤
                         │   ├─ HiHat            (LFSR + 4th-order HP + choke, §4.3)   │                          │
                         │   └─ SlengTengBass    (square→12dB LPF→pluck, §5.1) ──► ×bassGain ────────────────────┘
                         └────────────────────────────────────────────────────────────┘
```

## Module map

| Spec section | Module | File |
| :-- | :-- | :-- |
| §3.1 DCO / CV | `DCOVoice` | `Source/dsp/DCOVoice.h` |
| §3.2 Static LPF | `OnePoleLPF` | `Source/dsp/OnePoleLPF.h` |
| §3.3 Two-stage VCA | `TwoStageEnvelope` | `Source/dsp/TwoStageEnvelope.h` |
| §2 Polyphony | `VoiceAllocator` (8, oldest-steal) | `Source/dsp/VoiceAllocator.h` |
| §4.1 Kick | `TwinT_Resonator` | `Source/dsp/TwinTResonator.{h,cpp}` |
| §4.2 Snare | `SnareDrum` | `Source/dsp/SnareDrum.{h,cpp}` |
| §4.3 Hi-hat + choke | `HiHat` | `Source/dsp/HiHat.{h,cpp}` |
| §4 Noise source | `LFSRNoise` (shared) | `Source/dsp/LFSRNoise.h` |
| §5.1 Sleng Teng bass | `SlengTengBass` | `Source/dsp/SlengTengBass.{h,cpp}` |
| §5 Sequencer + §5.2 state machine | `RhythmEngine` | `Source/dsp/RhythmEngine.{h,cpp}` |
| §5 Casio Chord | `CasioChord` | `Source/dsp/CasioChord.{h,cpp}` |
| §6 APVTS params | `createParameterLayout` | `Source/Parameters.{h,cpp}` |
| §1-§7 Top level | `CasioMT40AudioProcessor` | `Source/PluginProcessor.{h,cpp}` |

## Key modelling decisions

* **DCO** — two independent phase accumulators produce two pulse waves whose
  duty cycles come from the active preset; a fixed `detuneCents` offset between
  them gives the inherent chorused fatness (§3.1). Biquad-free, one-pole RC LPF
  after summation (§3.2).
* **Two-stage envelope** — linear attack, exponential release, *no* separate
  decay stage (CV synthesis holds at peak while the key is down). The Sustain
  toggle multiplies the release time constant by **3×** (§3.3).
* **Kick** — a band-pass biquad at 55 Hz / Q 18.5 excited by a 1 ms unit
  impulse; the high Q gives the ~250 ms ring. `tanh` drive at the output
  models transistor saturation (§4.1).
* **Snare / Hi-hat noise** — a shared 32-bit Galois **LFSR** feeds the snare
  wires (2.5 kHz HP, ~150 ms decay) and the hi-hat (4th-order 8 kHz HP). The
  hi-hat choke hard-resets the open-hat envelope on any closed-hat trigger
  (§4.2, §4.3).
* **State machine** — `STOPPED → ARMED → PLAYING`. Fill-in held ⇒ constant
  1/4-note kicks; a double-tap-and-hold ⇒ 1/8-note snares; release returns to
  the normal pattern on the next bar line (§5.2).

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

JUCE 8.0.4 is fetched automatically via `FetchContent`. To reuse a local
checkout: `cmake -B build -DJUCE_DIR=/path/to/JUCE`. Targets: VST3, AU
(macOS) and a Standalone app.
