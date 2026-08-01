# CLAUDE.md — Phenotype

Guidance for working in `phenotype/`. This subtree is a self-contained VST3/AU
plugin, independent of the MT-40 emulator that shares the repository root.

## What this is

Diploid granular **cross-synthesis** plugin: JUCE 8 / C++20 audio backend +
React 18 / WebGL WebView UI, bridged by pure-JSON IPC. CMake-native (no
Projucer). Botanical metaphor — two "chromosome" source buffers are granulated
and cross-faded; modulation comes from a non-linear "capillary" fill/drain model
instead of LFO/ADSR.

## Build & test

```bash
# Frontend first — emits a single self-contained ui/dist/index.html
cd ui && npm ci && npm run build && cd ..

# Plugin (fetches JUCE 8, or pass -DJUCE_DIR=/path/to/JUCE)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel

# DSP unit tests — standalone, no JUCE, no network
cmake -B build-tests tests && cmake --build build-tests
ctest --test-dir build-tests --output-on-failure

# Frontend checks
cd ui && npm run typecheck   # strict tsc
```

Frontend dev in a plain browser: `cd ui && npm run dev` — a mock backend feeds
synthetic telemetry so the visualiser is fully explorable without JUCE.

## Non-negotiable DSP constraints (audio thread)

Everything reachable from `GranularEngine::process` / `processBlock` MUST be:

- **Allocation-free** — no `new`/`malloc`/`std::vector::push_back`/`resize`. All
  buffers are pre-allocated in `prepare()`. The grain pool is a fixed array.
- **Lock-free** — no mutexes. UI⇄audio parameter transfer goes through
  `dsp::ParameterHub` (`std::atomic<float>`), snapshotted once per block.
- **Trig-free** — no `std::sin`/`std::cos`/`std::exp` in the hot path. Use
  `dsp::fastmath` (`fastExp`, `onePoleCoeff`, `equalPowerPair`, `softClip`).
- **Exception-free / `noexcept`**.

The DSP core under `Source/dsp/` is deliberately JUCE-independent so `tests/`
can compile and verify it standalone with `-Werror`. Keep it that way — do not
`#include` JUCE headers from `Source/dsp/`.

## Capillary modulator (spec contract)

`CapillaryModulator` replaces LFO/ADSR with three phases, implemented as
first-order RC recursions:

1. **Absorption** — `y = 1 − e^{−t/τA}`, `τA` from **Caudal** (flow).
2. **Saturation** — instantaneous state flip at substrate capacity.
3. **Drainage** — `y = e^{−t/τD}`, `τD` from **Densidad del Suelo** (density).

## Parameters

`Source/Parameters.h` is the single source of truth (12 normalised 0..1 params).
The APVTS is host-facing (automation + preset persistence); the audio thread
reads cached `std::atomic<float>*` and syncs them into the hub each block. Adding
a parameter = add one `Def` there; the UI, IPC, and persistence follow the id.

## IPC contract (pure JSON)

- **UI → host**: native function `phenotypeSend` — `{type:"param",id,value}` or
  `{type:"batch",params:{...}}`. Applied via `setValueNotifyingHost`.
- **host → UI**: events `phenotypeTelemetry` (`{fft:[...],capillary,activeGrains}`)
  and `phenotypeParams` (`{params:{...}}`, for host automation / preset recall).

UI edits route through the APVTS, and inbound `phenotypeParams` are applied to
the Zustand store **without** echoing back (and skip the param under active
drag) to avoid feedback loops.

## Frontend rules

- Per-frame FFT lives in the **transient** `telemetry` buffer (mutable, outside
  React), read inside r3f `useFrame`. Never put per-frame FFT in React state —
  it would re-render the tree 30–60×/s.
- Camera is **orthographic** (isometric). Palette is fixed C40:
  `#F4F4F4` field · `#222222` ink · `#00FF00` chlorophyll · `#FF00FF` magenta
  (`src/three/theme.ts`).
- The JUCE frontend helper is vendored at `src/juce/` (no npm package exists);
  import `getNativeFunction` from there.

## Conventions

- Do not commit build artifacts: `ui/node_modules`, `ui/dist`, `build*/` are
  ignored. `ui/package-lock.json` IS committed.
- Match the existing terse, comment-headed style of each file.
