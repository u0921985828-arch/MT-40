# MoogVA Synth

Sintetizador virtual-analógico (JUCE 8, C++17). UI en WebView. DSP en `Source/DSP`.

## Estilo de respuesta

Disparador: la palabra **`absoluto`** (o por defecto en este repo). Al activarse, aplica
todo lo siguiente hasta nueva orden:

- **Modo absoluto**: conciso, directo, sin relleno, preámbulos ni cierres de cortesía.
  Nada de "voy a…/ahora…/déjame…", ni narrar herramientas/lecturas/greps/ejecuciones.
  Solo lo esencial.
- **Proceso en silencio, rigor íntegro**: leer, verificar, iterar y corregir siempre,
  pero sin narrarlo. Una sola respuesta al terminar: ENTREGABLE + EXPLICACIÓN BREVE
  (1–5 líneas: qué y por qué). Bloques largos (auditoría, decisiones, verificación)
  solo bajo demanda.
- **Un solo mensaje final por tarea**: al terminar cada tarea/petición, UN único
  mensaje de **máximo 2 líneas** explicando lo hecho (qué + estado build/commit).
  Nada más de texto de cierre.
- **Tokens/coste**: no se pueden medir con exactitud desde el chat. Para verlos, usar
  el comando `/cost` de Claude Code (tokens y coste de la sesión). No inventar cifras.
- Trabajo por fases cuando aplique: primero todo el código, luego toda la UI, y una
  sola compilación al final (no compilar tras cada cambio).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Linux necesita: `libgtk-3-dev libwebkit2gtk-4.1-dev` (WebView) además de las deps X11/ALSA/GL de JUCE.
