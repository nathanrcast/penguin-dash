// Penguin Dash — Android native entry (port step 3, milestone A0).
//
// A pure-NativeActivity host: android_native_app_glue drives the lifecycle, we
// stand up an EGL GLES2 context on the activity's window and run a clear-color
// loop. This proves the app boots, signs, installs, and holds a live GLES2
// surface — the foundation the (already GLES2-native) ETR renderer plugs into
// at milestone A1. No SFML, no Java.

#include <android_native_app_glue.h>
#include <android/log.h>
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
    int32_t width = 0;
    int32_t height = 0;
    bool running = false;  // true once we hold a usable GL surface
    float t = 0.0f;        // animates the clear colour so it's visibly alive
};

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
    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, e->app->window, nullptr);
    if (surface == EGL_NO_SURFACE) { LOGW("eglCreateWindowSurface failed"); return false; }

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) { LOGW("eglCreateContext failed"); return false; }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGW("eglMakeCurrent failed");
        return false;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &e->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &e->height);

    e->display = display;
    e->surface = surface;
    e->context = context;
    e->running = true;

    LOGI("EGL GLES2 up: %dx%d, GL_VERSION=%s", e->width, e->height, glGetString(GL_VERSION));
    return true;
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
        // Block only when we have nothing to draw; otherwise pump events and render.
        int timeout = engine.running ? 0 : -1;
        while (ALooper_pollAll(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested != 0) {
                egl_term(&engine);
                return;
            }
        }
        draw_frame(&engine);
    }
}
