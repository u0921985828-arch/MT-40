# MoogVA Synth — Contexto del proyecto

Sintetizador virtual-analógico estilo **Minimoog Model D**. Dos entregables desde el
mismo motor/diseño:

1. **Plugin JUCE 8** (C++17) — VST3 / AU / Standalone, UI en WebView.
2. **Web app móvil** de un solo archivo (`webapp/index.html` → `moogva-mobile.html`),
   que suena de verdad y se publica como Artifact privado.

## Estado actual

Todo implementado, compilado y entregado. Sin tareas pendientes.

- Dropdowns metálicos in-DOM (abren hacia abajo, dentro del marco, flyouts anidados).
- Árbol de presets por carpetas (Biblioteca → Categoría → Preset) + flechas prev/next.
- **10 bibliotecas** de presets, ~700 presets (`webapp/presets.json`, ~321 KB).
- Glow-up de audio (tube/shelf en plugin; cadena de masterización nativa en web).
- Glow-up visual (knobs premium, placas, displays scope/spectrum/VU, teclado).
- **Mono/Poly**: interruptor POLY en Controllers. Mono = una voz auténtica
  (última nota); Poly = 8 voces para acordes.

## Arquitectura

### Plugin (`Source/`)
- `Parameters.h` — IDs de parámetros APVTS (incl. `POLY_ON = "POLY_ON"`).
- `PluginProcessor.{h,cpp}` — APVTS; banco de **8 voces** `std::array<MonoSynthEngine,8>`;
  `allocateVoice()` (idle → released-más-viejo → robar-más-viejo) / `findVoiceForNote()`;
  MIDI por voz; mono usa `voices[0]` con el stream completo, poly reparte note-ons y
  suma con `applyGain(0.6f)`. Master: shelves warmth/air + tube tanh sobreoversampleado
  4× + DC blocker.
- `DSP/` — osciladores PolyBLEP, filtro ladder Moog ZDF (TPT, feedback-tanh, 2×
  interno), ADSR exponencial, drift analógico, mod bus. `MonoSynthEngine` expone
  `isActive()`.
- `PresetManager.h` — banco `{libraries:[...]}` + biblioteca "User"; parsea IDs
  `Library|||Category|||Name`; `resetToDefaults()` **excluye** `polyOn` (modo global,
  no parte del preset).
- `UI/web/` — `index.html`, `js/main.js`, `style.css`. Embebido con
  `juce_add_binary_data` (incluye `webapp/presets.json`).

### Web app (`webapp/index.html`)
- `Engine` (voz mono, ScriptProcessorNode 512) con `reset()` / `active()`.
- `Poly` (8 `Engine`, misma lógica de asignación/robo que el plugin).
- Cadena de masterización Web Audio (WaveShaper 4×, shelves, Haas width, compresor).
- Banco de 10 bibliotecas embebido; mismos builders de UI/presets que el plugin.
- `P.polyOn` preservado al cargar presets (`keepPoly`).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Linux: `libgtk-3-dev libwebkit2gtk-4.1-dev` (WebView) + deps X11/ALSA/GL de JUCE.

## Git

- Rama de desarrollo: `claude/moog-synth-vst-dsp-p6mrst` (única rama de push).
- Artifact web (privado): `https://claude.ai/code/artifact/ff2cf8c1-e700-4756-b99a-ee41e6b16854`.

## Convenciones (CLAUDE.md)

- **Modo absoluto** (por defecto): conciso, trabajo en silencio, un solo mensaje final
  ≤2 líneas por tarea. Trabajo por fases: primero todo el código, luego toda la UI,
  una sola compilación al final.
- Arreglos que aprender:
  - No usar `<select>` nativo (rompe rotación/diseño) → dropdown in-DOM `.dd`.
  - Submenús anidados: combinador hijo `.dd-folder.open > .dd-sub` (no descendiente).
  - Flechas: emitir `‹` / `›` (ASCII) — WebKitGTK decodifica main.js como
    Latin-1 y corrompe literales `‹`/`›`.
  - Dropdowns: abrir siempre hacia abajo, `max-height` vía cadena `offsetTop`.
