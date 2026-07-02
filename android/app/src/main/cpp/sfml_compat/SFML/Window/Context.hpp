// SFML-compat shim — <SFML/Window/Context.hpp> for Penguin Dash / Android (A1).
// ETR only uses sf::Context::getFunction to load GL2.0 entry points in glshader.cpp.
// On GLES2 those are core; we resolve them via eglGetProcAddress (impl in .cpp).
#ifndef PD_SFML_COMPAT_WINDOW_CONTEXT_HPP
#define PD_SFML_COMPAT_WINDOW_CONTEXT_HPP

namespace sf {

typedef void (*GlFunctionPointer)();

class Context {
public:
    static GlFunctionPointer getFunction(const char* name);
};

} // namespace sf

#endif
