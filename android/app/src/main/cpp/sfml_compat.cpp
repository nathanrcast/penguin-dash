// SFML-compat shim implementations for Penguin Dash / Android (port step 3, A1).
//
// STUB PHASE: every method here is a link-satisfying stub (safe defaults, no-ops)
// so the full ETR engine compiles and links into libpenguindash.so. Subsystems are
// then made real one at a time:
//   A1c  Window/Keyboard/Event  -> the native_main EGL shim + AInputEvent
//   A1d  Image/Texture/Font/Text/Sprite/RenderTarget -> stb_image/stb_truetype + Shader2D
//   A3   SoundBuffer/Sound -> SoundPool (pre-decoded SFX); Music -> MediaPlayer (.ogg)
// Grep TODO(A1c/A1d/A3) for what remains to implement.

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window/Context.hpp>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <android/keycodes.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "glshader.h"      // Shader2D_* — the GLES2 2D draw path (A1d)
#include "native_bridge.h"

#define LOG_TAG "PenguinDashAudio"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace {
// Last known pointer position (surface pixels), returned by sf::Mouse and used
// by the state manager to place clicks. Updated as pointer events are drained.
int g_pointer_x = 0;
int g_pointer_y = 0;
bool g_key_down[sf::Keyboard::KeyCount] = {};
bool g_joy_button_down[16] = {};
float g_joy_axis_x = 0.f;
float g_joy_axis_y = 0.f;

// ---- 2D draw helpers (A1d) ----
// The Shader2D program uses a bottom-left ortho (Setup2dScene); SFML drawables
// are top-left with y down, so we flip Y when emitting vertices.
float g_scrW = 0.f, g_scrH = 0.f;
void Begin2D() {
    unsigned w = 0, h = 0;
    pd::GetSurfaceSize(w, h);
    g_scrW = static_cast<float>(w);
    g_scrH = static_cast<float>(h);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Shader2D_Begin(g_scrW, g_scrH);
}
void End2D() { Shader2D_End(); }
inline float FlipY(float y) { return g_scrH - y; }

GLuint UploadRGBA(const unsigned char* px, int w, int h, GLuint existing,
                  bool smooth, bool repeated) {
    GLuint t = existing;
    if (!t) glGenTextures(1, &t);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    // GLES2 forbids REPEAT on non-power-of-two textures; clamp is always safe.
    GLint wrap = repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    return t;
}

// ---- Font atlas (stb_truetype), one baked bitmap per pixel size ----
constexpr int kFirstGlyph = 32;
constexpr int kGlyphCount = 96;   // ASCII 32..127
struct FontAtlas {
    GLuint tex = 0;
    int W = 0, H = 0;
    float ascent = 0.f;
    float pixelHeight = 0.f;
    stbtt_bakedchar cd[kGlyphCount];
};
struct FontData {
    std::vector<unsigned char> ttf;
    stbtt_fontinfo info;
    std::map<int, FontAtlas*> atlases;  // keyed on pixel size
};

FontAtlas* GetAtlas(FontData* fd, int pixelSize) {
    if (!fd || pixelSize <= 0) return nullptr;
    auto it = fd->atlases.find(pixelSize);
    if (it != fd->atlases.end()) return it->second;

    // Bigger sheet for large title sizes so glyphs don't overflow.
    int dim = (pixelSize > 48) ? 1024 : 512;
    std::vector<unsigned char> alpha(dim * dim, 0);
    FontAtlas* a = new FontAtlas();
    a->W = dim; a->H = dim; a->pixelHeight = static_cast<float>(pixelSize);
    stbtt_BakeFontBitmap(fd->ttf.data(), 0, static_cast<float>(pixelSize),
                         alpha.data(), dim, dim, kFirstGlyph, kGlyphCount, a->cd);

    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&fd->info, &asc, &desc, &gap);
    a->ascent = asc * stbtt_ScaleForPixelHeight(&fd->info, static_cast<float>(pixelSize));

    // Shader2D multiplies vertex colour by the sampled texel, so store rgb=255
    // and alpha=coverage: coloured text keeps its colour with anti-aliased edges.
    std::vector<unsigned char> rgba(dim * dim * 4);
    for (int i = 0; i < dim * dim; i++) {
        rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = alpha[i];
    }
    a->tex = UploadRGBA(rgba.data(), dim, dim, 0, true, false);
    fd->atlases[pixelSize] = a;
    return a;
}

// Android keycode -> the handful of sf::Keyboard keys the menus/gameplay need.
sf::Keyboard::Key MapKey(int code) {
    using K = sf::Keyboard;
    switch (code) {
        case AKEYCODE_BACK:
        case AKEYCODE_ESCAPE:       return K::Escape;
        case AKEYCODE_ENTER:
        case AKEYCODE_DPAD_CENTER:
        case AKEYCODE_BUTTON_A:     return K::Return;
        case AKEYCODE_DPAD_UP:      return K::Up;
        case AKEYCODE_DPAD_DOWN:    return K::Down;
        case AKEYCODE_DPAD_LEFT:    return K::Left;
        case AKEYCODE_DPAD_RIGHT:   return K::Right;
        case AKEYCODE_SPACE:        return K::Space;
        case AKEYCODE_A:            return K::A;
        case AKEYCODE_C:            return K::C;
        case AKEYCODE_D:            return K::D;
        case AKEYCODE_F:            return K::F;
        case AKEYCODE_H:            return K::H;
        case AKEYCODE_P:            return K::P;
        case AKEYCODE_R:            return K::R;
        case AKEYCODE_S:            return K::S;
        case AKEYCODE_T:            return K::T;
        case AKEYCODE_W:            return K::W;
        default:                    return K::Unknown;
    }
}

