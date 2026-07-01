// Penguin Dash — Android native entry (port step 3, milestone A0).
//
// A pure-NativeActivity host: android_native_app_glue drives the lifecycle, we
// stand up an EGL GLES2 context on the activity's window and run a clear-color
// loop. This proves the app boots, signs, installs, and holds a live GLES2
// surface — the foundation the (already GLES2-native) ETR renderer plugs into
// at milestone A1. No SFML, no Java.

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cmath>

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
    float t = 0.0f;        // animates the clear colour so it's visibly alive
};

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

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) { LOGW("eglCreateContext failed"); return false; }

    e->display = display;
    e->config = config;
    e->format = format;
    e->context = context;

    if (!egl_create_surface(e)) return false;
    e->running = true;
    return true;
}

// Rebuild the surface if the native window has since been laid out to a new size
// (handles the 1x1-at-init race without waiting on a resize event).
void egl_check_resize(Engine* e) {
    if (!e->running || e->app->window == nullptr) return;
    int32_t w = ANativeWindow_getWidth(e->app->window);
    int32_t h = ANativeWindow_getHeight(e->app->window);
    if (w == e->width && h == e->height) return;
    eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
    e->surface = EGL_NO_SURFACE;
    egl_create_surface(e);
}

void egl_term(Engine* e) {
    if (e->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->context != EGL_NO_CONTEXT) eglDestroyContext(e->display, e->context);
        if (e->surface != EGL_NO_SURFACE) eglDestroySurface(e->display, e->surface);
        eglTerminate(e->display);
    }
    e->display = EGL_NO_DISPLAY;
    e->surface = EGL_NO_SURFACE;
    e->context = EGL_NO_CONTEXT;
    e->running = false;
}

void draw_frame(Engine* e) {
    if (!e->running) return;
    egl_check_resize(e);
    e->t += 0.02f;
    // slow cool-blue pulse — obviously "alive", obviously not the game yet
    float b = 0.5f + 0.5f * std::sin(e->t);
    glViewport(0, 0, e->width, e->height);
    glClearColor(0.05f, 0.15f + 0.15f * b, 0.35f + 0.25f * b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    eglSwapBuffers(e->display, e->surface);
}

void handle_cmd(android_app* app, int32_t cmd) {
    Engine* e = static_cast<Engine*>(app->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) egl_init(e);
            break;
        case APP_CMD_TERM_WINDOW:
            egl_term(e);
            break;
        default:
            break;
    }
}

} // namespace

void android_main(android_app* app) {
    Engine engine;
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = handle_cmd;

    while (true) {
        int events;
        android_poll_source* source;
        // Timeout must be re-evaluated on every poll: block (-1) while we have no
        // surface, but return immediately (0) once running so we fall through to
        // draw each frame. Hoisting it into a variable would leave it stale when
        // `running` flips true mid-loop and hang on the blocking poll.
        while (ALooper_pollAll(engine.running ? 0 : -1, nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested != 0) {
                egl_term(&engine);
                return;
            }
        }
        draw_frame(&engine);
    }
}
