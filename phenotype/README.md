# Phenotype

Diploid granular **cross-synthesis** VST3/AU plugin built on botanical metaphors.
JUCE 8 / C++20 audio backend, React 18 + WebGL (`@react-three/fiber`) WebView UI,
pure-JSON bidirectional IPC. CMake-native (no Projucer).

```
phenotype/
├── CMakeLists.txt              # JUCE 8, C++20, WebView, embeds ui/dist
├── Source/
│   ├── PluginProcessor.*       # AudioProcessor + FFT analysis publisher
│   ├── PhenotypeWebEditor.*    # WebView bridge: resource provider + IPC + telemetry timer
│   ├── ipc/MessageDispatcher.h # JSON UI⇄DSP router
│   └── dsp/
│       ├── FastMath.h          # trig-free fastExp / one-pole / equal-power
│       ├── CapillaryModulator.h# non-linear fill/saturate/drain modulator (spec §3)
│       ├── Grain.h             # diploid grain voice (reads chromosome A & B)
│       ├── ParameterHub.h      # lock-free std::atomic parameter bridge
│       └── GranularEngine.*    # allocation-free granular cloud
└── ui/                         # Vite + React + Three.js + Zustand frontend
    └── src/
        ├── bridge/juceIntegration.ts  # window.juceIntegration (native fn + telemetry)
        ├── store/usePhenotypeStore.ts # Zustand: reactive params + transient FFT buffer
        └── three/                     # orthographic isometric grid, nodes, splines
```

## The three execution vectors

**Vector 1 — Architecture (C++ / CMake).** `CMakeLists.txt` fetches JUCE 8,
targets C++20, embeds the built frontend as `BinaryData`. `PhenotypeWebEditor`
hosts a `juce::WebBrowserComponent` with a resource provider, a `phenotypeSend`
native function (UI→DSP), and a 30 Hz timer that emits `phenotypeTelemetry`
(DSP→UI). `MessageDispatcher` translates pure-JSON frames both ways.

**Vector 2 — DSP.** `CapillaryModulator` replaces LFO/ADSR with a capillary
model: logarithmic **Absorption** (capacitor charge `1 − e^{−t/τA}`, param
*Caudal*), instantaneous **Saturation** at substrate capacity, exponential
**Drainage** (`e^{−t/τD}`, param *Densidad del Suelo*) — implemented as
first-order recursions with a trig-free polynomial `fastExp`. `GranularEngine`
is a fixed-pool, allocation-free diploid cloud: two source ring buffers
(chromosome A/B) are cross-faded per grain; parameters arrive through the
`std::atomic` `ParameterHub`, snapshotted once per block.

**Vector 3 — Frontend (React + Three.js).** Orthographic (isometric) Three.js
scene. `window.juceIntegration` streams FFT + capillary phase + grain count into
a Zustand store split into a reactive tier (params/HUD) and a transient buffer
read inside `useFrame` so per-frame FFT never re-renders React. Nodes pulse with
band energy; splines (quadratic béziers) brighten with their band and shimmer
with the capillary phase. Palette: `#F4F4F4` field, `#222222` ink, chlorophyll
`#00FF00`, LED magenta `#FF00FF`.

## Build

```bash
# 1) Frontend (emits a single self-contained ui/dist/index.html)
cd ui && npm ci && npm run build && cd ..

# 2) Plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Pass `-DJUCE_DIR=/path/to/JUCE` to reuse a local JUCE 8 checkout instead of the
`FetchContent` download. Formats produced: VST3, AU (macOS), Standalone.

## Frontend dev (browser mock)

```bash
cd ui && npm run dev
```

Outside JUCE, `check_native_interop.js` installs a placeholder backend and
`juceIntegration` falls back to a self-animating telemetry mock, so the full
visualiser is explorable in a plain browser tab.
