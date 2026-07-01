# Penguin Dash — Android shell plan (port step 3)

Step 2 (GLES2 render rewrite, M0–M6) is done: the renderer is shader-based and self-contained on a
GLES2 context. This step stands up the Android app that hosts it.

## Decision (2026-07-01): direct NDK/EGL shim — SFML dropped on Android

Chosen over cross-compiling SFML's Android backend. Rationale: with M0–M6 the renderer no longer needs
SFML for GL context or matrices, so SFML's remaining value on Android was only window/input/audio/asset
— all provided natively by the NDK. This avoids the fiddly per-ABI SFML cross-compile and gives direct
control over the kid-friendly tilt controls the plan wants. Cost: ETR's SFML platform layer
(`winsys`, `sf::Music` audio, `sf::Event` input, file IO) is replaced by a native shim.

App shape: **pure `NativeActivity`** (`android_native_app_glue`, no Java) — simpler than OneCube's SDL
Java-activity path. Reuse from `assaultcube/source/android/`: the Gradle wrapper, root/`settings`/
`app` build.gradle signing + per-ABI (arm64-v8a) + CMake wiring, `keystore.properties` pattern, and the
extract-assets-on-new-versionCode idea. Do **not** reuse its SDL/gl4es/openal native deps.

Toolchain on `.13`: NDK 26.3.11579264 + 27.3, SDK platform-34 / build-tools 34.0.0,
`~/keystores/onecube-android-release.jks`, full JDK at `~/amh-android-build/jdk21`
(system java-21 is a JRE — build with `JAVA_HOME=~/amh-android-build/jdk21 ./gradlew ...`).

## Sub-milestones (device-tested on Tab A9+ / OnePlus N200 — both arm64)

- **A0 – Scaffold + native boot:** ✅ **DONE + device-verified 2026-07-01 on the Tab A9+** (SM_X210,
  GLES 3.2). Gradle project reusing the OneCube template; `NativeActivity` + `android_native_app_glue`;
  EGL GLES2 context; pulsing clear-color loop fills the full 1920×1200 surface. Two device-found bugs
  fixed vs. the first build: (1) `APP_CMD_INIT_WINDOW` fires pre-layout so the window is 1×1 at surface
  creation — split context-init from surface-creation and recreate the surface on window-size change
  (`egl_check_resize`, also covers rotation/background); (2) the poll timeout must be re-evaluated each
  `ALooper_pollAll` call — hoisting it into a variable hangs the loop when `running` flips true
  mid-iteration. No ETR engine yet.
- **A1 – Engine bring-up:** compile ETR `src/*.cpp` into `libpenguindash.so` via CMake (autotools not
  used on Android). Replace `winsys`/context with the EGL shim; drive ETR's main loop from the native
  glue; render the first menu frame through the existing GLES2 renderer. Prune the vestigial fixed-
  function wrappers (`glVertex3`/`glNormal3`/`glTexCoord2`/`glTranslate`) the Android link surfaces.
- **A2 – Input (touch + tilt):** `AInputEvent` touch → menu/UI + on-screen buttons (brake/jump/paddle);
  `ASensor` accelerometer → steering, with deadzone/sensitivity tuned for ages 6–10.
- **A3 – Audio:** replace `sf::Music`/`sf::Sound` with Oboe (music + SFX).
- **A4 – Assets:** package `data/` into APK assets; AAssetManager-backed IO, extract-on-first-run to
  `getExternalFilesDir()` keyed on versionCode (OneCube pattern).
- **A5 – Sign & sideload:** `keystore.properties` → `onecube-android-release.jks`, `assembleRelease`,
  `adb install -r` to the tablets. Then create/push `nathanrcast/penguin-dash` (public, GPL).

## Risks

- ETR's platform layer coupling to SFML is wider than the renderer was — audio and file IO especially.
- `NativeActivity` lifecycle (surface create/destroy on rotate/background) must not leak the GL context;
  keep GL objects reloadable.
- Tilt feel on a tablet vs. keyboard — expect a sensitivity pass.
