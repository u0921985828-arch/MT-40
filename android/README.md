# MoogVA Synth — Android (APK)

App Android que envuelve la web app del sintetizador (`webapp/index.html`) en un
`WebView` a pantalla completa. Todo el DSP corre en la página vía Web Audio; la
app nativa solo la aloja. El `index.html` se copia a `assets/` en tiempo de
compilación desde `../webapp/index.html` (tarea `syncWebApp`), así que el APK
siempre usa el mismo banco de presets que el plugin y la web.

## Requisitos

- JDK 17+ (probado con 21).
- Android SDK con:
  - Platform `android-34`
  - Build-Tools `34.0.0`
- Variable `ANDROID_HOME` (o `local.properties` con `sdk.dir=/ruta/al/Android/Sdk`).

La forma más fácil: instalar **Android Studio**, abrir la carpeta `android/` y
pulsar Run ▶ (crea `local.properties` y descarga el SDK solo).

## Compilar por línea de comandos

```bash
cd android
echo "sdk.dir=$ANDROID_HOME" > local.properties   # o la ruta de tu SDK

# APK de depuración (instalable directamente):
./gradlew assembleDebug
#   -> app/build/outputs/apk/debug/app-debug.apk

# APK "release" (firmado con la clave debug por defecto; cambia la firma
# para publicar en Play Store):
./gradlew assembleRelease
#   -> app/build/outputs/apk/release/app-release.apk
```

Instalar en un dispositivo con depuración USB activada:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Notas

- `minSdk 24` (Android 7.0): el WebView del sistema ya soporta Web Audio.
- `MediaPlaybackRequiresUserGesture(false)` permite que el `AudioContext`
  arranque en la primera tecla.
- La app pide orientación `fullUser`; la propia UI web bloquea a horizontal.
- No requiere permisos de red: la web app es totalmente local (assets).
- Para publicar: genera tu keystore y sustituye `signingConfigs.debug` en
  `app/build.gradle.kts` por tu configuración de firma release.

## Por qué no viene el APK ya compilado

El entorno donde se generó este proyecto tiene la red restringida y bloquea los
repositorios de Google (`dl.google.com`, Android Gradle Plugin, SDK), así que no
se pudo compilar allí. En una máquina con acceso normal a internet, los comandos
de arriba producen el APK sin cambios adicionales.
