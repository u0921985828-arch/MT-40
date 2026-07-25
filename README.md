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

## UI — WebView (`Source/UI/web`)

The editor is a **JUCE 8 WebView** (`juce::WebBrowserComponent`) rendering an
embedded HTML/CSS/JS front-end. The web assets are compiled into the binary
with `juce_add_binary_data` and served at runtime by a `ResourceProvider`, so
the plugin stays fully self-contained.

- `index.html` / `style.css` — the vintage Minimoog layout (walnut frame, dark
  metal panels, pointer knobs, coloured rocker switches) in the five classic
  sections: **Controllers**, **Oscillator Bank**, **Mixer**, **Modifiers**
  (filter + filter envelope) and **Output** (loudness envelope + master), plus
  an on-screen keyboard.
- `js/main.js` — builds the knobs/switches/combos and binds each to its
  parameter through the JUCE frontend library, and renders the real-time
  **oscilloscope + spectrum** display.
- `js/juce/index.js` — the JUCE WebView frontend library (vendored).

The visualiser is fed from the audio thread: the processor writes the output
into a lock-free ring buffer (`readScope`), and a 30 Hz timer in the editor
computes an FFT and pushes both the waveform and a log-spaced spectrum to the
page via `emitEventIfBrowserIsVisible("visualiser", …)`.

Every control is bridged to the `AudioProcessorValueTreeState` in
`PluginEditor.cpp`: each parameter gets a relay
(`WebSliderRelay` / `WebToggleButtonRelay` / `WebComboBoxRelay`) and a matching
`Web…ParameterAttachment`, generated automatically from the parameter list. The
on-screen keyboard calls back into the processor through native functions
(`noteOn` / `noteOff`). Parameter IDs live in `Source/Parameters.h`.

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

On Debian/Ubuntu, JUCE needs the usual GUI/audio dev packages plus GTK and
WebKitGTK for the WebView editor:

```bash
sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxcomposite-dev libasound2-dev libfreetype6-dev libfontconfig1-dev \
  libgl1-mesa-dev libgtk-3-dev libwebkit2gtk-4.1-dev
```