bool AudioFileExists(const std::string& filename) {
    FILE* f = std::fopen(filename.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// PCM WAV duration from the RIFF header (fmt byte rate + data chunk size).
// SoundPool has no is-playing query, so Sound::getStatus tracks one-shot
// playback natively as startedAt + duration; 0 on parse failure.
float WavDurationSeconds(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0.f;
    unsigned char h[12];
    if (std::fread(h, 1, 12, f) != 12 ||
        std::memcmp(h, "RIFF", 4) != 0 || std::memcmp(h + 8, "WAVE", 4) != 0) {
        std::fclose(f);
        return 0.f;
    }
    unsigned byteRate = 0, dataSize = 0;
    unsigned char ch[8];
    while (std::fread(ch, 1, 8, f) == 8) {
        unsigned sz = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((unsigned)ch[7] << 24);
        if (std::memcmp(ch, "fmt ", 4) == 0 && sz >= 16) {
            unsigned char fmt[16];
            if (std::fread(fmt, 1, 16, f) != 16) break;
            byteRate = fmt[8] | (fmt[9] << 8) | (fmt[10] << 16) | ((unsigned)fmt[11] << 24);
            if (sz > 16) std::fseek(f, sz - 16, SEEK_CUR);
        } else if (std::memcmp(ch, "data", 4) == 0) {
            dataSize = sz;
            break;
        } else {
            std::fseek(f, sz + (sz & 1), SEEK_CUR);
        }
    }
    std::fclose(f);
    if (!byteRate || !dataSize) return 0.f;
    return static_cast<float>(dataSize) / static_cast<float>(byteRate);
}

JNIEnv* GetJniEnv() {
    JavaVM* vm = pd::JavaVm();
    if (!vm) return nullptr;

    JNIEnv* env = nullptr;
    jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) return env;
    if (status == JNI_EDETACHED && vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
        return env;
    return nullptr;
}

bool ClearJavaException(JNIEnv* env, const char* where) {
    if (!env || !env->ExceptionCheck()) return false;
    LOGW("MediaPlayer exception in %s", where);
    env->ExceptionClear();
    return true;
}

struct JavaMediaPlayer {
    jobject player = nullptr;
    bool prepared = false;

    ~JavaMediaPlayer() { release(); }

    bool create(JNIEnv* env) {
        if (player) return true;
        jclass cls = env->FindClass("android/media/MediaPlayer");
        if (!cls || ClearJavaException(env, "FindClass(MediaPlayer)")) return false;
        jmethodID ctor = env->GetMethodID(cls, "<init>", "()V");
        if (!ctor || ClearJavaException(env, "MediaPlayer.<init>")) {
            env->DeleteLocalRef(cls);
            return false;
        }
        jobject local = env->NewObject(cls, ctor);
        if (!local || ClearJavaException(env, "NewObject(MediaPlayer)")) {
            env->DeleteLocalRef(cls);
            return false;
        }
        player = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        env->DeleteLocalRef(cls);
        return player != nullptr;
    }

    bool callVoid(const char* name, const char* sig, const char* where) {
        JNIEnv* env = GetJniEnv();
        if (!env || !player) return false;
        jclass cls = env->GetObjectClass(player);
        jmethodID method = env->GetMethodID(cls, name, sig);
        env->DeleteLocalRef(cls);
        if (!method || ClearJavaException(env, where)) return false;
        env->CallVoidMethod(player, method);
        return !ClearJavaException(env, where);
    }

    bool prepareFile(const std::string& filename) {
        JNIEnv* env = GetJniEnv();
        if (!env || !create(env)) return false;

        reset();
        jclass cls = env->GetObjectClass(player);
        jmethodID setDataSource = env->GetMethodID(cls, "setDataSource", "(Ljava/lang/String;)V");
        jmethodID prepare = env->GetMethodID(cls, "prepare", "()V");
        env->DeleteLocalRef(cls);
        if (!setDataSource || !prepare || ClearJavaException(env, "MediaPlayer methods"))
            return false;

        jstring path = env->NewStringUTF(filename.c_str());
        env->CallVoidMethod(player, setDataSource, path);
        env->DeleteLocalRef(path);
        if (ClearJavaException(env, "setDataSource")) return false;

        env->CallVoidMethod(player, prepare);
        prepared = !ClearJavaException(env, "prepare");
        return prepared;
    }

    bool start() {
        if (!prepared) return false;
        if (callVoid("start", "()V", "start")) return true;
        prepared = false;
        return false;
    }

    void reset() {
        if (!player) return;
        callVoid("reset", "()V", "reset");
        prepared = false;
    }

    void stop() {
        if (!player) return;
        if (isPlaying()) callVoid("stop", "()V", "stop");
        prepared = false;
    }

    void release() {
        if (!player) return;
        JNIEnv* env = GetJniEnv();
        if (env) {
            callVoid("release", "()V", "release");
            env->DeleteGlobalRef(player);
        }
        player = nullptr;
        prepared = false;
    }

    void setLooping(bool loop) {
        JNIEnv* env = GetJniEnv();
        if (!env || !player) return;
        jclass cls = env->GetObjectClass(player);
        jmethodID method = env->GetMethodID(cls, "setLooping", "(Z)V");
        env->DeleteLocalRef(cls);
        if (!method || ClearJavaException(env, "setLooping method")) return;
        env->CallVoidMethod(player, method, loop ? JNI_TRUE : JNI_FALSE);
        ClearJavaException(env, "setLooping");
    }

    void setVolume(float volume) {
        JNIEnv* env = GetJniEnv();
        if (!env || !player) return;
        jclass cls = env->GetObjectClass(player);
        jmethodID method = env->GetMethodID(cls, "setVolume", "(FF)V");
        env->DeleteLocalRef(cls);
        if (!method || ClearJavaException(env, "setVolume method")) return;
        float gain = std::max(0.f, std::min(volume / 100.f, 1.f));
        env->CallVoidMethod(player, method, gain, gain);
        ClearJavaException(env, "setVolume");
    }

    bool isPlaying() const {
        JNIEnv* env = GetJniEnv();
        if (!env || !player) return false;
        jclass cls = env->GetObjectClass(player);
        jmethodID method = env->GetMethodID(cls, "isPlaying", "()Z");
        env->DeleteLocalRef(cls);
        if (!method || ClearJavaException(env, "isPlaying method")) return false;
        jboolean playing = env->CallBooleanMethod(player, method);
        if (ClearJavaException(env, "isPlaying")) return false;
        return playing == JNI_TRUE;
    }
};

// ---- SoundPool-backed SFX (perf review P1) ----
// MediaPlayer's synchronous prepare() per play() ran a blocking codec init on
// the render thread (multi-frame hitch on every herring pickup / terrain
// change). SoundPool decodes each .wav ONCE at load time on a background
// thread; play() is a single cached-jmethodID JNI call into pre-decoded PCM.
struct SoundPoolBackend {
    jobject pool = nullptr;   // global ref, app lifetime
    jmethodID midLoad = nullptr, midPlay = nullptr, midStop = nullptr, midSetVolume = nullptr;
    bool triedInit = false;

    bool init() {
        if (pool) return true;
        if (triedInit) return false;
        triedInit = true;
        JNIEnv* env = GetJniEnv();
        if (!env) return false;
        jclass cls = env->FindClass("android/media/SoundPool");
        if (!cls || ClearJavaException(env, "FindClass(SoundPool)")) return false;
        // The (maxStreams, streamType, srcQuality) constructor is deprecated but
        // present on every API level and far simpler than the Builder via JNI.
        jmethodID ctor = env->GetMethodID(cls, "<init>", "(III)V");
        midLoad      = env->GetMethodID(cls, "load", "(Ljava/lang/String;I)I");
        midPlay      = env->GetMethodID(cls, "play", "(IFFIIF)I");
        midStop      = env->GetMethodID(cls, "stop", "(I)V");
        midSetVolume = env->GetMethodID(cls, "setVolume", "(IFF)V");
        if (!ctor || !midLoad || !midPlay || !midStop || !midSetVolume ||
            ClearJavaException(env, "SoundPool methods")) {
            env->DeleteLocalRef(cls);
            return false;
        }
        jobject local = env->NewObject(cls, ctor, 8, 3 /*AudioManager.STREAM_MUSIC*/, 0);
        env->DeleteLocalRef(cls);
        if (!local || ClearJavaException(env, "new SoundPool")) return false;
        pool = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        return pool != nullptr;
    }

    // Kicks off an async decode; the returned sample id is usable once decoding
    // finishes (play() on a not-yet-ready sample returns stream id 0 = no-op).
    int load(const std::string& path) {
        if (!init()) return 0;
        JNIEnv* env = GetJniEnv();
        if (!env) return 0;
        jstring jpath = env->NewStringUTF(path.c_str());
        jint id = env->CallIntMethod(pool, midLoad, jpath, 1);
        env->DeleteLocalRef(jpath);
        return ClearJavaException(env, "SoundPool.load") ? 0 : id;
    }

    int play(int soundId, float gain, bool loop) {
        if (!pool || soundId <= 0) return 0;
        JNIEnv* env = GetJniEnv();
        if (!env) return 0;
        jint stream = env->CallIntMethod(pool, midPlay, (jint)soundId,
                                         (jfloat)gain, (jfloat)gain,
                                         (jint)1, (jint)(loop ? -1 : 0), (jfloat)1.0f);
        return ClearJavaException(env, "SoundPool.play") ? 0 : stream;
    }

    void stop(int streamId) {
        if (!pool || streamId == 0) return;
        JNIEnv* env = GetJniEnv();
        if (!env) return;
        env->CallVoidMethod(pool, midStop, (jint)streamId);
        ClearJavaException(env, "SoundPool.stop");
    }

    void setVolume(int streamId, float gain) {
        if (!pool || streamId == 0) return;
        JNIEnv* env = GetJniEnv();
        if (!env) return;
        env->CallVoidMethod(pool, midSetVolume, (jint)streamId, (jfloat)gain, (jfloat)gain);
        ClearJavaException(env, "SoundPool.setVolume");
    }
};
SoundPoolBackend g_soundPool;

float GainFromVolume(float volume) {
    return std::max(0.f, std::min(volume / 100.f, 1.f));
}

struct AudioFile {
    std::string filename;
    bool exists = false;
    int soundId = 0;          // SoundPool sample id (0 = not loaded)
    float durationSec = 0.f;  // one-shot length for native play-state tracking
};

struct SoundData {
    std::string filename;
    bool exists = false;
    bool missingLogged = false;
    int soundId = 0;
    float durationSec = 0.f;
    int streamId = 0;         // last started SoundPool stream (0 = none)
    bool looping = false;
    std::chrono::steady_clock::time_point startedAt{};
};

struct MusicData {
    std::string filename;
    bool exists = false;
    bool missingLogged = false;
    JavaMediaPlayer player;
};
} // namespace

namespace sf {

// ---- Graphics value-type statics ----
const Color Color::Black(0, 0, 0);
const Color Color::White(255, 255, 255);
const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::Yellow(255, 255, 0);
const Color Color::Magenta(255, 0, 255);
const Color Color::Cyan(0, 255, 255);
const Color Color::Transparent(0, 0, 0, 0);
const BlendMode BlendAlpha = {};
const RenderStates RenderStates::Default = RenderStates();

// ---- Keyboard / Mouse / Joystick (A1c: pointer cache; A2: tilt/buttons) ----
bool Keyboard::isKeyPressed(Key key) {
    return key >= 0 && key < KeyCount && g_key_down[key];
}
Vector2i Mouse::getPosition() { return Vector2i(g_pointer_x, g_pointer_y); }
bool Joystick::isConnected(unsigned int joystick) { return joystick == 0; }
unsigned int Joystick::getButtonCount(unsigned int joystick) { return joystick == 0 ? 4 : 0; }
bool Joystick::hasAxis(unsigned int joystick, Axis axis) {
    return joystick == 0 && (axis == X || axis == Y);
}
float Joystick::getAxisPosition(unsigned int joystick, Axis axis) {
    if (joystick != 0) return 0.f;
    if (axis == X) return g_joy_axis_x;
    if (axis == Y) return g_joy_axis_y;
    return 0.f;
}

// ---- VideoMode / Window (A1c: bridged to native_main's EGL surface) ----
VideoMode VideoMode::getDesktopMode() {
    unsigned w = 0, h = 0;
    pd::GetSurfaceSize(w, h);
    if (w == 0 || h == 0) { w = 1920; h = 1080; }
    return VideoMode(w, h, 32);
}

Window::Window() : m_size(0, 0), m_open(false) {}
Window::~Window() {}
void Window::create(VideoMode, const String&, Uint32, const ContextSettings&) {
    // The window already exists (the EGL surface). Adopt its real pixel size and
    // ignore the requested VideoMode.
    unsigned w = 0, h = 0;
    pd::GetSurfaceSize(w, h);
    m_size = Vector2u(w, h);
    m_open = true;
}
void Window::close() { m_open = false; }
bool Window::isOpen() const { return m_open && !pd::ShouldClose(); }
bool Window::pollEvent(Event& event) {
    pd::InputEvent ie;
    if (!pd::PollInput(ie)) return false;
    switch (ie.kind) {
        case pd::EvKind::PointerDown:
            g_pointer_x = ie.x; g_pointer_y = ie.y;
            event.type = Event::MouseButtonPressed;
            event.mouseButton.button = Mouse::Left;
            event.mouseButton.x = ie.x; event.mouseButton.y = ie.y;
            break;
        case pd::EvKind::PointerUp:
            g_pointer_x = ie.x; g_pointer_y = ie.y;
            event.type = Event::MouseButtonReleased;
            event.mouseButton.button = Mouse::Left;
            event.mouseButton.x = ie.x; event.mouseButton.y = ie.y;
            break;
        case pd::EvKind::PointerMove:
            g_pointer_x = ie.x; g_pointer_y = ie.y;
            event.type = Event::MouseMoved;
            event.mouseMove.x = ie.x; event.mouseMove.y = ie.y;
            break;
        case pd::EvKind::KeyDown:
        case pd::EvKind::KeyUp:
            event.type = (ie.kind == pd::EvKind::KeyDown) ? Event::KeyPressed
                                                          : Event::KeyReleased;
            event.key.code = MapKey(ie.keycode);
            event.key.alt = event.key.control = event.key.shift = event.key.system = false;
            if (event.key.code >= 0 && event.key.code < Keyboard::KeyCount)
                g_key_down[event.key.code] = (ie.kind == pd::EvKind::KeyDown);
			break;
        case pd::EvKind::JoystickMove:
            event.type = Event::JoystickMoved;
            event.joystickMove.joystickId = ie.joystickId;
            event.joystickMove.axis = (ie.joystickAxis == 0) ? Joystick::X : Joystick::Y;
            event.joystickMove.position = ie.joystickPosition;
            if (ie.joystickId == 0) {
                if (event.joystickMove.axis == Joystick::X)
                    g_joy_axis_x = ie.joystickPosition;
                else if (event.joystickMove.axis == Joystick::Y)
                    g_joy_axis_y = ie.joystickPosition;
            }
            break;
        case pd::EvKind::JoystickButtonDown:
        case pd::EvKind::JoystickButtonUp:
            event.type = (ie.kind == pd::EvKind::JoystickButtonDown) ? Event::JoystickButtonPressed
                                                                      : Event::JoystickButtonReleased;
            event.joystickButton.joystickId = ie.joystickId;
            event.joystickButton.button = ie.joystickButton;
            if (ie.joystickId == 0 && ie.joystickButton < 16)
                g_joy_button_down[ie.joystickButton] = (ie.kind == pd::EvKind::JoystickButtonDown);
            break;
    }
    return true;
}
void Window::display() { pd::PumpEvents(); pd::SwapBuffers(); }
void Window::setVerticalSyncEnabled(bool) {}
void Window::setFramerateLimit(unsigned int) {}
void Window::setMouseCursorVisible(bool) {}
void Window::setKeyRepeatEnabled(bool) {}
void Window::setTitle(const String&) {}
void Window::setActive(bool) {}
Vector2u Window::getSize() const {
    unsigned w = 0, h = 0;
    pd::GetSurfaceSize(w, h);
    return (w && h) ? Vector2u(w, h) : m_size;
}

// ---- Image (A1d: stb_image, forced RGBA) ----
Image::Image() : m_w(0), m_h(0) {}
Image::~Image() {}
bool Image::loadFromFile(const std::string& filename) {
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load(filename.c_str(), &w, &h, &n, 4);
    if (!data) return false;
    m_w = static_cast<unsigned>(w);
    m_h = static_cast<unsigned>(h);
    m_pixels.assign(data, data + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}
void Image::create(unsigned int w, unsigned int h, const Uint8* pixels) {
    m_w = w; m_h = h;
    m_pixels.assign(w * h * 4, 0);
    if (pixels) m_pixels.assign(pixels, pixels + w * h * 4);
}
Vector2u Image::getSize() const { return Vector2u(m_w, m_h); }
const Uint8* Image::getPixelsPtr() const { return m_pixels.empty() ? nullptr : m_pixels.data(); }
void Image::flipVertically() {
    if (m_pixels.empty() || m_h < 2) return;
    const std::size_t row = static_cast<std::size_t>(m_w) * 4;
    for (unsigned y = 0; y < m_h / 2; y++)
        std::swap_ranges(m_pixels.begin() + y * row,
                         m_pixels.begin() + (y + 1) * row,
                         m_pixels.begin() + (m_h - 1 - y) * row);
}

// ---- Texture (A1d: GL texture upload) ----
Texture::Texture() : m_texture(0), m_w(0), m_h(0), m_smooth(false), m_repeated(false) {}
Texture::~Texture() { if (m_texture) glDeleteTextures(1, &m_texture); }
bool Texture::loadFromImage(const Image& image, const IntRect&) {
    Vector2u s = image.getSize();
    if (!s.x || !s.y || !image.getPixelsPtr()) return false;
    m_w = s.x; m_h = s.y;
    m_texture = UploadRGBA(image.getPixelsPtr(), m_w, m_h, m_texture, m_smooth, m_repeated);
    return true;
}
bool Texture::loadFromFile(const std::string& filename, const IntRect& area) {
    Image img;
    if (!img.loadFromFile(filename)) return false;
    return loadFromImage(img, area);
}
void Texture::update(const Image& image) { loadFromImage(image, IntRect()); }
Vector2u Texture::getSize() const { return Vector2u(m_w, m_h); }
void Texture::setSmooth(bool s) { m_smooth = s; }
void Texture::setRepeated(bool r) { m_repeated = r; }
void Texture::bind(const Texture* texture) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture ? texture->m_texture : 0);
}

// ---- Sprite (TODO A1d: Shader2D quad) ----
Sprite::Sprite() : m_texture(nullptr), m_color(Color::White) {}
Sprite::Sprite(const Texture& t) : m_texture(&t), m_color(Color::White) {
    Vector2u s = t.getSize();
    m_rect = IntRect(0, 0, static_cast<int>(s.x), static_cast<int>(s.y));
}
Sprite::Sprite(const Texture& t, const IntRect& r) : m_texture(&t), m_rect(r), m_color(Color::White) {}
void Sprite::setTexture(const Texture& t, bool resetRect) {
    m_texture = &t;
    if (resetRect) { Vector2u s = t.getSize(); m_rect = IntRect(0, 0, s.x, s.y); }
}
void Sprite::setTextureRect(const IntRect& r) { m_rect = r; }
void Sprite::setColor(const Color& c) { m_color = c; }
FloatRect Sprite::getLocalBounds() const {
    return FloatRect(0.f, 0.f, static_cast<float>(m_rect.width), static_cast<float>(m_rect.height));
}
FloatRect Sprite::getGlobalBounds() const {
    return FloatRect(m_pos.x, m_pos.y, m_rect.width * m_scale.x, m_rect.height * m_scale.y);
}
void Sprite::draw(RenderTarget&, const RenderStates& states) const {
    const Texture* tex = m_texture ? m_texture : states.texture;
    if (!tex) return;
    Vector2u ts = tex->getSize();
    if (!ts.x || !ts.y) return;

    // Local quad corners (top-left origin), transformed by origin/scale/rotation
    // /position into surface space, then Y-flipped for the bottom-left ortho.
    const float lx[4] = {0.f, (float)m_rect.width, (float)m_rect.width, 0.f};
    const float ly[4] = {0.f, 0.f, (float)m_rect.height, (float)m_rect.height};
    const float rad = m_rotation * 3.14159265f / 180.f;
    const float cs = std::cos(rad), sn = std::sin(rad);
    float pos[8];
    for (int i = 0; i < 4; i++) {
        float dx = (lx[i] - m_origin.x) * m_scale.x;
        float dy = (ly[i] - m_origin.y) * m_scale.y;
        float sx = m_pos.x + dx * cs - dy * sn;
        float sy = m_pos.y + dx * sn + dy * cs;
        pos[i * 2] = sx;
        pos[i * 2 + 1] = FlipY(sy);
    }
    const float u0 = (float)m_rect.left / ts.x;
    const float v0 = (float)m_rect.top / ts.y;
    const float u1 = (float)(m_rect.left + m_rect.width) / ts.x;
    const float v1 = (float)(m_rect.top + m_rect.height) / ts.y;
    const float uv[8] = {u0, v0, u1, v0, u1, v1, u0, v1};

    Begin2D();
    Texture::bind(tex);
    Shader2D_DrawArrays(GL_TRIANGLE_FAN, pos, uv, 4, true, m_color);
    End2D();
}

// ---- RectangleShape ----
RectangleShape::RectangleShape(const Vector2f& size) : m_rectSize(size), m_fill(Color::White), m_outline(Color::Black) {}
void RectangleShape::setSize(const Vector2f& size) { m_rectSize = size; }
void RectangleShape::setFillColor(const Color& c) { m_fill = c; }
void RectangleShape::setOutlineColor(const Color& c) { m_outline = c; }
void RectangleShape::setOutlineThickness(float t) { m_outlineThickness = t; }
FloatRect RectangleShape::getGlobalBounds() const {
    return FloatRect(m_pos.x, m_pos.y, m_rectSize.x * m_scale.x, m_rectSize.y * m_scale.y);
}
void RectangleShape::draw(RenderTarget&, const RenderStates&) const {
    const float rad = m_rotation * 3.14159265f / 180.f;
    const float cs = std::cos(rad), sn = std::sin(rad);
    // Emit one transformed, Y-flipped solid quad from a local rect.
    auto quad = [&](float x0, float y0, float x1, float y1, const Color& col) {
        const float lx[4] = {x0, x1, x1, x0};
        const float ly[4] = {y0, y0, y1, y1};
        float pos[8];
        for (int i = 0; i < 4; i++) {
            float dx = (lx[i] - m_origin.x) * m_scale.x;
            float dy = (ly[i] - m_origin.y) * m_scale.y;
            pos[i * 2] = m_pos.x + dx * cs - dy * sn;
            pos[i * 2 + 1] = FlipY(m_pos.y + dx * sn + dy * cs);
        }
        Shader2D_DrawArrays(GL_TRIANGLE_FAN, pos, nullptr, 4, false, col);
    };
    Begin2D();
    // Outline drawn as an expanded quad behind the fill (opaque UI frames).
    if (m_outlineThickness > 0.f) {
        float t = m_outlineThickness;
        quad(-t, -t, m_rectSize.x + t, m_rectSize.y + t, m_outline);
    }
    quad(0.f, 0.f, m_rectSize.x, m_rectSize.y, m_fill);
    End2D();
}

// ---- VertexArray (best-effort; used only by the credits scroll) ----
// Shader2D takes one constant colour, so per-vertex colour gradients aren't
// reproduced — positions + texcoords are, which is enough to not crash/misplace.
void VertexArray::draw(RenderTarget&, const RenderStates& states) const {
    if (m_verts.empty()) return;
    const bool textured = states.texture != nullptr;
    Vector2u ts = textured ? states.texture->getSize() : Vector2u(1, 1);
    if (textured && (!ts.x || !ts.y)) return;

    std::vector<float> pos, uv;
    auto emit = [&](const Vertex& v) {
        pos.push_back(v.position.x);
        pos.push_back(FlipY(v.position.y));
        if (textured) {
            uv.push_back(v.texCoords.x / ts.x);
            uv.push_back(v.texCoords.y / ts.y);
        }
    };
    if (m_type == Quads) {
        for (std::size_t i = 0; i + 3 < m_verts.size(); i += 4) {
            const std::size_t idx[6] = {i, i + 1, i + 2, i, i + 2, i + 3};
            for (std::size_t k : idx) emit(m_verts[k]);
        }
    } else {
        for (const Vertex& v : m_verts) emit(v);
    }
    if (pos.empty()) return;

    unsigned mode = (m_type == Quads) ? GL_TRIANGLES
                  : (m_type == Triangles) ? GL_TRIANGLES
                  : (m_type == TriangleStrip) ? GL_TRIANGLE_STRIP
                  : (m_type == TriangleFan) ? GL_TRIANGLE_FAN
                  : (m_type == Lines) ? GL_LINES
                  : (m_type == LineStrip) ? GL_LINE_STRIP : GL_POINTS;

    Begin2D();
    if (textured) Texture::bind(states.texture);
    Shader2D_DrawArrays(mode, pos.data(), textured ? uv.data() : nullptr,
                        static_cast<int>(pos.size() / 2), textured, m_verts[0].color);
    End2D();
}

// ---- Font / Text (A1d: stb_truetype atlas + Shader2D) ----
Font::Font() : m_impl(nullptr) {}
Font::~Font() {
    FontData* fd = static_cast<FontData*>(m_impl);
    if (!fd) return;
    for (auto& kv : fd->atlases) {
        if (kv.second->tex) glDeleteTextures(1, &kv.second->tex);
        delete kv.second;
    }
    delete fd;
}
bool Font::loadFromFile(const std::string& filename) {
    FILE* f = std::fopen(filename.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return false; }
    FontData* fd = new FontData();
    fd->ttf.resize(static_cast<std::size_t>(sz));
    std::size_t rd = std::fread(fd->ttf.data(), 1, static_cast<std::size_t>(sz), f);
    std::fclose(f);
    if (rd != static_cast<std::size_t>(sz) ||
        !stbtt_InitFont(&fd->info, fd->ttf.data(),
                        stbtt_GetFontOffsetForIndex(fd->ttf.data(), 0))) {
        delete fd;
        return false;
    }
    m_impl = fd;
    return true;
}

Text::Text() : m_font(nullptr), m_charSize(30), m_fill(Color::White), m_outline(Color::Black), m_outlineThickness(0.f) {}
Text::Text(const String& s, const Font& f, unsigned int size)
    : m_string(s), m_font(&f), m_charSize(size), m_fill(Color::White), m_outline(Color::Black), m_outlineThickness(0.f) {}
void Text::setString(const String& s) { m_string = s; }
void Text::setFont(const Font& f) { m_font = &f; }
void Text::setCharacterSize(unsigned int size) { m_charSize = size; }
void Text::setFillColor(const Color& c) { m_fill = c; }
void Text::setOutlineColor(const Color& c) { m_outline = c; }
void Text::setOutlineThickness(float t) { m_outlineThickness = t; }

FloatRect Text::getLocalBounds() const {
    FontData* fd = m_font ? static_cast<FontData*>(m_font->impl()) : nullptr;
    FontAtlas* a = GetAtlas(fd, static_cast<int>(m_charSize));
    if (!a)
        return FloatRect(0.f, 0.f, m_string.getSize() * m_charSize * 0.5f, (float)m_charSize);
    float x = 0.f, y = 0.f, maxx = 0.f;
    const std::string& s = m_string.str();
    for (char ch : s) {
        if (ch < kFirstGlyph || ch >= kFirstGlyph + kGlyphCount) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(a->cd, a->W, a->H, ch - kFirstGlyph, &x, &y, &q, 1);
        if (q.x1 > maxx) maxx = q.x1;
    }
    return FloatRect(0.f, 0.f, maxx, (float)m_charSize);
}
Vector2f Text::findCharacterPos(std::size_t index) const {
    FontData* fd = m_font ? static_cast<FontData*>(m_font->impl()) : nullptr;
    FontAtlas* a = GetAtlas(fd, static_cast<int>(m_charSize));
    if (!a) return Vector2f(m_pos.x + index * m_charSize * 0.5f, m_pos.y);
    float x = 0.f, y = 0.f;
    const std::string& s = m_string.str();
    for (std::size_t i = 0; i < index && i < s.size(); i++) {
        char ch = s[i];
        if (ch < kFirstGlyph || ch >= kFirstGlyph + kGlyphCount) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(a->cd, a->W, a->H, ch - kFirstGlyph, &x, &y, &q, 1);
    }
    return Vector2f(m_pos.x + x * m_scale.x, m_pos.y);
}
FloatRect Text::getGlobalBounds() const {
    FloatRect b = getLocalBounds();
    return FloatRect(m_pos.x, m_pos.y, b.width * m_scale.x, b.height * m_scale.y);
}
void Text::draw(RenderTarget&, const RenderStates&) const {
    FontData* fd = m_font ? static_cast<FontData*>(m_font->impl()) : nullptr;
    FontAtlas* a = GetAtlas(fd, static_cast<int>(m_charSize));
    if (!a) return;
    const std::string& s = m_string.str();
    if (s.empty()) return;

    std::vector<float> pos, uv;
    pos.reserve(s.size() * 12);
    uv.reserve(s.size() * 12);
    const float startX = 0.f;
    float x = startX, y = a->ascent;   // pen at baseline; text top sits at m_pos.y
    auto vtx = [&](float px, float py, float u, float v) {
        // local (top-left, baseline-relative) -> scaled about origin -> surface
        float sx = m_pos.x + (px - m_origin.x) * m_scale.x;
        float sy = m_pos.y + (py - m_origin.y) * m_scale.y;
        pos.push_back(sx); pos.push_back(FlipY(sy));
        uv.push_back(u); uv.push_back(v);
    };
    for (char ch : s) {
        if (ch == '\n') { x = startX; y += a->pixelHeight; continue; }
        if (ch < kFirstGlyph || ch >= kFirstGlyph + kGlyphCount) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(a->cd, a->W, a->H, ch - kFirstGlyph, &x, &y, &q, 1);
        // two triangles per glyph
        vtx(q.x0, q.y0, q.s0, q.t0); vtx(q.x1, q.y0, q.s1, q.t0); vtx(q.x1, q.y1, q.s1, q.t1);
        vtx(q.x0, q.y0, q.s0, q.t0); vtx(q.x1, q.y1, q.s1, q.t1); vtx(q.x0, q.y1, q.s0, q.t1);
    }
    if (pos.empty()) return;

    Begin2D();
    glBindTexture(GL_TEXTURE_2D, a->tex);
    Shader2D_DrawArrays(GL_TRIANGLES, pos.data(), uv.data(),
                        static_cast<int>(pos.size() / 2), true, m_fill);
    End2D();
}

// ---- RenderTarget / RenderWindow / RenderTexture (A1c; 2D draw = A1d) ----
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states) { drawable.draw(*this, states); }
void RenderTarget::draw(const Vertex*, std::size_t, PrimitiveType, const RenderStates&) {}
void RenderTarget::clear(const Color& c) {
    glClearColor(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
    glDepthMask(GL_TRUE);   // depth writes may be off from the last render mode
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}
void RenderTarget::pushGLStates() {}
void RenderTarget::popGLStates() {}

RenderWindow::RenderWindow() {}
RenderWindow::~RenderWindow() {}
Vector2u RenderWindow::getSize() const {
    unsigned w = 0, h = 0;
    pd::GetSurfaceSize(w, h);
    return (w && h) ? Vector2u(w, h) : m_size;
}
void RenderWindow::display() { pd::PumpEvents(); pd::SwapBuffers(); }
bool RenderWindow::setActive(bool) { return true; }

RenderTexture::RenderTexture() : m_fbo(0), m_w(0), m_h(0) {}
RenderTexture::~RenderTexture() {}
bool RenderTexture::create(unsigned int w, unsigned int h, bool) { m_w = w; m_h = h; return true; }
void RenderTexture::display() {}
const Texture& RenderTexture::getTexture() const { return m_texture; }
void RenderTexture::setSmooth(bool) {}
Vector2u RenderTexture::getSize() const { return Vector2u(m_w, m_h); }
bool RenderTexture::setActive(bool) { return true; }

// ---- Audio ----
// SFX (SoundBuffer/Sound): SoundPool — pre-decoded at load, hitch-free play,
// zero per-frame JNI (play-state is tracked natively; the engine polls
// getStatus every frame for the looping terrain sound and to rate-limit
// repeat triggers like tree_hit).
// Music: Android MediaPlayer via JNI (streamed .ogg, state-transition only).
// load/open still report success if an audio file is absent so the engine's
// music/theme bookkeeping stays valid.
SoundBuffer::SoundBuffer() : m_impl(nullptr) {}
SoundBuffer::~SoundBuffer() { delete static_cast<AudioFile*>(m_impl); }
bool SoundBuffer::loadFromFile(const std::string& filename) {
    delete static_cast<AudioFile*>(m_impl);
    AudioFile* file = new AudioFile();
    file->filename = filename;
    file->exists = AudioFileExists(filename);
    if (file->exists) {
        file->soundId = g_soundPool.load(filename);   // async decode, no hitch
        file->durationSec = WavDurationSeconds(filename);
        if (file->durationSec <= 0.f) file->durationSec = 0.5f;
    } else {
        LOGW("sound file missing, keeping silent placeholder: %s", filename.c_str());
    }
    m_impl = file;
    return true;
}

Sound::Sound() : m_buffer(nullptr), m_impl(new SoundData()) {}
Sound::~Sound() {
    SoundData* data = static_cast<SoundData*>(m_impl);
    if (data && data->looping) g_soundPool.stop(data->streamId);
    delete data;
}
void Sound::setBuffer(const SoundBuffer& b) {
    m_buffer = &b;
    SoundData* data = static_cast<SoundData*>(m_impl);
    AudioFile* file = static_cast<AudioFile*>(b.impl());
    if (!data || !file) return;
    data->filename = file->filename;
    data->exists = file->exists;
    data->soundId = file->soundId;
    data->durationSec = file->durationSec;
    data->missingLogged = false;
    data->streamId = 0;
    data->looping = false;
}
void Sound::setVolume(float volume) {
    SoundSource::setVolume(volume);
    SoundData* data = static_cast<SoundData*>(m_impl);
    if (data && data->streamId)
        g_soundPool.setVolume(data->streamId, GainFromVolume(volume));
}
void Sound::play() {
    SoundData* data = static_cast<SoundData*>(m_impl);
    if (!data || data->filename.empty()) { m_status = Stopped; return; }
    if (!data->exists) {
        if (!data->missingLogged) {
            LOGW("sound playback skipped, file missing: %s", data->filename.c_str());
            data->missingLogged = true;
        }
        m_status = Stopped;
        return;
    }
    if (m_loop && data->looping && data->streamId) { m_status = Playing; return; }
    int stream = g_soundPool.play(data->soundId, GainFromVolume(m_volume), m_loop);
    if (stream == 0) { m_status = Stopped; return; }  // sample still decoding; caller retries
    data->streamId = stream;
    data->looping = m_loop;
    data->startedAt = std::chrono::steady_clock::now();
    m_status = Playing;
}
void Sound::stop() {
    SoundData* data = static_cast<SoundData*>(m_impl);
    if (data && data->streamId) {
        g_soundPool.stop(data->streamId);
        data->streamId = 0;
        data->looping = false;
    }
    m_status = Stopped;
}
SoundSource::Status Sound::getStatus() const {
    // Native tracking, no JNI: SoundPool cannot be queried for playback state.
    const SoundData* data = static_cast<const SoundData*>(m_impl);
    if (!data || data->streamId == 0) return Stopped;
    if (data->looping) return Playing;
    float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - data->startedAt).count();
    return elapsed < data->durationSec ? Playing : Stopped;
}

