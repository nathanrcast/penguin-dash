# Penguin Dash — Android performance review (framerate & stutter)

Review done 2026-07-10 after stutter was observed in a play-through on the Tab A9+ (Mali-G52 MP2,
1920×1200 surface). Targets: smooth on the A9+ and headroom for older/weaker hardware.
Findings are split into **hitch sources** (frame-time spikes = the felt stutter) and **constant
frame cost** (low average FPS; under vsync this oscillates 60↔30 and also reads as stutter).

## TL;DR — ranked fix list

| # | Fix | Type | Effort | Expected win |
|---|-----|------|--------|--------------|
| P1 | ✅ SFX off MediaPlayer → SoundPool; kill per-frame JNI | hitch | S–M | removes pickup/terrain-change spikes |
| P2 | ✅ Batch snowflakes/curtains/particles into one draw per group | constant | S | −500…−3000 draw calls/frame |
| P3 | ✅ Fix curtain double-draw (upstream bug) | constant | XS | halves curtain fill cost |
| P4 | ✅ Cheapen per-tree draws (static index buffer, skip normal-matrix, hoist rot) | constant | S | big CPU win on tree-heavy courses |
| P5 | ✅ Terrain: static VBO + terrain-id attrib | constant | M–L | removes MB-scale per-frame copies |
| P6 | Optional render-scale (smaller EGL surface, HW upscale) | constant | S | ~2× fill headroom for old devices |
| P7 | ✅ Clamp `time_step` max | playability | XS | hitches stop cascading into physics jumps |

**Stage 1 (P1+P3+P7) SHIPPED 2026-07-10**, device-verified on the A9+ (race runs, SoundPool
sessions active, no missing-SFX warnings, no crash). Implementation notes: SFX playback state is
tracked natively (SoundPool has no is-playing query) — looping streams by flag, one-shots by
load-time WAV-header duration — preserving the `getStatus()==Playing` guard that rate-limits
`tree_hit` retriggering. Music stays on MediaPlayer (state-transition only). Remaining human
check: confirm by ear that pickups no longer stutter mid-race.

**Stage 3 (P5) SHIPPED 2026-07-10**, device-verified on the A9+ against a parent-build A/B at the
same start-gate frame (identical: snow grain, shaded banks, ice field with edge blending; full
race, no crash). Implementation: `Fields[].terrain` is baked into the VNC colour alpha byte and
the whole array uploaded once per course to a static VBO (`Shader3D_UploadVNC` from
`FillGlArrays`); the per-pass colour/alpha the CPU used to rewrite each frame is derived in VS_3D
from `u_terrainMode`/`u_terrain` (1 = main `tid >= t`, 2 = env black base, 3 = env additive
`tid == t`); `quadsquare::Render` sets the pass and never touches vertex memory. Client-array
fallback kept for drivers without buffer objects (same shader logic). **Two gotchas hit:**
(1) `LoadCourse` filled the GL arrays *before* `LoadTerrainMap`, so the baked terrain ids were
all zero — harmless before (terrain was read live per frame), invisible-terrain regions with the
VBO; `FillGlArrays` now runs after the terrain map. (2) A/B builds by overwriting files in
`~/penguin-dash-build` gave sources older mtimes than existing objects — ninja/make kept stale
`.o`s; `touch src/*` before rebuilding after any content-only file swap.

**Stage 2 (P2+P4) SHIPPED 2026-07-10**, device-verified on the A9+ (snow grade 2 race: batched
flakes + curtains + brake-spray particles, trees/items/herring, full race → game-over loop, no
crash). Implementation: flakes/curtain-tiles/race-particles each collect into reused vertex
vectors and issue one draw per group (`append_billboard_quad`; particles carry per-vertex colour
via new `Shader3D_SetTexturedColoredArray` since alpha fades per particle; >16384-quad batches
chunk on the GLushort index limit). `Shader3D_DrawQuadArray`'s index table is now function-static
(was a heap-allocated rebuild per tree per frame). Trees/items use the new fast model path —
`Shader3D_SetModelRotation` folds the shared 1° rotation into the base once,
`Shader3D_SetModelTranslation` recomputes only the modelview's 4th column per object — no
per-object 4×4 multiply or normal-matrix cofactor inverse; `Shader3D_Begin3D` uploads
identity-model matrices so particle systems skip SetModel3D entirely. Menu 2D snow
(per-sprite draws in `draw_ui_snow`) deliberately deferred — menu-only, revisit if menus
ever profile slow on older devices.
**Post-ship fix (2026-07-10, user-reported):** the fast path's translation column was computed
from `base = view*rot` instead of `view`, spinning every tree's *position* 1° about the world
origin — negligible near the start (where the A/B looked), ~10+ units sideways far down-course,
leaving trees hovering over ground of a different height. `T*R` carries the raw translation, so
the modelview 4th column must be `view*[t,1]`. Grounded-tree A/B re-verified on-device at the
00:26/00:36 course sections.

