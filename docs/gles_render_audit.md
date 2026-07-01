# Penguin Dash — GLES2 render audit & rewrite milestone (port step 2)

Audit of the OpenGL renderer to scope the GLES2 rewrite (the core Android-port task). Done 2026-07-01
against the desktop baseline build (`src/etr`, ETR 0.8.4).

## TL;DR — smaller than the raw count suggested

The "~76 immediate-mode GL sites" headline overstated it. Reality:

- **Rendering is already centralized** behind `ogl.h`/`ogl.cpp` (wrappers `glVertex3`, `glTexCoord2`,
  `glNormal3`, `glColor`, `glLoadMatrix`, `set_material`, and a `PushRenderMode/PopRenderMode` state
  stack). Only **6 files** call the draw wrappers directly. Retarget the abstraction, not the callers.
- **The matrix stack — usually the hardest GLES2 chore — is basically already gone.** Zero
  `glPushMatrix/glTranslate/glRotate/glScale/glOrtho`; the engine drives its own **`TMatrix<4,4>`**
  (`matrices.cpp`/`mathlib.cpp`) and uploads via a `glLoadMatrix(TMatrix)` wrapper. We feed those as
  uniforms — the math already exists.
- **Vertex arrays are already the norm** (`glVertexPointer` 23, `glTexCoordPointer` 11,
  `glEnableClientState` 29, `glDrawArrays` 22). These convert mechanically to VBO + `glVertexAttribPointer`.
- **True immediate mode is only 7 `glBegin` blocks** (localized, simple primitives).
- Pipeline is **pure fixed-function — no shaders yet**, so we introduce the first shaders (2–3 small ones).

Net: a concentrated, mostly-mechanical rewrite across ~8–10 files, with the real design work
front-loaded into getting the shaders + software matrix stack right (M0–M2 below).

## Subsystem inventory

| File | Role | GL style today | Rewrite work |
|---|---|---|---|
| `ogl.cpp`/`ogl.h` | GL core: matrix setup, render-mode state stack, material, draw wrappers | fixed-function + `TMatrix` | **retarget point** — shader mgr, sw matrix stack, uniform upload |
| `course_render.cpp` | Terrain mesh | vertex arrays | VBO + 3D shader (light+fog) |
| `env.cpp` | Sky / environment / fog | arrays + 1 `glBegin` QUAD_STRIP | 3D shader; convert quad-strip |
| `hud.cpp` | 2D HUD / speed gauge | arrays + 1 `glBegin` TRIANGLE_FAN | 2D shader; convert fan |
| `textures.cpp` | 2D textured quads (UI/images) | arrays | 2D shader |
| `font.cpp` | Text | (SFML text + GL) | 2D shader path |
| `particles.cpp` | Snow particles | arrays | 3D/point shader; verify blend |
| `track_marks.cpp` | Ski trail decals on snow | 2× `glBegin` (QUADS, QUAD_STRIP) | 3D shader; convert; verify blend |
| `tux.cpp` | Character model | **`gluSphere` quadrics** + 3× `glBegin` fans/strips | generate sphere meshes; convert fans |
| `tools*.cpp`, `ogl_test.cpp`, `quadtree.cpp` | Editor tools / test / spatial | `gluLookAt`, misc | low priority / last |

## GLES2 incompatibilities & exact sites

1. **Fixed-function transforms** — `ogl.cpp:166–181` (`glMatrixMode`/`glLoadIdentity`), `ogl.cpp:437`
   (`glLoadMatrixd`), `view.cpp:156/170` (`glLoadMatrix`). → software proj/modelview from `TMatrix`,
   upload MVP (+ normal matrix) as uniforms.
2. **Immediate mode (7 `glBegin`)** — `env.cpp:351`, `hud.cpp:111`, `track_marks.cpp:117/138`,
   `tux.cpp:672/690/706`. → build vertex arrays / use the batching shim (below).
3. **Client-state vertex arrays** (`glEnableClientState`/`glVertexPointer`/`glTexCoordPointer`/
   `glNormalPointer` + `glDrawArrays`) — bulk, across terrain/env/particles/textures. → VBO +
   generic vertex attribs. Mechanical.
4. **Lighting/material** — `glLight`×4, `glMaterial`×4 (`ogl.cpp:117–146` `set_material*`). → single
   directional light + material uniforms in the 3D shader.
5. **Fog** — `glFog`×4. → fog term in the 3D fragment shader.
6. **`glShadeModel(GL_SMOOTH)`** ×10 — no-op in GLES2 (smooth is default). → delete.
7. **`glAlphaFunc`** ×2 — no alpha test in GLES2. → `discard` in fragment shader.
8. **GLU** (not in GLES) — `gluErrorString` (→ static string map), `gluPerspective` (`ogl.cpp:180` →
   perspective `TMatrix`), `gluLookAt` (`tools.cpp:113`, editor → lookAt `TMatrix`), `gluSphere`+
   quadrics (`tux.cpp:367–372` → generate sphere mesh once).

## Rewrite architecture

- **Shader manager + 2–3 programs** (new, small):
  - **3D**: MVP + normal matrix, one directional light, material uniforms, optional texture, fog.
    Used by terrain, env, tux, track marks, particles.
  - **2D**: ortho MVP, texture + vertex color. Used by HUD, `textures.cpp`, fonts.
  - (optional 3rd: untextured colored.)
- **Immediate-mode batching shim in `ogl.cpp`:** reimplement `glBegin/glVertex3/glColor/glTexCoord2/
  glNormal3/glEnd` as a client-side batcher that flushes through the active shader. This keeps the 7
  `glBegin` sites and all wrapper call sites working with minimal edits — the pragmatic path.