Music::Music() : m_impl(nullptr) {}
Music::~Music() { delete static_cast<MusicData*>(m_impl); }
bool Music::openFromFile(const std::string& filename) {
    delete static_cast<MusicData*>(m_impl);
    MusicData* data = new MusicData();
    data->filename = filename;
    data->exists = AudioFileExists(filename);
    if (!data->exists)
        LOGW("music file missing, keeping silent placeholder: %s", filename.c_str());
    m_impl = data;
    return true;
}
void Music::setVolume(float volume) {
    SoundSource::setVolume(volume);
    MusicData* data = static_cast<MusicData*>(m_impl);
    if (data) data->player.setVolume(volume);
}
void Music::play() {
    MusicData* data = static_cast<MusicData*>(m_impl);
    if (!data || data->filename.empty()) { m_status = Stopped; return; }
    if (!data->exists) {
        if (!data->missingLogged) {
            LOGW("music playback skipped, file missing: %s", data->filename.c_str());
            data->missingLogged = true;
        }
        m_status = Stopped;
        return;
    }
    if (!data->player.prepareFile(data->filename)) { m_status = Stopped; return; }
    data->player.setLooping(m_loop);
    data->player.setVolume(m_volume);
    m_status = data->player.start() ? Playing : Stopped;
}
void Music::stop() {
    MusicData* data = static_cast<MusicData*>(m_impl);
    if (data) data->player.stop();
    m_status = Stopped;
}
SoundSource::Status Music::getStatus() const {
    MusicData* data = static_cast<MusicData*>(m_impl);
    return (data && data->player.isPlaying()) ? Playing : Stopped;
}

// ---- Context::getFunction -> GLES2 entry points via EGL ----
GlFunctionPointer Context::getFunction(const char* name) {
    return reinterpret_cast<GlFunctionPointer>(eglGetProcAddress(name));
}

} // namespace sf
