# LEADR — desglose funcional completo + plan de inspiración para MoogVA

Objetivo: entender **todo** el funcionamiento de LEADR (Tuff Nerds) y, sin
copiarlo, destilar QUÉ lo hace vender para inspirar a MoogVA.

> Nota de fuentes: la web oficial bloquea el scraping (403) y los detalles finos
> (nombres exactos de knobs, lista literal de los 18 FX, los 3 modos de acorde)
> viven en sus vídeos, no en texto indexable. Lo de abajo está confirmado por
> varias fuentes; lo inferido se marca como *(inferido)*.

---

## 1. Identidad y propuesta

- "**Lead-focused sampler + synth**". VST3/AU, Mac/Win. **$127**, prueba 5 días.
- Primer plugin de un creador indie; marketing 100% social (Instagram/YouTube).
- **Promesa central**: *"convierte una sola tecla en una idea musical terminada"*.
  Todo el diseño gira en torno a **velocidad a la idea** (inspiration-first).

## 2. Arquitectura / cadena de señal

```
[Sample slot 1]  ┐
[Sample slot 2]  ├─► BLEND ─► (filtro + envolventes) ─► FX RACK (18) ─► OUT
[Osciladores VA] │
[Physical model] ┘
        ▲
  WATERFALL (arp) + CHORD LOCK  ─── generan las notas que tocan las fuentes
        └────────────────► (también) MIDI OUT ─► Serum / Diva / cualquier VST
```

**Fuentes de sonido (se mezclan en una voz):**
1. **2 slots de one-shots** — de una librería de **500+** o **de tus propias
   carpetas** (import de WAV del usuario). *Esta es su bala de plata.*
2. **Osciladores internos** (virtual-analog).
3. **Physical modeling** (cuerdas/cañas/mazos — *inferido*: da carácter acústico).

**Generación de notas (el gancho):**
4. **Chord Lock** — menú de **3 modos** de acorde → una tecla dispara un acorde.
5. **WATERFALL** — arpegiador propio que *"cascadea el acorde en un lavado
   sostenido sobre el que puedes cabalgar"*: no es staccato, es **solapado/
   sostenido** (notas que se encadenan y se sostienen formando textura).

**Procesado:**
6. **FX Rack: 18 efectos** con **randomización musical** (botón que combina FX
   de forma con sentido, no aleatoria pura).

**Modo:**
7. **Mono/Poly** (leads clásicos vs acordes).

**Salida/flujo:**
8. **MIDI OUT**: el acorde+arp puede **enrutarse a otros VST** (Serum, Diva…).
   LEADR funciona también como **generador MIDI de acordes/arps** para tu setup.
9. **200+ presets**.

## 3. Por qué vende (lo que hay que robar en ESENCIA, no en forma)

1. **Una tecla → frase terminada** (acorde + arp + FX + carácter). Fricción cero.
2. **Fuentes híbridas**: samples reales aportan lo que un VA no puede.
3. **Import de tus WAV**: sonido único + librería vendible.
4. **Randomización musical en todo**: descubrimiento sin esfuerzo.
5. **MIDI-out**: se integra como cerebro de acordes/arps del resto del setup.
6. **WATERFALL**: un arp con identidad (cascada/wash), no "otro arp".
7. **Marca + marketing social** fuertes.

---

## 4. MoogVA hoy vs LEADR

| Capacidad | LEADR | MoogVA (hoy) | Brecha |
|---|---|---|---|
| Osciladores VA | Sí | 3 osc + ladder ZDF | — |
| Samples one-shot | 2 slots + 500 + import | 8 wavetables proc. | **Falta import/one-shots reales** |
| Physical modeling | Sí | No | Opcional |
| Chord lock | 3 modos | 10 tipos | ✓ (mejor) |
| Arp | Custom (WATERFALL) | 5 modos staccato | **Falta modo cascada/wash** |
| FX | 18 + random musical | 4 + random | **Faltan FX + random dedicado** |
| MIDI out a otros VST | Sí | No | **Falta (killer de flujo)** |
| Mono/Poly | Sí | Sí | — |
| Presets | 200 | 700 (10 libs) | ✓ (mejor) |
| Móvil/web/APK | No | Sí | ✓ (ventaja única) |

---

## 5. Plan de inspiración — ESTADO

**P1 — cerrado en esta tanda (inspirado ≠ copia):**
1. ✅ **Modo arp "CASCADE" (WATERFALL)**: notas solapadas que forman un *wash*
   sostenido (hasta 6 notas sonando, rueda). Plugin + web.
2. ✅ **FX ampliado a 7** (Drive · Chorus · Phaser · Crush · Tone · Delay · Reverb)
   + **FX RANDOM musical**. Plugin (FxRack C++) + web (Web Audio).
3. ✅ **MIDI-OUT (plugin)**: modo generador — el acorde/arp sale como MIDI para
   tocar Serum/Diva/etc.; silencia el audio interno (`producesMidi=true`).
4. ✅ **Import de WAV del usuario (web)**: slot "User" en el mixer que carga tu
   audio (botón WAV → `decodeAudioData`) y lo reproduce pitcheado + loop.
   ⏳ *Pendiente*: import de WAV en el **plugin** (requiere puente nativo de
   file-chooser al WebView; siguiente iteración).

**P2 — carácter y descubrimiento:**
4. ✅ **Macro "IDEA"**: un botón que genera un parche musical completo (sonido +
   acorde + arp + FX) por arquetipo — la promesa "una tecla → idea". Web + plugin.
5. ✅ **Pad XY morph**: interpola en tiempo real entre 4 slots de parche (web).
6. ✅ **DSP next-gen**: saturación de ladder asimétrica dependiente de tolerancia
   (armónicos pares) + inestabilidad estocástica de voltaje por voz — ambos
   motores, gobernados por el knob Drift.
7. ✅ **Identidad Cyber-Dark** (web + plugin) + **visualizador WebGL** reactivo
   con fallback 2D.
Pendiente P2: más FX (~8–12); physical modeling ligero (Karplus-Strong).
**Aplazado:** MPE; FX drag-and-drop; oversampling adaptativo 8×/16×; morph XY en
el plugin (interpolación nativa de presets).

**P3 — producto/mercado:**
7. **Marca propia** (fuera "Moog", marca registrada) + web + demos sociales.
8. **Librería de one-shots** y **preset packs** vendibles; trial/licencia.

---

## 6. Lo que NO copiamos

Nombre/branding, el "trade dress" de su UI, sus samples/presets. Nos inspiramos
en la **filosofía** (velocidad a la idea, híbrido, random musical, MIDI-out) y la
ejecutamos con nuestra identidad (madera "UA", multiplataforma web+APK, 10 libs).
