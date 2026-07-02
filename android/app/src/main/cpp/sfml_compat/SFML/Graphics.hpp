// SFML-compat shim — <SFML/Graphics.hpp> equivalent for Penguin Dash / Android (A1).
// Color/Rect/Vertex and the Transformable state are real inline; Texture/Image/
// Font/Text/Sprite/RenderTexture *loading and drawing* are declared with ETR's exact
// call surface and implemented in sfml_compat.cpp — stubbed first (engine links),
// then made real on stb_image/stb_truetype + the GLES2 Shader2D path in A1d.
#ifndef PD_SFML_COMPAT_GRAPHICS_HPP
#define PD_SFML_COMPAT_GRAPHICS_HPP

#include "Window.hpp"
#include <vector>

namespace sf {

// ---- Color ----
class Color {
public:
    Uint8 r, g, b, a;
    Color() : r(0), g(0), b(0), a(255) {}
    Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255) : r(red), g(green), b(blue), a(alpha) {}
    explicit Color(Uint32 c) : r((c >> 24) & 0xff), g((c >> 16) & 0xff), b((c >> 8) & 0xff), a(c & 0xff) {}
    Uint32 toInteger() const { return (r << 24) | (g << 16) | (b << 8) | a; }
    static const Color Black, White, Red, Green, Blue, Yellow, Magenta, Cyan, Transparent;
};
inline bool operator==(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
inline bool operator!=(const Color& a, const Color& b) { return !(a == b); }

// ---- Rect ----
template <typename T>
class Rect {
public:
    T left, top, width, height;
    Rect() : left(0), top(0), width(0), height(0) {}
    Rect(T l, T t, T w, T h) : left(l), top(t), width(w), height(h) {}
    Vector2<T> getSize() const { return Vector2<T>(width, height); }
    Vector2<T> getPosition() const { return Vector2<T>(left, top); }
};
typedef Rect<int>   IntRect;
typedef Rect<float> FloatRect;

// ---- Vertex / primitives ----
struct Vertex {
    Vector2f position;
    Color    color;
    Vector2f texCoords;
    Vertex() {}
    Vertex(const Vector2f& p) : position(p) {}
    Vertex(const Vector2f& p, const Color& c) : position(p), color(c) {}
    Vertex(const Vector2f& p, const Color& c, const Vector2f& t) : position(p), color(c), texCoords(t) {}
    Vertex(const Vector2f& p, const Vector2f& t) : position(p), texCoords(t) {}
};
enum PrimitiveType { Points, Lines, LineStrip, Triangles, TriangleStrip, TriangleFan, Quads };

struct BlendMode { enum Factor { Zero, One, SrcAlpha, OneMinusSrcAlpha }; };
extern const BlendMode BlendAlpha;

class Texture; // fwd
struct RenderStates {
    static const RenderStates Default;
    const Texture* texture;
    BlendMode blendMode;
    RenderStates() : texture(nullptr) {}
    RenderStates(const Texture* t) : texture(t) {}
    RenderStates(const BlendMode& b) : texture(nullptr), blendMode(b) {}
};

// ---- Image (CPU pixels) ----
class Image {
public:
    Image();
    ~Image();
    bool loadFromFile(const std::string& filename);
    void create(unsigned int width, unsigned int height, const Uint8* pixels = nullptr);
    Vector2u getSize() const;
    const Uint8* getPixelsPtr() const;
    void flipVertically();
private:
    std::vector<Uint8> m_pixels;
    unsigned int m_w, m_h;
};

// ---- Texture (GL texture) ----
class Texture {
public:
    Texture();
    ~Texture();
    bool loadFromFile(const std::string& filename, const IntRect& area = IntRect());
    bool loadFromImage(const Image& image, const IntRect& area = IntRect());
    void update(const Image& image);
    Vector2u getSize() const;
    void setSmooth(bool smooth);
    void setRepeated(bool repeated);
    unsigned int glHandle() const { return m_texture; }
    static void bind(const Texture* texture);
private:
    unsigned int m_texture;  // GL name
    unsigned int m_w, m_h;
    bool m_smooth, m_repeated;
};

// ---- Transformable state (shared by Sprite/Text/Shapes) ----
class Transformable {
public:
    Transformable() : m_pos(0, 0), m_scale(1, 1), m_rotation(0), m_origin(0, 0) {}
    void setPosition(float x, float y) { m_pos = Vector2f(x, y); }
    void setPosition(const Vector2f& p) { m_pos = p; }
    void setScale(float x, float y) { m_scale = Vector2f(x, y); }
    void setScale(const Vector2f& s) { m_scale = s; }
    void setRotation(float angle) { m_rotation = angle; }
    void setOrigin(float x, float y) { m_origin = Vector2f(x, y); }
    const Vector2f& getPosition() const { return m_pos; }
    const Vector2f& getScale() const { return m_scale; }
    float getRotation() const { return m_rotation; }
protected:
    Vector2f m_pos, m_scale;
    float m_rotation;
    Vector2f m_origin;
};

class RenderTarget; // fwd
class Drawable {
public:
    virtual ~Drawable() {}
protected:
    friend class RenderTarget;
    virtual void draw(RenderTarget& target, const RenderStates& states) const = 0;
};

// ---- Sprite ----
class Sprite : public Drawable, public Transformable {
public:
    Sprite();
    explicit Sprite(const Texture& texture);
    Sprite(const Texture& texture, const IntRect& rectangle);
    void setTexture(const Texture& texture, bool resetRect = false);
    void setTextureRect(const IntRect& rectangle);
    void setColor(const Color& color);
    FloatRect getLocalBounds() const;
    FloatRect getGlobalBounds() const;
    const Texture* getTexture() const { return m_texture; }
    const IntRect& getTextureRect() const { return m_rect; }
    const Color& getColor() const { return m_color; }
protected:
    void draw(RenderTarget& target, const RenderStates& states) const override;
    const Texture* m_texture;
    IntRect m_rect;
    Color m_color;
};

// ---- RectangleShape ----
class RectangleShape : public Drawable, public Transformable {
public:
    explicit RectangleShape(const Vector2f& size = Vector2f());
    void setSize(const Vector2f& size);
    void setFillColor(const Color& color);
    void setOutlineColor(const Color& color);
    void setOutlineThickness(float thickness);
    FloatRect getGlobalBounds() const;
protected:
    void draw(RenderTarget& target, const RenderStates& states) const override;
    Vector2f m_rectSize;
    Color m_fill, m_outline;
    float m_outlineThickness = 0.f;
};

// ---- VertexArray ----
class VertexArray : public Drawable {
public:
    VertexArray() : m_type(Points) {}
    VertexArray(PrimitiveType type, std::size_t count = 0) : m_verts(count), m_type(type) {}
    std::size_t getVertexCount() const { return m_verts.size(); }
    Vertex& operator[](std::size_t i) { return m_verts[i]; }
    const Vertex& operator[](std::size_t i) const { return m_verts[i]; }
    void clear() { m_verts.clear(); }
    void resize(std::size_t n) { m_verts.resize(n); }
    void append(const Vertex& v) { m_verts.push_back(v); }
    void setPrimitiveType(PrimitiveType t) { m_type = t; }
    PrimitiveType getPrimitiveType() const { return m_type; }
protected:
    void draw(RenderTarget& target, const RenderStates& states) const override;
    std::vector<Vertex> m_verts;
    PrimitiveType m_type;
};

// ---- Font / Text ----
class Font {
public:
    Font();
    ~Font();
    bool loadFromFile(const std::string& filename);
    void* impl() const { return m_impl; }
private:
    void* m_impl;  // stb_truetype face + atlas (A1d)
};

class Text : public Drawable, public Transformable {
public:
    enum Style { Regular = 0, Bold = 1, Italic = 2, Underlined = 4, StrikeThrough = 8 };
    Text();
    Text(const String& string, const Font& font, unsigned int characterSize = 30);
    void setString(const String& string);
    void setFont(const Font& font);
    void setCharacterSize(unsigned int size);
    void setFillColor(const Color& color);
    void setOutlineColor(const Color& color);
    void setOutlineThickness(float thickness);
    const String& getString() const { return m_string; }
    Vector2f findCharacterPos(std::size_t index) const;
    FloatRect getLocalBounds() const;
    FloatRect getGlobalBounds() const;
protected:
    void draw(RenderTarget& target, const RenderStates& states) const override;
    String m_string;
    const Font* m_font;
    unsigned int m_charSize;
    Color m_fill, m_outline;
    float m_outlineThickness;
};

// ---- RenderTarget / RenderWindow / RenderTexture ----
class RenderTarget {
public:
    virtual ~RenderTarget() {}
    void draw(const Drawable& drawable, const RenderStates& states = RenderStates::Default);
    void draw(const Vertex* vertices, std::size_t vertexCount, PrimitiveType type,
              const RenderStates& states = RenderStates::Default);
    virtual Vector2u getSize() const = 0;
    void clear(const Color& color = Color(0, 0, 0, 255));
    void pushGLStates();
    void popGLStates();
};

class RenderWindow : public Window, public RenderTarget {
public:
    RenderWindow();
    ~RenderWindow() override;
    Vector2u getSize() const override;
    void display();
    bool setActive(bool active = true);
};

class RenderTexture : public RenderTarget {
public:
    RenderTexture();
    ~RenderTexture() override;
    bool create(unsigned int width, unsigned int height, bool depthBuffer = false);
    void display();
    const Texture& getTexture() const;
    void setSmooth(bool smooth);
    Vector2u getSize() const override;
    bool setActive(bool active = true);
private:
    Texture m_texture;
    unsigned int m_fbo, m_w, m_h;
};

} // namespace sf

#endif
