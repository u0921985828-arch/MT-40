# MoogVA Synth

Sintetizador virtual-analógico (JUCE 8, C++17). UI en WebView. DSP en `Source/DSP`.

## Estilo de respuesta

- Palabra clave **`absoluto`**: cuando el usuario la use, o por defecto en este repo,
  responde en **modo absoluto**: conciso, directo, sin relleno ni preámbulos.
  Nada de resúmenes largos ni cierres de cortesía. Solo lo esencial.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Linux necesita: `libgtk-3-dev libwebkit2gtk-4.1-dev` (WebView) además de las deps X11/ALSA/GL de JUCE.
