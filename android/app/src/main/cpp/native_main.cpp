// Penguin Dash — Android native entry (port step 3, milestones A0 + A1c).
//
// A pure-NativeActivity host: android_native_app_glue drives the lifecycle, we
// stand up an EGL GLES2 context on the activity's window, then hand control to
// the ETR engine (pd_engine_main) which renders through the SFML-compat layer.
// The engine reaches the surface/input only through native_bridge.h. No Java.

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#include <cstdio>
#include <deque>
#include <string>

#include "native_bridge.h"

#define LOG_TAG "PenguinDash"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {

struct Engine {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    EGLint format = 0;     // native visual id for the chosen config
    int32_t width = 0;
    int32_t height = 0;
    bool running = false;  // true once we hold a usable GL surface
    bool closing = false;  // activity asked to finish
    std::deque<pd::InputEvent> input;  // filled on the glue thread, drained by the engine
};

Engine g_engine;

// Filesystem roots (A4). Resolved once in extract_assets().
std::string g_dataDir;     // <internalDataPath>/etr — the ETR data root
std::string g_configDir;   // <internalDataPath>     — writable (options/players)

// mkdir -p for the directory portion of a file path.
void make_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); i++)
        if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0775);
}

bool read_asset_text(AAssetManager* mgr, const char* name, std::string& out) {
    AAsset* a = AAssetManager_open(mgr, name, AASSET_MODE_BUFFER);
    if (!a) return false;
    off_t len = AAsset_getLength(a);
    out.resize(static_cast<std::size_t>(len));
    if (len > 0) AAsset_read(a, &out[0], len);
    AAsset_close(a);
    return true;
}

// Extract packaged assets to internal storage on first run (or after an update),
// keyed on the packaged assetver.txt vs a stored marker (OneCube pattern). ETR
// reads all its data via fopen, so a one-time extraction is simpler than hooking
// AAssetManager into every IO path.
void extract_assets(android_app* app) {
    AAssetManager* mgr = app->activity->assetManager;
    const std::string base = app->activity->internalDataPath ? app->activity->internalDataPath : ".";
    g_configDir = base;
    g_dataDir = base + "/etr";

    std::string wantVer, haveVer;
    read_asset_text(mgr, "assetver.txt", wantVer);
    if (FILE* f = std::fopen((base + "/assetver").c_str(), "rb")) {
        char buf[64]; std::size_t n = std::fread(buf, 1, sizeof(buf) - 1, f); buf[n] = 0;
        haveVer = buf; std::fclose(f);
    }
    if (!wantVer.empty() && wantVer == haveVer) {
        LOGI("assets up to date (ver %s)", wantVer.c_str());
        return;
    }

    std::string list;
    if (!read_asset_text(mgr, "filelist.txt", list)) { LOGW("no filelist.txt asset"); return; }
    mkdir(g_dataDir.c_str(), 0775);

    int count = 0;
    std::size_t pos = 0;
    while (pos < list.size()) {
        std::size_t nl = list.find('\n', pos);
        std::string rel = list.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? list.size() : nl + 1;
        while (!rel.empty() && (rel.back() == '\r' || rel.back() == '\n' || rel.back() == ' '))
            rel.pop_back();
        if (rel.empty() || rel == "filelist.txt" || rel == "assetver.txt") continue;

        AAsset* a = AAssetManager_open(mgr, rel.c_str(), AASSET_MODE_STREAMING);
        if (!a) { LOGW("asset missing: %s", rel.c_str()); continue; }
        const std::string dst = g_dataDir + "/" + rel;
        make_parent_dirs(dst);
        if (FILE* out = std::fopen(dst.c_str(), "wb")) {
            char buf[65536]; int r;
            while ((r = AAsset_read(a, buf, sizeof buf)) > 0) std::fwrite(buf, 1, r, out);
            std::fclose(out);
            count++;
        }
        AAsset_close(a);
    }
    if (FILE* f = std::fopen((base + "/assetver").c_str(), "wb")) {
        std::fwrite(wantVer.data(), 1, wantVer.size(), f); std::fclose(f);
    }
    LOGI("asset extraction complete: %d files -> %s", count, g_dataDir.c_str());
}

// (Re)create the window surface for the current app->window and make it current.
// Split from context creation so we can rebuild the surface when the window is
// resized/recreated (rotation, background, or the pre-layout 1x1 race on some
// devices where APP_CMD_INIT_WINDOW fires before the view is laid out).
bool egl_create_surface(Engine* e) {
    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, e->format);
    e->surface = eglCreateWindowSurface(e->display, e->config, e->app->window, nullptr);
    if (e->surface == EGL_NO_SURFACE) { LOGW("eglCreateWindowSurface failed"); return false; }
    if (!eglMakeCurrent(e->display, e->surface, e->surface, e->context)) {
        LOGW("eglMakeCurrent failed");
        return false;
    }
    eglQuerySurface(e->display, e->surface, EGL_WIDTH, &e->width);
    eglQuerySurface(e->display, e->surface, EGL_HEIGHT, &e->height);
    LOGI("EGL surface: %dx%d, GL_VERSION=%s", e->width, e->height, glGetString(GL_VERSION));
    return true;
}

