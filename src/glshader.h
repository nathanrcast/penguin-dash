/* --------------------------------------------------------------------
PENGUIN DASH (fork of Extreme Tux Racer)

GLES2 port scaffolding — M0. A minimal GLSL program wrapper plus the core
2D/3D shader programs. In M0 these are compiled and validated at startup but
NOT bound — the fixed-function pipeline stays active. Later milestones (M1+)
switch the draw paths onto these programs.

Licensed under the GNU General Public License; see COPYING.
---------------------------------------------------------------------*/

#ifndef GLSHADER_H
#define GLSHADER_H

#include "bh.h" // GL types (GLuint/GLint via GL/gl.h)

class TShaderProgram {
	unsigned int program = 0;
public:
	bool Compile(const char* vsrc, const char* fsrc, const char* name);
	void Use() const;
	bool Valid() const { return program != 0; }
	int Uniform(const char* name) const;
	int Attrib(const char* name) const;
	unsigned int Id() const { return program; }
};

// Load the GL2.0 shader entry points via sf::Context::getFunction. Idempotent;
// returns false if the driver does not expose shader support.
bool InitShaderFunctions();

// Compile the core 2D/3D programs (M0). Never binds them; logs the outcome.
void InitCoreShaders();

struct TCoreShaders {
	TShaderProgram shader2d;
	TShaderProgram shader3d;
	bool ready = false;
};
extern TCoreShaders CoreShaders;

// --- M1: 2D shader draw path (screen-space quads through the core 2D program) ---
// Bind the 2D program with an ortho projection sized to the screen (pixels,
// origin bottom-left to match the legacy fixed-function Setup2dScene).
void Shader2D_Begin(float screenW, float screenH);
// Draw a quad/fan. pos2 = xy float pairs; uv2 = st float pairs (may be null if
// !useTexture). color is a constant vertex colour.
void Shader2D_DrawArrays(unsigned int mode, const float* pos2, const float* uv2,
                         int vertCount, bool useTexture, const sf::Color& color);
// Restore the fixed-function pipeline (glUseProgram 0) for subsequent legacy/SFML draws.
void Shader2D_End();

#endif
