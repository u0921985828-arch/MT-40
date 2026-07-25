plugins {
    id("com.android.application")
}

android {
    namespace = "com.moogva.synth"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.moogva.synth"
        minSdk = 24          // Android 7.0 — modern WebView with Web Audio
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            // Debug-signed by default so `assembleRelease` still yields an
            // installable APK. Replace with your own keystore for the Play Store.
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.webkit:webkit:1.11.0")
}

// Single source of truth: copy the shared web app into the APK's assets at
// build time from ../webapp/index.html, so the Android build never drifts from
// the plugin/web bank. When built outside the repo (source missing), the copy
// already shipped in assets/ is used.
val webAppSource = rootProject.file("../webapp/index.html")
val syncWebApp by tasks.registering(Copy::class) {
    onlyIf { webAppSource.exists() }
    from(webAppSource)
    into(layout.projectDirectory.dir("src/main/assets"))
}
tasks.named("preBuild") { dependsOn(syncWebApp) }
