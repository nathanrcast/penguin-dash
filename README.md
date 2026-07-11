# Penguin Dash

A kid-friendly (ages 6–10) downhill racer for Android tablets: slide Tux the penguin down snowy
slopes on his belly, grab herring, and beat the clock. Ad-free, offline, open source.

**Penguin Dash is a rebranded fork of [Extreme Tux Racer](https://sourceforge.net/projects/extremetuxracer/)**,
being modernized and ported to Android. It remains licensed under the **GPL** (see `COPYING`); original
project docs are preserved in `UPSTREAM_README` and authors in `AUTHORS`.

Fork base: Extreme Tux Racer **0.8.4** source tarball (imported 2026-06-30; upstream ships tarballs,
no public git, so there is no upstream commit history). Play-tested + approved 2026-06-30.

## Status

**Released for Android — v0.5.0.** A signed APK is on the
[Releases page](https://github.com/nathanrcast/penguin-dash/releases/latest). The legacy
immediate-mode GL renderer has been rewritten on **GLES2 shaders** (desktop and Android share the
code path), and an Android app under `android/` hosts the engine natively (`NativeActivity` + EGL,
no Java activity): menus, full races, HUD, and touch + tilt controls run on device. Audio is wired
(MediaPlayer music + SoundPool SFX) and verified on device.

v0.5.0 replaces the fixed onscreen D-pad with a **floating stick** (relocates to the thumb on press;
knob follows the drag) and raises its rest position off the bottom bezel, plus a Configuration
screen layout pass so rows no longer overlap and arrow hit targets are finger-sized. v0.4.0 added
**Movement** (Tilt / Onscreen) and **Sensitivity** (1–10). v0.3.0 landed the performance overhaul —
static-VBO terrain, batched particles, and a render-scale option for older tablets. Play-tested on
the Galaxy Tab A9+; older tablets should set render scale to 67–75%. History:
[`docs/port_plan.md`](docs/port_plan.md) →
[`docs/gles_render_audit.md`](docs/gles_render_audit.md) →
[`docs/android_port_plan.md`](docs/android_port_plan.md) →
[`docs/performance_review.md`](docs/performance_review.md).

## Install

Download `penguin-dash-v*.apk` from the
[latest release](https://github.com/nathanrcast/penguin-dash/releases/latest) and sideload it
(enable "install unknown apps"). Package `com.anathemasit.penguindash`.

## Building

- **Desktop (Linux):** autotools + SFML 2.5/2.6 + OpenGL — `./configure && make`, binary at `src/etr`.
- **Android:** `cd android && ./gradlew :app:assembleRelease` (needs NDK 26+, SDK platform-34, a full
  JDK 21; release signing reads a gitignored `android/keystore.properties`, else falls back to debug
  signing). Game data under `data/` is packaged into the APK and extracted on first run.

## Credits & license

- Original game: Extreme Tux Racer team (`AUTHORS`), descended from Tux Racer.
- Licensed under the GNU General Public License (`COPYING`). Source stays open per the GPL.
