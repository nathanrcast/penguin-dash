# Penguin Dash — Android port plan

Rebranded fork of Extreme Tux Racer. Fork base: **etr-0.8.4** source tarball (SourceForge; no public
git). Play-tested + approved 2026-06-30. Reuses the OneCube/AssaultCube Android toolchain (NDK,
keystore signing, `assembleRelease`, sideload).

## Codebase baseline (from probe)

- C++, **GPL**, ~47 MB. Build system **autotools** (`configure.ac`, `src/Makefile.am`, `autogen.sh`).
- Windowing/audio via **SFML 2.5**; rendering via **OpenGL**.
- **Rendering is legacy fixed-function:** ~76 immediate-mode calls (`glBegin`/`glVertex`/`glPushMatrix`)
  vs. only ~23 modern (`glDrawArrays`/VBO). Immediate mode **does not exist in OpenGL ES** — this is
  the core port work.

## Port strategy

1. **Desktop baseline:** ✅ **DONE 2026-07-01.** Built on the dev desktop with the pre-generated `./configure`
   (no autotools needed) + `make` — clean, **zero errors/warnings**. Only added dep was `libsfml-dev`
   (GL/GLU dev already present). Notably compiled + linked against system **SFML 2.6.1** despite the
   source targeting 2.5 — **no API breakage** (de-risks the SFML-Android step). Binary: `src/etr` (15M),
   all libs resolved. Launch needs a data dir (repo ships `data/`); run on the physical display `:0`.
2. **GLES rendering rewrite — the main task:** ✅ **DONE 2026-07-01 (M0–M6) → `docs/gles_render_audit.md`.**
   Full desktop-first GLES2 rewrite shipped across 6 milestones (M0 scaffolding → M6 cleanup). Renderer
   is now shader-based (2D + 3D programs), matrices/light/material/fog/texgen/alpha all tracked
   engine-side in `TRenderState` and fed as uniforms — **no fixed-function calls left in the active
   render path** (remaining matches are comments or editor/tool/test code). Clean recompile of all of
   `src/` is green (zero errors/warnings), `src/etr` relinks. **Loose end for step 3:** vestigial
   `glVertex3`/`glNormal3`/`glTexCoord2`/`glTranslate` wrappers in `ogl.cpp` are now unreferenced but
   still call GLES2-nonexistent `gl*d` symbols — prune when the Android link surfaces them.
3. **SFML on Android:** SFML 2.6 has an official Android/NDK backend — stand up the SFML Android
   project template (Gradle + NDK), reuse OneCube keystore/signing.
   **Toolchain probe 2026-07-01:** NDK 26.3 + 27.3, SDK platform-34 / build-tools 34.0.0, and
   the shared release keystore are present. OneCube ships a reusable Gradle template at
   `assaultcube/source/android/` (native side is SDL, not SFML — Gradle/manifest/signing reusable, glue
   is not). **Gating unknown: SFML for Android is not packaged** — only desktop `amd64` 2.6.1 is
   installed; the Android backend must be cross-compiled per-ABI from source (+ Android extlibs), OR
   bypassed. See "Android GL/window approach" decision below.
4. **Controls:** steering via **device tilt (accelerometer)** with on-screen touch fallback
   (brake/jump/paddle buttons). Tune for 6–10 — forgiving, simple.
5. **Assets:** package `data/` (courses, models, textures, music) into the APK/OBB.
6. **Sign & sideload:** release-sign with the existing shared keystore, `assembleRelease`,
   `adb install -r` to the kids' tablets.

## Risks / open questions

- The **GLES rewrite** is non-trivial (terrain rendering, particle/snow effects may use fixed-function
  tricks). Biggest unknown — spike terrain first.
- Old autotools build → NDK build glue (may be easier to drive sources directly from the Android
  `Android.mk`/CMake than to reuse autotools).
- Tilt-steering feel on a tablet vs. keyboard; may need a sensitivity/deadzone pass for kids.

## Distribution

Sideload first (matches OneCube/AMH). F-Droid later (GPL-native); Google Play only if broad reach is
wanted (per-app Families/COPPA compliance). GitHub repo **local-only for now** — create/push
`nathanrcast/penguin-dash` (public, GPL) when there's a build to show.
