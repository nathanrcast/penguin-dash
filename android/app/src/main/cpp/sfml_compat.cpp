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

// ---- Keyboard / Mouse / Joystick (TODO A1c/A2) ----
bool Keyboard::isKeyPressed(Key) { return false; }
Vector2i Mouse::getPosition() { return Vector2i(0, 0); }
bool Joystick::isConnected(unsigned int) { return false; }
unsigned int Joystick::getButtonCount(unsigned int) { return 0; }
bool Joystick::hasAxis(unsigned int, Axis) { return false; }
float Joystick::getAxisPosition(unsigned int, Axis) { return 0.f; }

// ---- VideoMode / Window (TODO A1c: bridge to native_main EGL surface) ----
VideoMode VideoMode::getDesktopMode() { return VideoMode(1920, 1080, 32); }

Window::Window() : m_size(0, 0), m_open(false) {}
Window::~Window() {}
void Window::create(VideoMode mode, const String&, Uint32, const ContextSettings&) {
    m_size = Vector2u(mode.width, mode.height);
    m_open = true;
}
void Window::close() { m_open = false; }
bool Window::isOpen() const { return m_open; }
bool Window::pollEvent(Event&) { return false; }
void Window::display() {}
void Window::setVerticalSyncEnabled(bool) {}
void Window::setFramerateLimit(unsigned int) {}
void Window::setMouseCursorVisible(bool) {}
void Window::setTitle(const String&) {}
void Window::setActive(bool) {}
Vector2u Window::getSize() const { return m_size; }

// ---- Image (TODO A1d: stb_image) ----
Image::Image() : m_w(0), m_h(0) {}
Image::~Image() {}
bool Image::loadFromFile(const std::string&) { return false; }
void Image::create(unsigned int w, unsigned int h, const Uint8* pixels) {
    m_w = w; m_h = h;
    m_pixels.assign(w * h * 4, 0);
    if (pixels) m_pixels.assign(pixels, pixels + w * h * 4);
}
Vector2u Image::getSize() const { return Vector2u(m_w, m_h); }
const Uint8* Image::getPixelsPtr() const { return m_pixels.empty() ? nullptr : m_pixels.data(); }
void Image::flipVertically() {}

// ---- Texture (TODO A1d: GL texture upload) ----
Texture::Texture() : m_texture(0), m_w(0), m_h(0), m_smooth(false), m_repeated(false) {}
Texture::~Texture() {}
bool Texture::loadFromFile(const std::string&, const IntRect&) { return false; }
bool Texture::loadFromImage(const Image&, const IntRect&) { return false; }
void Texture::update(const Image&) {}
Vector2u Texture::getSize() const { return Vector2u(m_w, m_h); }
void Texture::setSmooth(bool s) { m_smooth = s; }
void Texture::setRepeated(bool r) { m_repeated = r; }
void Texture::bind(const Texture*) {}

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
void Sprite::draw(RenderTarget&, const RenderStates&) const {}

// ---- RectangleShape ----
RectangleShape::RectangleShape(const Vector2f& size) : m_rectSize(size), m_fill(Color::White), m_outline(Color::Black) {}
void RectangleShape::setSize(const Vector2f& size) { m_rectSize = size; }
void RectangleShape::setFillColor(const Color& c) { m_fill = c; }
void RectangleShape::setOutlineColor(const Color& c) { m_outline = c; }
void RectangleShape::setOutlineThickness(float t) { m_outlineThickness = t; }
FloatRect RectangleShape::getGlobalBounds() const {
    return FloatRect(m_pos.x, m_pos.y, m_rectSize.x * m_scale.x, m_rectSize.y * m_scale.y);
}
void RectangleShape::draw(RenderTarget&, const RenderStates&) const {}

// ---- VertexArray ----
void VertexArray::draw(RenderTarget&, const RenderStates&) const {}

// ---- Font / Text (TODO A1d: stb_truetype atlas + Shader2D) ----
Font::Font() : m_impl(nullptr) {}
Font::~Font() {}
bool Font::loadFromFile(const std::string&) { return false; }

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
    // Rough metric until the real atlas lands (A1d): ~0.5em advance, 1em height.
    return FloatRect(0.f, 0.f, m_string.getSize() * m_charSize * 0.5f, static_cast<float>(m_charSize));
}
Vector2f Text::findCharacterPos(std::size_t index) const {
    // Rough: uniform advance (A1d replaces with real glyph metrics).
    return Vector2f(m_pos.x + index * m_charSize * 0.5f, m_pos.y);
}
FloatRect Text::getGlobalBounds() const {
    FloatRect b = getLocalBounds();
    return FloatRect(m_pos.x, m_pos.y, b.width * m_scale.x, b.height * m_scale.y);
}
void Text::draw(RenderTarget&, const RenderStates&) const {}

// ---- RenderTarget / RenderWindow / RenderTexture (TODO A1c/A1d) ----
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states) { drawable.draw(*this, states); }
void RenderTarget::draw(const Vertex*, std::size_t, PrimitiveType, const RenderStates&) {}
void RenderTarget::clear(const Color&) {}
void RenderTarget::pushGLStates() {}
void RenderTarget::popGLStates() {}

RenderWindow::RenderWindow() {}
RenderWindow::~RenderWindow() {}
Vector2u RenderWindow::getSize() const { return m_size; }
void RenderWindow::display() {}
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
