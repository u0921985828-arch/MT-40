# MoogVA vs LEADR — comparativa y hoja de ruta

Análisis de **LEADR** (Tuff Nerds, sampler+synth de leads, VST3/AU, $127) frente a
**MoogVA** (este proyecto: VA estilo Minimoog + web app móvil), para cerrar la
brecha y llevar MoogVA de "un Moog" a un producto de industria.

## Comparativa de features

| Área | LEADR | MoogVA (antes) | MoogVA (ahora) |
|---|---|---|---|
| Síntesis | Osciladores + modelado físico + 2 one-shots | 3 osc VA + ruido + ladder ZDF | 3 osc VA + ruido + ladder ZDF |
| Capa de samples | **Sí** (500+ one-shots + tus carpetas) | No | **Sí — capa wavetable** (8 timbres: Digital/Organ/Voice/Reed/Bell/Pluck/Glass/Saw2) *(import de carpetas: roadmap)* |
| Acordes | Chord lock 3 modos | No | **Sí — 10 tipos** (Oct/5th/Power/Maj/Min/Sus4/Maj7/Min7/Add9) |
| Arpegiador | WATERFALL | No | **Sí** (Up/Down/Up-Down/Random/As-Played · 1/4–1/32 · 1–4 oct · gate · sync host/BPM) |
| Randomize | FX aleatorizables | No | **Sí — Randomize musical** de todo el patch |
| Efectos | 18 FX | Cadena master fija (tube+shelves+Haas+comp) | Cadena master fija *(roadmap: rack de FX)* |
| Poly | Sí | **Sí** (8 voces mono/poly) | Sí |
| Presets | 200+ | **700 (10 librerías)** | 700 (10 librerías) |
| Móvil | No | **Sí** (web app + APK, pantalla completa) | Sí + Scope/Spectrum |
| Precio | $127 | — | — |

**Ventajas ya nuestras:** 700 presets en 10 estilos, filtro ladder ZDF auténtico,
multiplataforma real (VST3/AU/Standalone **+ web + APK**), UI madera "UA".
**Lo que faltaba y se ha cerrado hoy:** acordes, arpegiador y randomize (paridad
con los ganchos comerciales de LEADR).

## Implementado en esta sesión (plugin + web, en paridad)

- **Capa SAMPLE** (mixer): 8 timbres wavetable generados aditivamente (sin assets
  externos), mezclables con los osciladores y enrutados por filtro/envolvente.
- **Sección PERFORM**: Chord · Arp (on/rate/mode/oct/gate/BPM) · RANDOM.
- Plugin C++: params APVTS nuevos + `applyPerform()` (expansión de acordes +
  arpegiador sample-accurate con tempo de host o BPM propio) en `processBlock`.
- Web app JS: capa `Perf` (acordes + scheduler de arp) sobre el motor + Randomize.
- Ambos: params de performance preservados al cargar presets (como Poly).

## Roadmap para "explotar la industria"

**P1 — diferenciación de sonido (lo que más vende):**
1. **Import de samples del usuario** + librería de one-shots reales (la capa
   wavetable ya existe; falta cargar WAV propios y una librería de fábrica).
2. **Rack de FX** (8–18): drive, chorus, phaser, delay ping-pong, reverb, bitcrush,
   filtro, width — con **Randomize de FX** "musical".

**P2 — expresividad y flujo:**
3. **MPE / velocity / aftertouch** y **unison** (spread de voces) para grosor.
4. **Mod matrix** simple (LFO×2 + envs → destino) y **sequencer** por pasos.
5. **Macros** (1–4 knobs asignables) para diseño rápido.

**P3 — producto y marca:**
6. **Marca propia** (nombre no-"Moog" para evitar marca registrada), logo, web.
7. **Sistema de licencia/prueba** (trial de N días) y **preset packs** de pago.
8. **Firma/instaladores** (macOS notarizado, Win instalador, AAX firmado).
9. **Demos/loops** y campaña (Instagram/YouTube como LEADR).

## Nota legal

"Moog"/"Minimoog" son marcas registradas de Moog Music. Para comercializar,
**renombrar** el producto y evitar el trade dress del Model D.
