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

1. **Desktop baseline:** ✅ **DONE 2026-07-01.** Built on `.13` with the pre-generated `./configure`
   (no autotools needed) + `make` — clean, **zero errors/warnings**. Only added dep was `libsfml-dev`
   (GL/GLU dev already present). Notably compiled + linked against system **SFML 2.6.1** despite the
   source targeting 2.5 — **no API breakage** (de-risks the SFML-Android step). Binary: `src/etr` (15M),
   all libs resolved. Launch needs a data dir (repo ships `data/`); run on the physical display `:0`.
2. **GLES rendering rewrite — the main task:** ✅ **AUDITED 2026-07-01 → `docs/gles_render_audit.md`.**
   More tractable than the raw count implied: rendering is centralized behind `ogl.h`/`ogl.cpp`, the
   engine already drives its own `TMatrix<4,4>` (no GL matrix stack to replace), and vertex arrays are
   already the norm — only **7 true `glBegin` blocks**. Plan: introduce 2–3 shaders + a software matrix
   stack + an immediate-mode batching shim behind the existing abstraction, then work 6 milestones
   (M0 scaffolding → M6 cleanup), desktop-first against a GLES2-compatible context before Android.
3. **SFML on Android:** SFML 2.5 has an official Android/NDK backend — stand up the SFML Android
   project template (Gradle + NDK), reuse OneCube keystore/signing.
4. **Controls:** steering via **device tilt (accelerometer)** with on-screen touch fallback
   (brake/jump/paddle buttons). Tune for 6–10 — forgiving, simple.
5. **Assets:** package `data/` (courses, models, textures, music) into the APK/OBB.
6. **Sign & sideload:** release-sign with the existing keystore (`~/keystores/`), `assembleRelease`,
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
