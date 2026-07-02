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
  - **A1a/b foundation DONE 2026-07-01:** SFML is replaced by a **compat shim** at
    `android/app/src/main/cpp/sfml_compat/SFML/{System,Window,Graphics,Audio}.hpp` (+ `Window/Context.hpp`),
    placed first on the include path so ETR's `#include <SFML/…>` resolves to it with zero edits to the
    5 include sites. Value types (`Color`, `Vector2`, `String`, `IntRect`, `Keyboard::Key` incl. 2.5
    aliases, `Vertex`, `VertexArray`, `Clock`) are real inline; media classes (`Image`/`Texture`/`Font`/
    `Text`/`Sprite`/`RenderTarget`/`Music`/`Sound`) are declared to ETR's exact call surface and **stubbed
    in `sfml_compat.cpp`** so the engine links first (grep `TODO(A1c/A1d/A3)`). `bh.h` gated for
    `__ANDROID__` (GLES2 headers, no GLX); `ogl.{h,cpp}` gated for the desktop-only compiled-vertex-array
    ext; `spx.h` given an explicit `<ios>`. CMake compiles the full source list minus `main.cpp` (entry =
    `android_main`). All shim/portability errors cleared.
  - **A1 render conversion DONE 2026-07-01:** all remaining fixed-function **state-setup** calls
    removed in favour of M6's tracked-state setters (`RenderSetLight`/`RenderSetFog`/`RenderSetFogEnabled`/
    `RenderSetTexGenPlanes`) — the shader path (`glshader.cpp` reading `RenderStateSnapshot()`) already
    drove rendering on both platforms, so the fixed-function calls were pure vestige. Removed: `glLightfv`/
    `glFog*`/`glTexGen*`/`glTexEnvf`/`glMaterial*` functions; `glEnable/glDisable` of `GL_LIGHTING`/
    `GL_TEXTURE_2D`/`GL_NORMALIZE`/`GL_COLOR_MATERIAL`/`GL_TEXTURE_GEN_*`/`GL_FOG`; the `glColor`/
    `glColor4*` current-color path (color travels as a vertex attribute); dead matrix/vertex wrappers
    (`glTranslate`/`glNormal3`/`glVertex3`/`glTexCoord2`); and the fixed-function query rows in
    `PrintGLInfo`'s `gl_values[]` (`GL_MAX_LIGHTS`/`*_STACK_DEPTH`/`GL_DOUBLEBUFFER`). The light API moved
    from `GL_LIGHT0-3` GLenum tokens (undefined in GLES2) to plain int indices (`TLight::Enable(int)`,
    `RenderSetLight(int)`). Touched `env`, `course_render`, `hud`, `particles`, `track_marks`, `quadtree`,
    `tux`, `textures`, `ogl`, `ogl_test`, `tools`. **Verified:** desktop `etr` still builds+links (exit 0);
    every render TU now compiles under the NDK. Kept the still-valid GLES2 fixed state
    (`GL_DEPTH_TEST`/`GL_CULL_FACE`/`GL_BLEND`/`GL_STENCIL_TEST`, `glDepthMask/Func`, stencil ops).
  - **A1c DONE + device-verified 2026-07-01 on the Tab A9+:** the full engine now links into
    `libpenguindash.so` and `android_main` drives the real ETR init + `State::manager.Run` loop on the live
    EGL surface. Pieces: (1) `native_bridge.h` — the seam between the raw EGL/glue host and the engine:
    `SurfaceReady`/`GetSurfaceSize`/`SwapBuffers`/`PumpEvents`/`ShouldClose`/`PollInput`; (2) `native_main.cpp`
    refactored — file-scope `Engine`, context kept across surface loss (background/foreground), `onInputEvent`
    → input queue, `android_main` waits for the first surface then calls `pd_engine_main()`; (3)
    `android_entry.cpp` (new, replaces the excluded `main.cpp`) — defines `g_game` + the init sequence, made
    asset-load-failure-tolerant (logs + continues, no abort, until A4); (4) `sfml_compat.cpp` Window bridged to
    the surface — `display`→`PumpEvents`+`eglSwapBuffers`, `getSize`→real surface px, `pollEvent`→`AInputEvent`
    (touch→mouse, keys via `MapKey`: BACK/ESC→Escape, ENTER/DPAD-center→Return, DPAD→arrows), `Mouse::getPosition`
    cached from pointers, `RenderTarget::clear`→`glClear`; (5) `winsys.cpp` — `setKeyRepeatEnabled` shim (no-op),
    `TakeScreenshot` gated off on Android (needs A4/stb), and `resolution` adopts the real surface size; (6)
    `CTexture::GetSFTexture` made null-safe (missing texture → empty, matching its `GetTexture` sibling) so the
    loop survives the asset-less state. **Verified:** boots on the A9+ (EGL 1920×1200, GLES 3.2), runs the loop
    with no crash (process stays alive), clears the full surface to the GUI bg `colBackgr` (102,153,204) each
    frame via the bridged swap; tap + dpad input exercised without crashing.
  - **A1d + A4 (NEXT, coupled): real 2D draw + APK assets → first menu frame.** The loop runs but draws
    nothing — `Texture`/`Font`/`Text`/`Sprite`/`Image` are still shim stubs and there are no assets on device
    (`LoadTextureList` fails, logged). A1d = implement those on `stb_image`/`stb_truetype` + the GLES2 Shader2D
    path (add stb to `cpp/third_party`); A4 = package `data/` into APK assets, `AAssetManager`-backed IO,
    extract-on-first-run keyed on versionCode (OneCube pattern). Both are needed before the logo/menu text
    actually appear — do them together.
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