bool egl_init(Engine* e) {
    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_BLUE_SIZE,  8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE,   8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,   // ETR's Tux shadow uses the stencil buffer
        EGL_NONE
    };
    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) { LOGW("eglGetDisplay failed"); return false; }
    if (!eglInitialize(display, nullptr, nullptr)) { LOGW("eglInitialize failed"); return false; }

    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
        LOGW("eglChooseConfig found no matching config");
        return false;
    }

    // On Android the native window buffer geometry must match the EGL config's
    // native visual id.
    EGLint format = 0;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    // Reuse a context across surface loss (background/foreground) so GL objects
    // survive; only create it once.
    if (e->context == EGL_NO_CONTEXT) {
        e->context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (e->context == EGL_NO_CONTEXT) { LOGW("eglCreateContext failed"); return false; }
    }

    e->display = display;
    e->config = config;
    e->format = format;

    if (!egl_create_surface(e)) return false;
    e->running = true;
    return true;
}

// Tear down just the surface (context is kept for the app's lifetime).
void egl_drop_surface(Engine* e) {
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
    }
    e->surface = EGL_NO_SURFACE;
    e->running = false;
}

// Rebuild the surface if the native window has since been laid out to a new size
// (handles the 1x1-at-init race without waiting on a resize event).
void egl_check_resize(Engine* e) {
    if (!e->running || e->app->window == nullptr) return;
    int32_t w = ANativeWindow_getWidth(e->app->window);
    int32_t h = ANativeWindow_getHeight(e->app->window);
    if (w == e->width && h == e->height) return;
    egl_drop_surface(e);
    egl_create_surface(e);
    e->running = (e->surface != EGL_NO_SURFACE);
}

void egl_term(Engine* e) {
    egl_drop_surface(e);
    if (e->display != EGL_NO_DISPLAY) {
        if (e->context != EGL_NO_CONTEXT) eglDestroyContext(e->display, e->context);
        eglTerminate(e->display);
    }
    e->display = EGL_NO_DISPLAY;
    e->context = EGL_NO_CONTEXT;
}

void handle_cmd(android_app* app, int32_t cmd) {
    Engine* e = static_cast<Engine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) egl_init(e);
            break;
        case APP_CMD_TERM_WINDOW:
            egl_drop_surface(e);   // keep the context; surface returns on re-init
            break;
        case APP_CMD_DESTROY:
            e->closing = true;
            break;
        default:
            break;
    }
}

// Translate a native input event into our queue. Returns 1 when consumed.
int32_t handle_input(android_app* app, AInputEvent* ev) {
    Engine* e = static_cast<Engine*>(app->userData);
    int32_t type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        int x = static_cast<int>(AMotionEvent_getX(ev, 0));
        int y = static_cast<int>(AMotionEvent_getY(ev, 0));
        pd::EvKind k;
        switch (action) {
            case AMOTION_EVENT_ACTION_DOWN:   k = pd::EvKind::PointerDown; break;
            case AMOTION_EVENT_ACTION_MOVE:   k = pd::EvKind::PointerMove; break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_CANCEL: k = pd::EvKind::PointerUp;   break;
            default: return 1;
        }
        e->input.push_back({k, x, y, 0});
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(ev);
        int code = AKeyEvent_getKeyCode(ev);
        if (action == AKEY_EVENT_ACTION_DOWN)
            e->input.push_back({pd::EvKind::KeyDown, 0, 0, code});
        else if (action == AKEY_EVENT_ACTION_UP)
            e->input.push_back({pd::EvKind::KeyUp, 0, 0, code});
        return 1;
    }
    return 0;
}

// Service the looper once. block=true parks until an event arrives (used while we
// have no surface); block=false returns immediately (per-frame pump).
void pump(bool block) {
    Engine* e = &g_engine;
    int events;
    android_poll_source* source;
    while (ALooper_pollAll(block ? -1 : 0, nullptr, &events,
                           reinterpret_cast<void**>(&source)) >= 0) {
        if (source != nullptr) source->process(e->app, source);
        if (e->app->destroyRequested != 0) { e->closing = true; return; }
        if (block) break;   // one blocking wait is enough; caller re-checks state
    }
    egl_check_resize(e);
}

} // namespace

// ---- native_bridge.h implementations ----
namespace pd {

bool SurfaceReady() { return g_engine.running && g_engine.surface != EGL_NO_SURFACE; }

void GetSurfaceSize(unsigned& w, unsigned& h) {
    w = static_cast<unsigned>(g_engine.width);
    h = static_cast<unsigned>(g_engine.height);
}

void SwapBuffers() {
    Engine* e = &g_engine;
    // If the surface is momentarily gone (backgrounded), park until it comes
    // back or the app is closing — never spin the engine loop on a dead context.
    while (!SurfaceReady() && !e->closing)
        pump(true);
    if (SurfaceReady())
        eglSwapBuffers(e->display, e->surface);
}

void PumpEvents() { pump(false); }

bool ShouldClose() { return g_engine.closing || g_engine.app->destroyRequested != 0; }

bool PollInput(InputEvent& out) {
    if (g_engine.input.empty()) return false;
    out = g_engine.input.front();
    g_engine.input.pop_front();
    return true;
}

const char* DataDir() { return g_dataDir.c_str(); }
const char* ConfigDir() { return g_configDir.c_str(); }

} // namespace pd

void android_main(android_app* app) {
    g_engine = Engine{};
    g_engine.app = app;
    app->userData = &g_engine;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    // Unpack game data to internal storage before the engine boots (A4).
    extract_assets(app);

    // Wait for the first usable surface before booting the engine (it needs a
    // live GLES2 context for shader/texture init).
    while (!pd::SurfaceReady() && !g_engine.closing)
        pump(true);

    if (pd::SurfaceReady())
        pd_engine_main();   // runs the ETR init + main loop; returns on quit

    egl_term(&g_engine);
}
