# Compilar un APK desde una web app (guía genérica)

Cómo empaquetar una web app (un `index.html` autónomo) en un APK Android
envolviéndola en un `WebView` a pantalla completa, y compilarlo/probarlo en un
runner de CI o en local. Todo el código de la app corre en la página; la parte
nativa solo la aloja.

> Sustituye los valores entre `<...>` por los tuyos:
> `<APP_ID>` (p. ej. `com.miempresa.miapp`), `<OWNER>/<REPO>`, rutas, etc.

---

## 1. Estructura mínima del proyecto Android

```
android/
├─ settings.gradle.kts
├─ build.gradle.kts
├─ gradle.properties
├─ gradlew  gradlew.bat  gradle/wrapper/…      # wrapper de Gradle
└─ app/
   ├─ build.gradle.kts
   └─ src/main/
      ├─ AndroidManifest.xml
      ├─ assets/index.html                     # tu web app (copiada en build)
      ├─ java/<paquete>/MainActivity.java
      └─ res/…                                  # icono, tema, strings
```

### `MainActivity` — el WebView

```java
WebView web = new WebView(this);
setContentView(web);
WebSettings s = web.getSettings();
s.setJavaScriptEnabled(true);
s.setDomStorageEnabled(true);
s.setMediaPlaybackRequiresUserGesture(false); // audio sin gesto previo
web.loadUrl("file:///android_asset/index.html");
```

### Una sola fuente de verdad para la web app

En vez de duplicar el HTML, cópialo en `assets/` al compilar con una tarea
Gradle (en `app/build.gradle.kts`):

```kotlin
val webAppSource = rootProject.file("../<ruta>/index.html")
val syncWebApp by tasks.registering(Copy::class) {
    onlyIf { webAppSource.exists() }
    from(webAppSource)
    into(layout.projectDirectory.dir("src/main/assets"))
}
tasks.named("preBuild") { dependsOn(syncWebApp) }
```

Y añade `app/src/main/assets/index.html` al `.gitignore` para no versionar la copia.

---

## 2. Compilar en CI (GitHub Actions)

Un workflow con acceso a internet instala el SDK y compila, sin que tengas que
tener nada local. Guárdalo en `.github/workflows/build-apk.yml`:

```yaml
name: Build APK
on:
  workflow_dispatch:
  push:
    branches: [ <RAMA> ]
    paths: [ 'android/**', '<ruta>/index.html', '.github/workflows/build-apk.yml' ]
permissions:
  contents: write
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with: { distribution: temurin, java-version: '17' }
      - uses: android-actions/setup-android@v3
      - name: Build debug APK
        working-directory: android
        run: |
          chmod +x gradlew
          ./gradlew --no-daemon assembleDebug
      - name: Rename
        run: cp android/app/build/outputs/apk/debug/app-debug.apk MiApp.apk
      - uses: actions/upload-artifact@v4          # descargable desde Actions
        with: { name: apk, path: MiApp.apk, if-no-files-found: error }
      - uses: softprops/action-gh-release@v2       # opcional: adjunta a un release
        with:
          tag_name: apk
          files: MiApp.apk
          make_latest: false
```

El APK queda en **Actions ▸ el run ▸ Artifacts** y, si usas el paso de release,
en `https://github.com/<OWNER>/<REPO>/releases/download/apk/MiApp.apk`.

### Probar en un emulador (opcional)

Un segundo workflow arranca un emulador y comprueba que la app abre:

```yaml
      - name: Enable KVM
        run: |
          echo 'KERNEL=="kvm", GROUP="kvm", MODE="0666", OPTIONS+="static_node=kvm"' \
            | sudo tee /etc/udev/rules.d/99-kvm4all.rules
          sudo udevadm control --reload-rules && sudo udevadm trigger --name-match=kvm
      - uses: reactivecircus/android-emulator-runner@v2
        with:
          api-level: 30
          arch: x86_64
          target: google_apis
          disable-animations: true
          emulator-options: -no-window -gpu swiftshader_indirect -noaudio -no-boot-anim
          script: |
            adb install -r android/app/build/outputs/apk/debug/app-debug.apk
            adb shell am start -n <APP_ID>/.MainActivity
            sleep 12
            adb exec-out screencap -p > screenshot.png
            if adb logcat -d | grep -q 'FATAL EXCEPTION'; then exit 1; fi
            adb shell dumpsys activity activities | grep -q '<APP_ID>/.MainActivity' || exit 1
```

Sube `screenshot.png` como artifact para verlo.

---

## 3. Compilar en local

Necesitas el **Android SDK** (una `platform` y unas `build-tools`; lo instala
Android Studio automáticamente).

```bash
cd android
echo "sdk.dir=$ANDROID_HOME" > local.properties

./gradlew assembleDebug
#   -> app/build/outputs/apk/debug/app-debug.apk

adb install -r app/build/outputs/apk/debug/app-debug.apk        # instalar
adb shell am start -n <APP_ID>/.MainActivity                    # lanzar
```

O más simple: **Android Studio ▸ abrir `android/` ▸ Run ▶**.

---

## 4. Instalar en el móvil

1. Descarga el `.apk`.
2. Ábrelo en el teléfono; activa **"Instalar apps desconocidas"** si lo pide.
3. O por USB con depuración: `adb install -r MiApp.apk`.

El APK *debug* va firmado con la clave de depuración. Para publicar en Play
Store, fírmalo con tu propio keystore (`signingConfigs` en `app/build.gradle.kts`).

---

## 5. Escollos habituales

- **`checkDebugDuplicateClasses` (Kotlin stdlib duplicada).** AndroidX trae
  `kotlin-stdlib` 1.8+ y otras deps piden `kotlin-stdlib-jdk7/jdk8` viejos.
  Alinéalos con el BOM en `dependencies`:
  ```kotlin
  implementation(platform("org.jetbrains.kotlin:kotlin-bom:<versión>"))
  ```
- **Audio no suena hasta tocar dos veces.** Usa
  `setMediaPlaybackRequiresUserGesture(false)` y reanuda el `AudioContext` en el
  primer evento de toque.
- **Iconos:** con solo `mipmap-anydpi-v26` (adaptive), añade PNG en
  `mipmap-mdpi…xxxhdpi` para Android < 8.
- **Proxies/redes restringidas:** si tu entorno bloquea `dl.google.com`, no
  podrás bajar el SDK ahí; compílalo en un runner con internet (sección 2).
