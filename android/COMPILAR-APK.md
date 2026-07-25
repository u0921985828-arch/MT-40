# Compilar el APK de MoogVA Synth

Cómo se genera y se prueba el APK Android, y cómo reproducirlo. El APK envuelve
la web app (`webapp/index.html`) en un `WebView` a pantalla completa; todo el
DSP corre en la página vía Web Audio.

---

## Opción A — Automático en GitHub Actions (recomendado)

No necesitas instalar nada: dos workflows hacen el trabajo en un runner con
internet y Android SDK.

### 1. Build (`.github/workflows/android-apk.yml`)

Se dispara en cada `push` que toque `android/**`, `webapp/index.html` o el
propio workflow (también manual desde la pestaña **Actions ▸ Run workflow**).

Pasos:
1. JDK 17 (`actions/setup-java`).
2. Android SDK (`android-actions/setup-android`).
3. `./gradlew assembleDebug`.
4. Sube el APK como **artifact** (`MoogVA-Synth-apk`).
5. Lo publica en el **release** con tag `apk-android` (`softprops/action-gh-release`).

**Descargar el APK:**
- Release: `https://github.com/u0921985828-arch/MT-40/releases/download/apk-android/MoogVA-Synth.apk`
- O: Actions ▸ el run ▸ sección *Artifacts* ▸ `MoogVA-Synth-apk`.

### 2. Test en emulador (`.github/workflows/android-emulator-test.yml`)

Arranca un emulador y comprueba que la app abre sin crashear.

Pasos:
1. Build del APK (igual que arriba).
2. **Enable KVM** (aceleración por hardware del emulador).
3. `reactivecircus/android-emulator-runner` — emulador x86_64 API 30,
   `google_apis`, perfil `pixel_5`, sin ventana.
4. Script: `adb install` → `am start` la `MainActivity` → espera →
   verifica que no hay `FATAL EXCEPTION` y que la actividad está *resumed* →
   `screencap`.
5. Sube la captura como artifact `emulator-screenshot` y la adjunta al release.

Si algo peta, el job falla y el logcat relevante queda en los logs del step
*Run app on emulator*.

---

## Opción B — Local

Necesitas el **Android SDK** (platform `android-34`, build-tools `34.0.0`).
Lo más cómodo: **Android Studio** ▸ abrir la carpeta `android/` ▸ Run ▶.

Por línea de comandos:

```bash
cd android
echo "sdk.dir=$ANDROID_HOME" > local.properties   # ruta de tu SDK

./gradlew assembleDebug
#   -> app/build/outputs/apk/debug/app-debug.apk

adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Probar en un emulador local:

```bash
# crear un AVD una vez (si no tienes ninguno)
sdkmanager "system-images;android-30;google_apis;x86_64"
avdmanager create avd -n moogva -k "system-images;android-30;google_apis;x86_64"

emulator -avd moogva &
adb wait-for-device
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.moogva.synth/.MainActivity
```

---

## Instalar en el móvil

1. Descarga `MoogVA-Synth.apk` (link del release de arriba).
2. Ábrelo en el teléfono; activa **"Instalar apps desconocidas"** si lo pide.
3. O por USB con depuración activada: `adb install -r MoogVA-Synth.apk`.

Es un APK **debug** (firmado con la clave de depuración). Para publicar en
Play Store hay que firmarlo con tu propio keystore — ver
`app/build.gradle.kts` (`signingConfigs`).

---

## Detalles técnicos

- `applicationId` `com.moogva.synth`, `minSdk 24`, `targetSdk 34`.
- El `index.html` se copia a `assets/` en tiempo de compilación desde
  `../webapp/index.html` (tarea Gradle `syncWebApp`), así el APK comparte el
  mismo banco de presets que el plugin y la web — una única fuente de verdad.
- `MediaPlaybackRequiresUserGesture(false)` para que el `AudioContext` arranque
  a la primera tecla; sin permisos de red (todo local).

### Problema que surgió y su arreglo

El primer build falló en `checkDebugDuplicateClasses`: AndroidX traía
`kotlin-stdlib 1.8.x` mientras otras dependencias transitivas pedían
`kotlin-stdlib-jdk7/jdk8 1.6.x`, y las clases colisionaban. Se resolvió
alineando todas las versiones de Kotlin con el BOM en `app/build.gradle.kts`:

```kotlin
implementation(platform("org.jetbrains.kotlin:kotlin-bom:1.9.24"))
```