- **Software matrix stack** in `ogl.cpp` using existing `TMatrix<4,4>`; add `Ortho/Perspective/LookAt`
  helpers; upload MVP on draw.
- **VBO-ify** the existing array paths behind the same abstraction.

## Milestones (desktop-first — validate on `.13` before Android)

Develop against a **GL 2.1 / GLES2-compatible subset** on desktop (SFML currently requests a GL 1.2
context in `winsys.cpp:101/103` — bump it), so each milestone is testable on `.13` before the Android
context switch.

- **M0 – Scaffolding:** ✅ **DONE 2026-07-01** (commit `bebff48`). `glshader.{h,cpp}` (GLSL program
  wrapper + GL2.0 entry points loaded via `sf::Context::getFunction` + core 2D/3D shaders) and
  `glmatrix.{h,cpp}` (Ortho/Perspective/LookAt builders + software matrix stack, column-major to match
  `TMatrix`). `InitCoreShaders()` called from `main` after GL init; GL context bumped 1.2→2.1. Shaders
  are compiled+linked but **not bound** → zero visual change. Verified on `.13`: build clean (no
  warnings), and a windowed smoke run logged *"core 2D/3D shaders compiled + linked OK"* on the live
  GL context. (`Makefile.in` hand-updated alongside `Makefile.am` — no autotools installed.)
- **M1 – 2D pipeline:** ✅ **DONE 2026-07-01** (part 1 `266df4e`: `TTexture::Draw*`; part 2 `bbcad5a`:
  HUD + numeric font). `textures.cpp` (`TTexture` draws + `DrawNumStr`/`DrawNumChr`) and `hud.cpp`
  (`draw_gauge` incl. the speed tri-fan, `DrawPercentBar`, `DrawWind`) routed onto the Shader2D path;
  `glTexGen` → inline UVs, matrix-stack translate/rotate → `Shader2D_SetModel`, immediate-mode
  `glBegin` → vertex arrays. `font.cpp` unchanged (SFML `sf::Text`, already GLES2-safe). **Setup2dScene
  intentionally left fixed-function** — the converted draws each set their own ortho via
  `Shader2D_Begin`, so nothing depends on it; it's removed in M6 cleanup with the other desktop-GL-only
  calls. **First visual proof met: menus + HUD render** (gauge + timer verified in a play-tested race).
- **M2 – Terrain:** ✅ **DONE 2026-07-01** (commit `74f4178`). The terrain mesh (`quadtree.cpp`
  `DrawTris`/`glDrawElements` over the interleaved VNC array) draws through the core 3D program.
  **Desktop-first key decision:** instead of intercepting the matrix/light/fog calls scattered across
  the engine, `Shader3D_BeginVNC` **snapshots the live fixed-function state via `glGet*`** at draw time
  (proj+modelview, light 0, global ambient, material, color-material flag, object-linear texgen planes,
  linear fog, enables) and reproduces it — guarantees a pixel match on desktop. VS_3D/FS_3D rewritten to
  **per-vertex Gouraud** lighting matching fixed-function. Verified rendering correctly (lighting+fog+
  texture, static and moving camera) in a Bunny Hill practice race. **Android TODO:** the `glGet`
  snapshot is desktop-GL-only; swap for tracked matrices/uniforms before the GLES2 context switch (M6).
- **M2b – Trees/items:** ✅ **DONE 2026-07-01** (commit `01fa2db`). `DrawTrees` billboards through the
  3D program: object path (`Shader3D_Begin3D` + per-object `Shader3D_SetModel3D` model matrix built in
  software — no matrix stack), `GL_QUADS`→triangles (`Shader3D_DrawQuadArray`), constant/per-item
  normal, and **alpha-test → `discard`** in FS_3D (snapshot from `GL_ALPHA_TEST`/`GL_ALPHA_TEST_REF`).
  Verified: trees + items render with correct billboard alpha transparency, lighting, placement, fog.
- **M3 – Environment:** ✅ **DONE 2026-07-01** (commit `09c8cca`). `env.cpp` `DrawSkybox` (unlit
  textured quads, `Shader3D_SetTexturedArray` + const white, model=translate-to-camera) and `DrawFog`
  (`GL_QUAD_STRIP`→`GL_TRIANGLE_STRIP`, per-vertex colour array, coloured by the shader fog term). The
  3D shader's lighting-OFF branch now uses the vertex colour directly (matches fixed-function ignoring
  material when lighting is off). Verified: skybox + distance fog render correctly.
- **M4 – Characters:** `tux.cpp` — replace `gluSphere` with a generated sphere mesh; convert the 3
  `glBegin` fans/strips; material/light. Tux renders correctly.
- **M5 – Decals & snow:** `track_marks.cpp` (2 `glBegin`) + `particles.cpp`; verify blend/alpha states.
- **M6 – Cleanup:** drop `glShadeModel`; `glAlphaFunc`→`discard`; remaining GLU; editor tools last;
  remove desktop-GL-only headers. **Full desktop GLES2 build green.**

Then → port_plan step 3 (SFML Android/NDK shell) with a renderer that already runs GLES2 on desktop.

## Effort & risks

- **Effort:** M0–M2 are the real design work (shaders + matrix upload + first VBO path); M3–M6 are
  largely repetition of the M2 pattern. Concentrated in ~8–10 files thanks to the central abstraction.
- **Risks:** (a) per-render-mode blend/alpha states for snow particles & track-mark decals must be
  preserved — audit `PushRenderMode` cases in `ogl.cpp:219–295`; (b) Tux sphere-mesh must match the
  old quadric look; (c) getting a true GLES2 context via SFML on desktop — fallback is developing
  against the GL2.1 subset and flipping the context attribute for Android only.
