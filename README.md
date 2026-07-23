# MoogVA Synth

A polyphonic virtual-analog synthesizer VST/AU plugin built with
[JUCE](https://juce.com), modelled on the classic **Minimoog Model D**
signal path: three anti-aliased oscillators, a resonant 4-pole transistor
ladder filter, two ADSR envelopes, glide/portamento and a soft-limited
master output.

## Signal path

```
3x Oscillators ─┐
Noise ──────────┼─► Mixer ─► Ladder Filter (24 dB/oct) ─► Amp VCA ─► Master + Limiter
Feedback ───────┘            ▲                              ▲
                        Filter Env (ADSR)              Amp Env (ADSR)
```

## Modules (`Source/DSP`)

| File | Role |
| --- | --- |
| `MoogOscillator.h` | PolyBLEP band-limited oscillator — Triangle, Saw, Square, Wide & Narrow Pulse, with per-octave foot ranges (LO/32'/16'/8'/4'/2'). |
| `MoogLadderFilter.h` | Zero-delay-feedback 4-pole ladder low-pass, `tanh` non-linearity in the resonance path, 2x oversampled, capable of self-oscillation. |
| `EnvelopeGenerator.h` | Analog-style ADSR with exponential segments. |
| `NoiseGenerator.h` | White noise + Paul-Kellet pink noise. |
| `SynthVoice.h/.cpp` | One voice: 3 oscillators, noise, filter, 2 envelopes, glide and the resonant feedback path. |
| `PolyphonicSynthesizer.h` | 16-voice manager (built on `juce::Synthesiser`, handles MIDI + voice stealing). |
| `SynthParameters.h` | Cached atomic pointers into the APVTS for lock-free audio-thread reads. |

## UI (`Source/UI`)

`LookAndFeelMoog` renders vintage black pointer knobs, coloured rocker
switches and a walnut-framed dark-metal chassis. The editor is laid out in the
five classic Minimoog sections: **Controllers**, **Oscillators Bank**,
**Mixer**, **Modifiers** (filter + filter envelope) and **Output** (loudness
envelope + master volume), plus an on-screen keyboard.

Every control is bound to the `AudioProcessorValueTreeState` defined in
`PluginProcessor::createParameterLayout()` (see `Source/Parameters.h` for the
parameter IDs).

## Building

Requires CMake ≥ 3.22 and a C++17 compiler. JUCE is fetched automatically via
`FetchContent` (or point `-DJUCE_DIR=` at a local checkout).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts (VST3 / AU / Standalone) are written under
`build/MoogVASynth_artefacts/Release/`.

### Linux dependencies

On Debian/Ubuntu, JUCE needs the usual GUI/audio dev packages:

```bash
sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxcomposite-dev libasound2-dev libfreetype6-dev libfontconfig1-dev \
  libgl1-mesa-dev
```
