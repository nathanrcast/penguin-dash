// SFML-compat shim implementations for Penguin Dash / Android (port step 3, A1).
//
// STUB PHASE: every method here is a link-satisfying stub (safe defaults, no-ops)
// so the full ETR engine compiles and links into libpenguindash.so. Subsystems are
// then made real one at a time:
//   A1c  Window/Keyboard/Event  -> the native_main EGL shim + AInputEvent
//   A1d  Image/Texture/Font/Text/Sprite/RenderTarget -> stb_image/stb_truetype + Shader2D
//   A3   SoundBuffer/Sound/Music -> Oboe + stb_vorbis
// Grep TODO(A1c/A1d/A3) for what remains to implement.

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window/Context.hpp>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/keycodes.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

#include "glshader.h"      // Shader2D_* — the GLES2 2D draw path (A1d)
#include "native_bridge.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace {
// Last known pointer position (surface pixels), returned by sf::Mouse and used
// by the state manager to place clicks. Updated as pointer events are drained.
int g_pointer_x = 0;
int g_pointer_y = 0;

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
        default:                    return K::Unknown;
    }
}
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

// ---- Keyboard / Mouse / Joystick (A1c: pointer cache; A2: tilt/keys) ----
bool Keyboard::isKeyPressed(Key) { return false; }
Vector2i Mouse::getPosition() { return Vector2i(g_pointer_x, g_pointer_y); }
bool Joystick::isConnected(unsigned int) { return false; }
unsigned int Joystick::getButtonCount(unsigned int) { return 0; }
bool Joystick::hasAxis(unsigned int, Axis) { return false; }
float Joystick::getAxisPosition(unsigned int, Axis) { return 0.f; }

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

// ---- Audio (TODO A3: Oboe + stb_vorbis) ----
SoundBuffer::SoundBuffer() : m_impl(nullptr) {}
SoundBuffer::~SoundBuffer() {}
bool SoundBuffer::loadFromFile(const std::string&) { return false; }

Sound::Sound() : m_buffer(nullptr) {}
Sound::~Sound() {}
void Sound::setBuffer(const SoundBuffer& b) { m_buffer = &b; }
void Sound::play() { m_status = Playing; }
void Sound::stop() { m_status = Stopped; }

Music::Music() : m_impl(nullptr) {}
Music::~Music() {}
bool Music::openFromFile(const std::string&) { return false; }
void Music::play() { m_status = Playing; }
void Music::stop() { m_status = Stopped; }

// ---- Context::getFunction -> GLES2 entry points via EGL ----
GlFunctionPointer Context::getFunction(const char* name) {
    return reinterpret_cast<GlFunctionPointer>(eglGetProcAddress(name));
}

} // namespace sf
