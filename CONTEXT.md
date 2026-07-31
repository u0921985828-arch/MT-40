# MoogVA Synth — Contexto y desglose del proyecto

Sintetizador virtual-analógico estilo **Minimoog Model D**, con capa de sampler,
performance (acordes/arp), rack de FX y salida MIDI. Un solo motor/diseño con
**dos entregables**:

1. **Plugin JUCE 8** (C++17) — VST3 / AU / Standalone (+ AAX opcional), UI en WebView.
2. **Web app móvil** de un solo archivo (`webapp/index.html` → `moogva-mobile.html`),
   que suena de verdad (Web Audio) y se publica como **Artifact** privado y como **APK**.

> Nota de marca: "Moog"/"Minimoog" son marcas registradas de Moog Music. Para
> comercializar hay que **renombrar** (evitar "Moog" y el trade dress del Model D).

---

## 1. Estado actual (resumen)

Todo compila y está verificado. Paridad plugin ↔ web salvo lo indicado.

**Síntesis:** 3 osciladores VA (PolyBLEP), ruido (white/pink), filtro **ladder
Moog ZDF** (TPT, feedback-tanh, 2× oversampling interno, drive + bass-thin),
ADSR exponencial ×2 (filtro/amp), glide, drift analógico, mod bus (osc/filtro),
**mono/poly (8 voces)**.

**Capa Sample:** 8 timbres wavetable generados aditivamente (Digital/Organ/Voice/
Reed/Bell/Pluck/Glass/Saw2), mezclados en el mixer. En **web**, además, slot
**"User"** que **importa tu WAV** (`decodeAudioData`, pitcheado + loop).
*(Import de WAV en el plugin: pendiente — requiere puente nativo file-chooser.)*