## Hitch sources (frame spikes)

### H1. Synchronous `MediaPlayer.prepare()` on the render thread — the likely felt stutter

`sfml_compat.cpp` `Sound::play()` runs `prepareFile()` → JNI `MediaPlayer.setDataSource` +
**`prepare()`** (blocking codec init, tens of ms) on the game thread every time a
non-already-playing sound starts:

- **Herring pickup fires three of these back-to-back** — `physics.cpp:196-198` plays
  `pickup1`+`pickup2`+`pickup3`. One pickup ≈ 3 × (prepare + setLooping + setVolume + start)
  JNI calls → a guaranteed multi-frame hitch on every herring.
- Terrain change (snow↔ice↔rock) does `Halt` + fresh `prepare` of the new loop
  (`racing.cpp:264-280` `PlayTerrainSound`).
- Tree collision → `tree_hit` same path (`physics.cpp:143`).

Additionally `TSound::Play` (`audio.cpp:41-45`) checks `getStatus()` first, which on Android is a
JNI `isPlaying()` round-trip **including `GetObjectClass`+`GetMethodID` each call** — and
`PlayTerrainSound` calls it **every frame** while skiing.

**Fix (P1):** move SFX to `SoundPool` (loads .wav once at `LoadChunk` time; `play()` is async and
pooled — the standard Android answer), keeping MediaPlayer for music only. Short of that:
pre-create + prepare one MediaPlayer per SFX at load, cache `jmethodID`s once (they're stable),
and track playing-state natively so the per-frame `getStatus` never crosses JNI.

### H2. Lazy font-atlas bake

`GetAtlas` (`sfml_compat.cpp`) bakes a 512²/1024² atlas the first time a new pixel size is drawn —
one-time hitches on first HUD/menu text of each size. Minor; could pre-warm HUD sizes at load.

## Constant frame cost

### C1. Terrain: multi-pass over a client-side vertex array (biggest bandwidth item)

The course VNC array is `nx*ny` vertices × 36 B (`STRIDE_GL_ARRAY`) — 100×1000 = **3.6 MB**
typical, up to **14.4 MB** (chragis 200×2000). All terrain draws are client-side
`glDrawElements(GL_UNSIGNED_INT, …)` (`glshader.cpp:504-532` `Shader3D_BeginVNC` uses raw
pointers, no VBO), so the driver re-copies the referenced vertex range **on every draw call**.

At the default `perf_level=3` (`game_config.cpp:94`), `quadsquare::Render` (`quadtree.cpp:742-793`)
does per frame: one pass per terrain (≈3) + the env/blend block (`perf_level > 1`): 1 black pass +
one additive pass per terrain (≈3) → **~7 terrain draws/frame**, and between passes the CPU
rewrites the colour/alpha bytes of every referenced vertex (`colorval` loops) — scattered writes
over megabytes, which also defeats any driver caching.

**Fix (P5), staged:**
1. *Cheap stopgap:* on Android default `perf_level` to 2 —— or accept the visual change and use 1
   (single pass per terrain, `MakeNoBlendTri`). Also consider `course_detail_level` 75 → ~55 on
   weaker devices (quadtree threshold, `racing.cpp` → `RenderCourse`).
2. *Real fix:* upload the VNC array to a **static VBO** once per course; move the per-pass
   alpha trickery into the shader — add a per-vertex `terrain_id` attribute and select alpha by a
   `u_terrain` uniform (+ pass mode uniform for the black/env passes). Vertex data becomes fully
   immutable; per frame only the index list changes (stream it into an IBO or keep client-side —
   indices are small). Removes all CPU vertex writes and MB-scale copies.

Note: `GL_UNSIGNED_INT` indices need `OES_element_index_uint` on GLES2 — universal on GLES3-era
devices (A9+ is 3.2) but a compat risk on truly old GLES2-only hardware.

### C2. One draw call per snowflake / curtain tile / race particle

`particles.cpp`:
- `TFlakeArea::Draw` (`particles.cpp:499-519`): each visible flake = its own
  `Shader3D_SetTexturedArray` (2 attrib pointer re-specs + constant-attr set) + 4-vertex
  `glDrawArrays`. Snow grade 3 = 3 areas × 1000 flakes → **hundreds to ~2-3k draw calls/frame**.
  This alone can dominate CPU/driver time on Mali.
- Race particles (`Particle::draw_billboard`) and curtains (`TCurtain::Draw`) use the same
  per-quad pattern.
- Menus: `draw_ui_snow` draws each 2D flake as an `sf::Sprite` → one `Shader2D` draw each
  (~1000+), with `Begin2D/End2D` state churn per sprite (`sfml_compat.cpp:503-534`).

**Fix (P2):** batch per group — build one interleaved pos/uv array per flake area / curtain /
particle system per frame (they already share texture + colour) and issue **one**
`glDrawArrays(GL_TRIANGLES)`. Straightforward: the per-quad corner math already produces the
vertices; append to a reused `std::vector` instead of drawing. Same for the 2D menu snow (one
atlas texture, one draw).

### C3. Snow curtains drawn twice per frame (upstream ETR bug)

`CCurtain::Update` ends by calling `Draw()` (`particles.cpp:852`) and the race loop then calls
`DrawSnow` → `Curtain.Draw()` again (`particles.cpp:1122`, loop at `racing.cpp:405-406`).
Curtains are large near-camera alpha-blended quads — at 1920×1200 this doubles a significant
fill cost. **Fix (P3):** delete the `Draw()` call inside `Update`.

### C4. Per-tree draw overhead

`DrawTrees` (`course_render.cpp:51-181`), per tree per frame:
- `Shader3D_SetModel3D` → full 4×4 multiply + **double-precision 3×3 cofactor inverse** for the
  normal matrix + 3 matrix uniform uploads (`glshader.cpp:465-501`). For translate-only models the
  normal matrix **never changes** — compute it once in `Shader3D_Begin3D` and skip per object.
- `Shader3D_DrawQuadArray` **heap-allocates a `std::vector<GLushort>`** and rebuilds the same
  6-per-quad index pattern on every call (`glshader.cpp:568-578`) — for every tree, item, and both
  billboard passes. Make it a function-`static` table built once (cap at, say, 1024 quads).
- `rot.SetRotationMatrix(1, 'y')` is constant — hoist out of the loop (`course_render.cpp:81-87`).
- Two attrib re-specs per tree; trees sharing a texture could be batched into one array in
  world space (drop the per-tree model matrix, bake the translate into the 8 verts on the CPU).

### C5. Fill rate at native 1920×1200 (older-hardware lever)

The race frame is heavily overdrawn: full-screen skybox (depth off) + terrain (multi-pass) + fog
plane + curtains (×2 today) + particles + HUD, on a low-end GPU at native res. The standard
Android lever is a **smaller EGL surface with free hardware upscale**:
`ANativeWindow_setBuffersGeometry(window, w*s, h*s, format)` in `egl_create_surface`
(`native_main.cpp:267-279`), s ≈ 0.67 → 1280×800 = 44 % of the pixels. UI scales with it
(`CalcScreenScale` follows surface size). Ship as a config option (`res_type`-style) defaulting
on for weaker devices; on the A9+ try it after P1–P4 land.

### C6. Minor
- `check_gl_error()` every frame in `State::Manager::CallLoopFunction` (`states.cpp:119`) —
  `glGetError` is usually client-side but is pure overhead in release builds; gate it debug-only.
- `Text::draw` builds two `std::vector`s per string per frame (`sfml_compat.cpp:695`) — reuse
  static buffers if HUD text ever shows up in a profile.
- `Reshape` recomputes the projection every frame (`racing.cpp:409`) — cheap, ignore.

## Playability

### P7. No max clamp on `time_step`

`states.cpp:121` clamps only the minimum (`max(0.0001f, elapsed)`). After any hitch (audio
prepare, GC, atlas bake) physics integrates the whole gap in one step — with tilt steering that
reads as a teleport/lurch and compounds the felt stutter. Clamp to e.g. 0.05 f so a dropped frame
costs slow-motion for one frame instead of a jump.

## What's already right (don't redo)

- Tux sphere meshes cached per division count (`tux.cpp` `GetSphereMesh`); render-state tracked
  engine-side, no `glGet*` in the frame; native loop blocks properly when backgrounded; EGL
  vsync paces the loop; quadtree LOD + frustum culling working as upstream.

## Suggested order

1. **P1 audio** (removes the felt spikes) + **P3 curtain double-draw** + **P7 time_step clamp** — small, independent, ship together.
2. **P2 particle batching** + **P4 tree-draw cheapening** — mechanical, big draw-call win.
3. **P5 terrain VBO** — the largest single rendering item, needs the shader tweak; stopgap = Android `perf_level` default.
4. **P6 render scale** — measure after the above; likely the knob that makes truly old tablets viable.

Verify each stage on-device (A9+), watching frame pacing during: herring pickups, terrain
transitions, heavy-snow courses (grade 3), and tree-dense courses.
