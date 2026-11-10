# Penguin Dash — agent notes

Extreme Tux Racer fork, ported to Android. Desktop build stays upstream autotools; the Android shell is direct-EGL/NativeActivity under `android/` on a GLES2 renderer (background and audit: `docs/gles_render_audit.md`).

## Commands

- Desktop: `./configure && make` → `src/etr` (autotools, SFML 2.5/2.6, OpenGL).
- Android: `cd android && ./gradlew :app:assembleRelease` (NDK 26+, SDK platform-34, a full JDK 21; signing reads a gitignored `android/keystore.properties`, else falls back to debug signing).
- No test suite. Gate: both builds compile, then check rendering changes on a real device — emulator GLES behavior does not match hardware.

## Plans — status lives there, never here

- `docs/android_port_plan.md`: milestones and device-verification state. Read it before touching `android/`.
- `docs/performance_review.md`: performance plan, state and gotchas.
- `docs/port_plan.md`: overall port strategy; `docs/codebase_modernization_review.md`: codebase notes.

**Public repo (GPL-2.0).** Lab details (build hosts, devices, push flow) go in the gitignored `CLAUDE.local.md`.
