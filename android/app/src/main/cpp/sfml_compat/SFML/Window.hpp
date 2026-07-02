// SFML-compat shim — <SFML/Window.hpp> equivalent for Penguin Dash / Android (A1).
// Input/window API surface ETR names. Enums + POD are real; Window/Context/input
// query methods are declared here and implemented in sfml_compat.cpp (wired to the
// EGL shim + AInputEvent in A1c/A2). Stubbed to start so the engine links.
#ifndef PD_SFML_COMPAT_WINDOW_HPP
#define PD_SFML_COMPAT_WINDOW_HPP

#include "System.hpp"

namespace sf {

class Keyboard {
public:
    enum Key {
        Unknown = -1,
        A = 0, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Escape, LControl, LShift, LAlt, LSystem, RControl, RShift, RAlt, RSystem,
        Menu, LBracket, RBracket, Semicolon, Comma, Period, Apostrophe, Slash,
        Backslash, Grave, Equal, Hyphen, Space, Enter, Backspace, Tab,
        PageUp, PageDown, End, Home, Insert, Delete,
        Add, Subtract, Multiply, Divide, Left, Right, Up, Down,
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15,
        Pause, KeyCount,
        // SFML 2.5 deprecated aliases still named by ETR:
        Return = Enter, BackSpace = Backspace, Dash = Hyphen,
        Quote = Apostrophe, Tilde = Grave, BackSlash = Backslash, SemiColon = Semicolon
    };
    static bool isKeyPressed(Key key);
};

class Mouse {
public:
    enum Button { Left, Right, Middle, XButton1, XButton2, ButtonCount };
    static Vector2i getPosition();
    template <typename T> static Vector2i getPosition(const T&) { return getPosition(); }
};

class Joystick {
public:
    enum { Count = 8 };
    enum Axis { X, Y, Z, R, U, V, PovX, PovY };
    static bool isConnected(unsigned int joystick);
    static unsigned int getButtonCount(unsigned int joystick);
    static bool hasAxis(unsigned int joystick, Axis axis);
    static float getAxisPosition(unsigned int joystick, Axis axis);
};

struct Event {
    enum EventType {
        Closed, Resized, LostFocus, GainedFocus, TextEntered,
        KeyPressed, KeyReleased, MouseWheelMoved, MouseWheelScrolled,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseEntered, MouseLeft,
        JoystickButtonPressed, JoystickButtonReleased, JoystickMoved,
        JoystickConnected, JoystickDisconnected, Count
    };
    struct KeyEvent { Keyboard::Key code; bool alt, control, shift, system; };
    struct TextEvent { Uint32 unicode; };
    struct MouseMoveEvent { int x, y; };
    struct MouseButtonEvent { Mouse::Button button; int x, y; };
    struct JoystickButtonEvent { unsigned int joystickId, button; };
    struct JoystickMoveEvent { unsigned int joystickId; Joystick::Axis axis; float position; };
    struct SizeEvent { unsigned int width, height; };

    EventType type;
    union {
        KeyEvent key;
        TextEvent text;
        MouseMoveEvent mouseMove;
        MouseButtonEvent mouseButton;
        JoystickButtonEvent joystickButton;
        JoystickMoveEvent joystickMove;
        SizeEvent size;
    };
};

struct VideoMode {
    unsigned int width, height, bitsPerPixel;
    VideoMode() : width(0), height(0), bitsPerPixel(32) {}
    VideoMode(unsigned int w, unsigned int h, unsigned int bpp = 32) : width(w), height(h), bitsPerPixel(bpp) {}
    static VideoMode getDesktopMode();
};

struct ContextSettings {
    unsigned int depthBits, stencilBits, antialiasingLevel, majorVersion, minorVersion;
    ContextSettings(unsigned int depth = 0, unsigned int stencil = 0, unsigned int aa = 0,
                    unsigned int major = 1, unsigned int minor = 1)
        : depthBits(depth), stencilBits(stencil), antialiasingLevel(aa),
          majorVersion(major), minorVersion(minor) {}
};

class Style {
public:
    enum { None = 0, Titlebar = 1, Resize = 2, Close = 4, Fullscreen = 8,
           Default = Titlebar | Resize | Close };
};

// The game's window handle. On Android there is exactly one, backed by the EGL
// surface owned by native_main.cpp; these methods bridge to it (impl in .cpp).
class Window {
public:
    Window();
    virtual ~Window();
    void create(VideoMode mode, const String& title, Uint32 style = Style::Default,
                const ContextSettings& settings = ContextSettings());
    void close();
    bool isOpen() const;
    bool pollEvent(Event& event);
    void display();
    void setVerticalSyncEnabled(bool enabled);
    void setFramerateLimit(unsigned int limit);
    void setMouseCursorVisible(bool visible);
    void setTitle(const String& title);
    void setActive(bool active = true);
    Vector2u getSize() const;
protected:
    Vector2u m_size;
    bool m_open;
};

} // namespace sf

#endif
