# Penguin Dash modernization review

Generated: 2026-07-01

Audience: follow-up coding model or reviewer. This is a review report only; no fixes were applied.

## Scope

Reviewed the current repository as an in-progress modernization/Android port of Extreme Tux Racer.
Focus areas:

- consistency of the port and rebrand
- renderer modernization direction and remaining GLES2 blockers
- runtime efficiency risks
- security and robustness issues, especially malformed local assets/configuration

Validation run:

- `make -j2` from the repository root completed successfully.

No network-facing code, shell execution, credential handling, or obvious secret material was found in
the hand-maintained source. The main security boundary is local file input: bundled assets today, and
potential downloaded/modded assets later.

## High-priority findings

### 1. Malformed course images can drive out-of-bounds reads

`CCourse::LoadTerrainMap()` logs a terrain-size mismatch but continues to index `terrImage` using the
elevation-map dimensions (`nx`, `ny`). If `terrain.png` is smaller than `elev.png`, the loop can read
past `terrImage.getPixelsPtr()`.

Relevant code:

- `src/course.cpp:599-627`

`CCourse::LoadAndConvertObjectMap()` has the same shape for `trees.png`: it uses `nx`/`ny` from the
elevation map without validating the object-map dimensions before indexing the pixel buffer.

Relevant code:

- `src/course.cpp:430-503`

Risk:

- crash or undefined behavior from a malformed course package
- possible memory disclosure/crash primitives if user-supplied courses are ever supported
- Android asset handling will make hard failure preferable to desktop-style best-effort loading

Recommended fix:

- Treat image size mismatch as a load failure and return before reading pixels.
- Add maximum accepted dimensions before allocating `Fields`/GL arrays.
- Add a small malformed-course regression test with undersized `terrain.png` and `trees.png`.

### 2. Parsed asset IDs and cross-references are not consistently validated

Several loaders use parsed names or IDs directly as indexes or `unordered_map::at()` lookups. With the
bundled data this works, but malformed data can terminate the process, silently pick the wrong asset,
or allocate unreasonable memory.

Examples:

- `src/textures.cpp:170-179`: `id` from `textures.lst` is used in a resize calculation before the
  `id >= 0` check. Very large positive IDs, or negative IDs other than `-1`, can request huge vectors.
- `src/game_ctrl.cpp:44-89`: event/cup/race references are resolved with unchecked `GetRaceIdx()` and
  `GetCupIdx()` calls, both backed by `map.at()`.
- `src/course.cpp:352-386`: item names use `ObjectIndex[name]`, which silently inserts a missing name
  as index `0`, changing behavior instead of failing.
- `src/audio.cpp:189-200`: racing theme names use `MusicIndex[item]`, which silently maps missing music
  to index `0` and can go out of bounds if no music loaded.
- `src/course.cpp:634-685`: course group/list loading assumes course dirs and `course.dim` data are
  valid after partial load failures.

Risk:

- denial of service on malformed assets
- silent content substitution that makes debugging port issues hard
- weak security posture if future Android builds ingest third-party courses, translations, or themes

Recommended fix:

- Add typed lookup helpers that return `bool` plus an out parameter, or adopt an optional-like type if
  the project moves beyond C++14.
- Validate ranges before resizing or indexing.
- Fail a single bad asset with a clear error rather than throwing or silently defaulting.
- Avoid `operator[]` for lookup-only map access.

## Medium-priority findings

### 3. The new 3D shader path still depends on desktop fixed-function state snapshots

The renderer migration is well centralized, but the current 3D shader bridge snapshots fixed-function
state with `glGet*`, `glGetLightfv`, `glGetMaterialfv`, and `glGetTexGenfv`.

Relevant code:

- `src/glshader.h:58-65`
- `src/glshader.cpp:393-445`
- `src/quadtree.cpp:1126-1134`

Risk:

- these state queries are unavailable or inappropriate in GLES2
- `glGet*` can cause driver synchronization/stalls on desktop
- deferred removal may make later Android debugging harder because shader behavior is still coupled to
  the fixed-function pipeline

Recommended fix:

- Move matrix, material, fog, texgen, and light state into explicit engine-side state structs.
- Update `PushRenderMode()`/render setup paths to write tracked state.
- Make shader draw calls consume tracked state only, then disable the desktop snapshot path behind a
  compile-time flag.

