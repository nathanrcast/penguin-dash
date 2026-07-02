// Penguin Dash — Android native bridge (port step 3, A1c).
//
// The thin seam between the raw EGL/native-glue host (native_main.cpp) and the
// ETR engine's SFML-compat platform layer (sfml_compat.cpp / winsys.cpp). The
// host owns the EGL surface, the android_app glue, and the input queue; the
// engine reaches it only through these free functions so neither file has to
// know the other's internals.
#ifndef PD_NATIVE_BRIDGE_H
#define PD_NATIVE_BRIDGE_H

namespace pd {

// True once a usable EGL surface + current GLES2 context exists.
bool SurfaceReady();

// Current EGL surface size in pixels (0x0 until SurfaceReady()).
void GetSurfaceSize(unsigned& w, unsigned& h);

// Present the current frame. If the surface is momentarily gone (app
// backgrounded) this blocks, pumping lifecycle, until it returns or a close is
// requested — so the engine's blocking main loop can't spin on a dead context.
void SwapBuffers();

// Service the native-glue looper once, non-blocking: drains lifecycle commands
// (surface create/destroy/resize) and input into the queue. Call once per frame.
void PumpEvents();

// True after the activity has been asked to finish; drives winsys isOpen().
bool ShouldClose();

// Queued input, translated by winsys into sf::Event. Pointer coords are in
// surface pixels; key is an Android AKEYCODE_* (only the few the menus need).
enum class EvKind { PointerDown, PointerMove, PointerUp, KeyDown, KeyUp };
struct InputEvent {
	EvKind kind;
	int x, y;      // pointer position (Pointer*), else 0
	int keycode;   // AKEYCODE_* (Key*), else 0
};
bool PollInput(InputEvent& out);

// Filesystem roots resolved at boot (A4). DataDir is where APK assets were
// extracted (the ETR data root); ConfigDir is the app's writable dir (options,
// saved players, high scores). Both valid after android_main's extract step.
const char* DataDir();
const char* ConfigDir();

} // namespace pd

// Engine entry, implemented in android_entry.cpp: runs the full ETR init + main
// loop on the live EGL surface. Called by android_main once SurfaceReady().
void pd_engine_main();

#endif