**Perform (inspirado en LEADR):**
- **Chord lock**: 10 tipos (Off/Oct/5th/Power/Maj/Min/Sus4/Maj7/Min7/Add9).
- **Arpegiador**: modos Up/Down/Up-Down/Random/As-Played/**Cascade** (wash
  sostenido tipo WATERFALL), 1/4–1/32 (+ tresillos), 1–4 octavas, gate, BPM
  (sync a tempo de host o propio).
- **MIDI-OUT** (solo plugin): el acorde/arp sale como MIDI para tocar otros
  synths (Serum/Diva…) y silencia el audio interno.
- **RANDOM**: randomización musical del patch completo.

**FX rack (master):** Drive · Chorus · Phaser · Crush (bitcrush) · Tone (tilt) ·
Delay (ping-pong) · Reverb — cada uno on/off + cantidad, con **FX RANDOM** musical.

**Máster:** shelves warmth/air + tube tanh sobre-oversampleado 4× + DC blocker.

**UI:** madera "Universal-Audio", 7 secciones (Controllers · Oscillator Bank ·
Mixer · Modifiers · Output · FX · Perform), selectores LCD ámbar, placas con
tornillos, Scope + Spectrum + VU. Plugin 1520×820 (redimensionable). Web: modo
**Auto pantalla completa** (sin letterbox), lienzo de diseño 1000 de alto.

**Presets:** 10 bibliotecas, ~700 presets (`webapp/presets.json`, embebido).

---

## 2. Arquitectura

### Plugin (`Source/`)
- `Parameters.h` — IDs APVTS + `ParamChoices` (waveforms, ranges, chordTypes,
  chordIntervals, arpRates/Modes/Octaves, sampleTables). Params de performance,
  sample y FX.
- `PluginProcessor.{h,cpp}` — APVTS; banco de **8 voces** `std::array<MonoSynthEngine,8>`
  (`allocateVoice`/`findVoiceForNote`, MIDI por voz); `createParameterLayout`;
  `applyPerform()` (expansión de acordes + arpegiador sample-accurate, incl.
  Cascade con `perfCasQueue`); **MIDI-out** (mutea audio, deja los eventos en
  `midiMessages`); master (shelves + tube 4× OS + DC); `FxRack fxRack`.
- `DSP/`
  - `MonoSynthEngine.{h,cpp}` — voz mono (last-note priority); osc/noise/ladder/
    env; **capa wavetable** (`waveBank()` estático, lectura a pitch del Osc 1).
  - `MoogOscillator.h`, `MoogLadderFilter.h`, `EnvelopeGenerator.h`, `NoiseGenerator.h`.
  - `SynthParameters.h` — cache de punteros atómicos APVTS para el hilo de audio.
  - **`FxRack.h`** — Drive (tanh) · Chorus (juce::dsp) · Phaser (4 allpass +LFO) ·
    Bitcrush (cuantización) · Tone (1-pole LP) · Delay ping-pong (manual) ·
    Reverb (juce::Reverb). `process(buffer, Params)`.
- `PluginEditor.{h,cpp}` — WebView, relays auto por parámetro (slider/toggle/combo),
  native functions (noteOn/off, pitchBend, getPresetBank/loadPreset/savePreset),
  timer 30 Hz que empuja el visualiser.
- `PresetManager.h` — banco `{libraries:[...]}` + "User"; parsea `Lib|||Cat|||Name`;
  `camelToId` (incluye sample/FX); `resetToDefaults` **excluye** performance
  (poly/chord/arp/midiOut).
- `UI/web/` — `index.html`, `js/main.js` (knobs canvas, dropdowns, preset tree,
  keyboard, visualiser, **randomize**), `style.css`. Embebido con
  `juce_add_binary_data` (incluye `webapp/presets.json`).

### Web app (`webapp/index.html`, ~1 archivo)
- `Osc/Noise/Ladder/Env` (port JS del C++), **`Engine`** (voz mono + capa sample/
  wavetable + WAV de usuario), **`Poly`** (8 Engine).
- **`Perf`** — acordes + scheduler de arp (incl. Cascade) sobre el motor; `Perf`
  enruta el teclado. **`randomize` / `randomizeFX`**.
- **`buildFX`** — cadena Web Audio: drive→chorus→phaser→crush→tone→delay→reverb,
  tras el bus de máster (WaveShaper/BiquadShelves/Haas/Compressor).
- **Scope/Spectrum** vía `AnalyserNode`; VU vía `engine.meter`.
- `#stage` con transform rotate/scale (landscape-lock) + modo **Auto** que ajusta
  el aspecto del lienzo al del dispositivo.
- Banco de 10 bibliotecas embebido (`<script id="bank">`, idéntico a `presets.json`).
- `window.__moog` — hook de QA (accesores de solo lectura a `P`/`engine`/`Perf`).

---

## 3. Parámetros clave (nombres)

Perform: `CHORD_TYPE`, `ARP_ON/RATE/MODE/OCT/GATE/BPM`, `MIDI_OUT_ON` (plugin),
`POLY_ON`. Sample: `SAMPLE_ON/SEL/VOL`. FX: `FX_{DRIVE,CHORUS,PHASER,CRUSH,TONE,
DELAY,REVERB}_ON` + amounts (`FX_DELAY_MIX/TIME`, etc.). En web, las mismas en
camelCase dentro del objeto `P`; los de performance se preservan al cargar presets
(`PERF_KEYS`).

---

## 4. Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# AAX opcional: -DAAX_SDK_PATH=/ruta/al/aax-sdk
```
Linux: `libgtk-3-dev libwebkit2gtk-4.1-dev` (WebView) + deps X11/ALSA/GL de JUCE.
CMake: `NEEDS_MIDI_INPUT/OUTPUT TRUE`. Proyecto VS Code en `.vscode/`. APK en
`android/` (WebView; ver `android/COMPILAR-APK.md`, se compila en GitHub Actions).

---

## 5. Verificación (cómo se prueba)

- **DSP/lógica**: Playwright + Chromium headless sobre la web app (sirviendo
  `webapp/` por HTTP), usando el hook `window.__moog` para llamar
  `engine`/`Perf`/`randomize` y medir RMS/voces activas. Evidencias logradas:
  chord Maj=3 voces / Maj7=4; arp recorre notas; cascade = wash; 8 wavetables
  suenan; FX sin errores; import path sin crash.
- **Render de UI**: Chromium con stub de los globals JUCE (`getSliderState`…,
  `window.JUCEGLUE={}` para bloquear el frontend real) sirviendo `Source/UI/web`.
- **Plugin**: compilación limpia (exit 0) — el DSP de audio del plugin no se
  escucha headless; se valida por compile + paridad de lógica con el JS testado.

---

## 6. Git / entrega

- Rama única de push: `claude/moog-synth-vst-dsp-p6mrst`.
- Artifact web (privado): `https://claude.ai/code/artifact/ff2cf8c1-e700-4756-b99a-ee41e6b16854`.
- Commits con footer `Co-Authored-By: Claude Opus 4.8` + `Claude-Session`.
- Docs: `docs/LEADR-vs-MoogVA.md` (comparativa) y `docs/LEADR-deep-dive.md`
  (desglose de LEADR + roadmap con estado).

---

## 7. MoogVA vs LEADR — estado

| Capacidad | LEADR | MoogVA |
|---|---|---|
| Osc VA + filtro | Sí | ✓ 3 osc + ladder ZDF |
| Samples one-shot | 2 slots + 500 + import | ✓ 8 wavetables; **web: import WAV** (plugin: pendiente) |
| Physical modeling | Sí | ✗ (roadmap: Karplus-Strong) |
| Chord lock | 3 modos | ✓ 10 tipos |
| Arp | WATERFALL | ✓ 6 modos incl. **Cascade** |
| FX | 18 + random | ✓ 7 + **FX Random** |
| MIDI-out | Sí | ✓ (plugin) |
| Mono/Poly | Sí | ✓ |
| Presets | 200 | ✓ 700 (10 libs) |
| Móvil/web/APK | ✗ | ✓ (ventaja propia) |

---

## 8. Roadmap (siguiente)

**P1 restante:** import de WAV en el **plugin** (file-chooser nativo → buffer en
`MonoSynthEngine`; librería de one-shots de fábrica).
**P2:** physical modeling (Karplus-Strong) como 4ª fuente; más FX hacia ~12;
macro **"IDEA"** (acorde+arp+FX+random en un botón); mod-matrix + LFOs.
**P3 (producto):** marca propia + web; licencia/trial; preset packs y librería
de samples vendibles; instaladores firmados (macOS notarizado, Win, AAX).

---

## 9. Convenciones (CLAUDE.md / GODMODE)

Modo **absoluto**: trabajo en silencio, verificación íntegra, un solo mensaje
final por tarea (entregable + explicación breve). Trabajo por fases (código →
UI → una compilación al final). Gotchas aprendidos:
- Web app **sin `<meta charset>`** → mojibake (arreglado).
- Dropdowns in-DOM (no `<select>` nativo); submenús con combinador hijo
  `.dd-folder.open > .dd-sub`; flechas como `‹/›` (WebKitGTK Latin-1).
- El frontend real de JUCE pisa los stubs de render → fijar `window.JUCEGLUE`.
- Grid del panel: usar `minmax()` + `min-width:0` para que los selectores no
  desborden entre secciones; altura del editor/lienzo debe crecer con nº de filas.
- `pkill` en comandos compuestos puede matar el shell (exit 144) — no encadenarlo
  con git; servir con `setsid python3 -m http.server --directory`.