### 4. Some converted draw paths create avoidable per-object allocations and many tiny draws

`Shader3D_DrawQuadArray()` builds a new index vector on every call. `DrawTrees()` calls it for each
tree/item, so dense courses allocate and upload tiny index buffers repeatedly.

Relevant code:

- `src/glshader.cpp:563-573`
- `src/course_render.cpp:71-120`

Particles and snow curtains remain many immediate/client-array style draws with matrix push/pop per
flake/curtain element.

Relevant code:

- `src/particles.cpp:506-519`
- `src/particles.cpp:767-799`

Recommended fix:

- Use static index arrays for one-quad/two-quad cases or draw pre-expanded triangles.
- Batch trees/items by texture and draw mode.
- Convert particles/snow to a single dynamic vertex buffer per pass.

### 5. Failed resource loads leak or leave invalid objects behind

Resource loaders often allocate before validating the load result.

Examples:

- `src/audio.cpp:56-63`: failed sound-buffer load leaves the new `TSound` in `sounds`.
- `src/audio.cpp:159-167`: failed music load leaks the newly allocated `sf::Music`.
- `src/font.cpp:111-119`: failed font load leaves a failed `sf::Font` in `fonts`.

Risk:

- memory leaks on partial asset failures
- invalid entries can shift indexes or break later assumptions
- harder Android diagnostics when an APK asset is missing

Recommended fix:

- Load into local value/`unique_ptr`, then commit to the container only after success.
- Prefer `std::vector<std::unique_ptr<T>>` or value containers where SFML types allow it.

### 6. Config and user-data paths retain legacy names and loose permissions

The Linux config directory remains `.etr`/`etr`, while the product is now Penguin Dash. Directories are
created with `0775`, and saves use plain `ofstream` paths.

Relevant code:

- `src/game_config.cpp:296-323`
- `src/winsys.cpp:179-195`
- `src/common.cpp:129-132`

Risk:

- group-writable config/screenshot directories are broader than needed
- rebrand inconsistency will carry into Android storage naming unless addressed
- future imported configs/assets need path rules before they become a user-controlled input boundary

Recommended fix:

- Use app-private directories on Android and `0700` for desktop config directories.
- Rename config roots intentionally, with a one-time migration from `.etr` if desktop continuity is
  desired.
- Centralize path construction and reject absolute paths / `..` components for asset references.

## Low-priority consistency and correctness notes

- Branding is inconsistent: `README.md` says Penguin Dash, while `configure.ac`, `src/bh.h`, binary
  name, window title, and config directory still say Extreme Tux Racer/`etr`.
- Build metadata is still autotools-first and generated files are present. `src/Makefile.am` is current,
  but Android should use a single source-list authority (likely CMake/Gradle/NDK) to avoid source list
  drift.
- `src/bh.h` includes desktop `<GL/gl.h>` and `src/ogl.cpp` includes `<GL/glu.h>`. This should be split
  behind a render backend/platform header before the GLES build.
- Ownership style is mixed. The new port code uses `std::vector`/`std::map`, while much of the legacy
  code still uses raw owning pointers and manual cleanup. This is not urgent everywhere, but loader
  boundaries would benefit from RAII first.
- `src/game_ctrl.cpp:187-188` appears to set the active player to `plyr.size() - 1` whenever any loaded
  player has `[active]`, instead of setting it to the current loop index.
- `src/font.cpp:47` checks `s[start] != '0'` instead of the null terminator `'\0'`, which can omit a
  final word equal to `"0"` and is likely a typo.
- `src/textures.cpp:66-73` and `src/textures.cpp:87-95` query texture dimensions with
  `glGetTexLevelParameteriv`; use `sf::Texture::getSize()` to avoid GL stalls and GLES incompatibility.

## Suggested next pass

1. Fix the image size/OOB issues and add malformed-asset tests first.
2. Introduce central safe lookup/path helpers for all `CSPList`-parsed references.
3. Replace the `glGet*` shader-state bridge with tracked renderer state before doing the Android GL
   context switch.
4. Batch tree/item/particle draws after correctness is stable; this should improve both desktop and
   Android performance.
5. Decide whether the rebrand includes package/binary/config migration now, before Android project
   scaffolding bakes in more names.
