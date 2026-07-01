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

#endif
