// SFML-compat shim for Penguin Dash on Android (port step 3, A1).
// Provides the sf:: value types ETR uses so its <SFML/System.hpp> include
// resolves here (this dir is first on the Android include path). Value types are
// fully implemented inline; nothing here needs the platform.
#ifndef PD_SFML_COMPAT_SYSTEM_HPP
#define PD_SFML_COMPAT_SYSTEM_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <chrono>

namespace sf {

typedef std::int8_t   Int8;
typedef std::uint8_t  Uint8;
typedef std::int16_t  Int16;
typedef std::uint16_t Uint16;
typedef std::int32_t  Int32;
typedef std::uint32_t Uint32;
typedef std::int64_t  Int64;
typedef std::uint64_t Uint64;

// ---- Vector2 ----
template <typename T>
class Vector2 {
public:
    T x, y;
    Vector2() : x(0), y(0) {}
    Vector2(T X, T Y) : x(X), y(Y) {}
    template <typename U>
    explicit Vector2(const Vector2<U>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}
};
template <typename T> Vector2<T> operator-(const Vector2<T>& r) { return Vector2<T>(-r.x, -r.y); }
template <typename T> Vector2<T> operator+(const Vector2<T>& a, const Vector2<T>& b) { return Vector2<T>(a.x + b.x, a.y + b.y); }
template <typename T> Vector2<T> operator-(const Vector2<T>& a, const Vector2<T>& b) { return Vector2<T>(a.x - b.x, a.y - b.y); }
template <typename T> Vector2<T> operator*(const Vector2<T>& a, T s) { return Vector2<T>(a.x * s, a.y * s); }
template <typename T> Vector2<T> operator*(T s, const Vector2<T>& a) { return Vector2<T>(a.x * s, a.y * s); }
template <typename T> Vector2<T>& operator+=(Vector2<T>& a, const Vector2<T>& b) { a.x += b.x; a.y += b.y; return a; }
template <typename T> Vector2<T>& operator-=(Vector2<T>& a, const Vector2<T>& b) { a.x -= b.x; a.y -= b.y; return a; }
template <typename T> bool operator==(const Vector2<T>& a, const Vector2<T>& b) { return a.x == b.x && a.y == b.y; }
template <typename T> bool operator!=(const Vector2<T>& a, const Vector2<T>& b) { return !(a == b); }
typedef Vector2<int>          Vector2i;
typedef Vector2<unsigned int> Vector2u;
typedef Vector2<float>        Vector2f;

// ---- String (UTF-8 backed; ETR menu text is ASCII/UTF-8) ----
class String {
public:
    String() {}
    String(char c) : m(1, c) {}
    String(const char* s) : m(s ? s : "") {}
    String(const std::string& s) : m(s) {}
    template <typename T>
    static String fromUtf8(T begin, T end) { String s; s.m.assign(begin, end); return s; }
    operator std::string() const { return m; }
    std::string toAnsiString() const { return m; }
    std::basic_string<Uint8> toUtf8() const { return std::basic_string<Uint8>(m.begin(), m.end()); }
    std::size_t getSize() const { return m.size(); }
    bool isEmpty() const { return m.empty(); }
    char operator[](std::size_t i) const { return m[i]; }
    char& operator[](std::size_t i) { return m[i]; }
    String& operator+=(const String& r) { m += r.m; return *this; }
    String& insert(std::size_t pos, const String& s) { m.insert(pos, s.m); return *this; }
    void erase(std::size_t pos, std::size_t count = 1) { if (pos < m.size()) m.erase(pos, count); }
    const std::string& str() const { return m; }
    friend bool operator==(const String& a, const String& b) { return a.m == b.m; }
    friend bool operator!=(const String& a, const String& b) { return a.m != b.m; }
    friend bool operator<(const String& a, const String& b) { return a.m < b.m; }
private:
    std::string m;
};
inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }

// ---- Clock / Time ----
class Time {
public:
    Time() : us(0) {}
    explicit Time(Int64 micro) : us(micro) {}
    float asSeconds() const { return us / 1000000.0f; }
    Int32 asMilliseconds() const { return static_cast<Int32>(us / 1000); }
    Int64 asMicroseconds() const { return us; }
private:
    Int64 us;
};
class Clock {
public:
    Clock() : start(std::chrono::steady_clock::now()) {}
    Time getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        return Time(std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
    }
    Time restart() { Time t = getElapsedTime(); start = std::chrono::steady_clock::now(); return t; }
private:
    std::chrono::steady_clock::time_point start;
};

class NonCopyable {
protected:
    NonCopyable() {}
    ~NonCopyable() {}
private:
    NonCopyable(const NonCopyable&);
    NonCopyable& operator=(const NonCopyable&);
};

} // namespace sf

#endif
