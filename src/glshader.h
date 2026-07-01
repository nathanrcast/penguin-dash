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
#include "matrices.h" // TMatrix (2D model transform)

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
// Set a model transform applied before the ortho projection (replaces the
// fixed-function glTranslate/glRotate on the modelview). u_mvp becomes
// ortho * model. Reset by calling again with the identity, or re-Begin.
void Shader2D_SetModel(const TMatrix<4, 4>& model);
// Draw a quad/fan. pos2 = xy float pairs; uv2 = st float pairs (may be null if
// !useTexture). color is a constant vertex colour.
void Shader2D_DrawArrays(unsigned int mode, const float* pos2, const float* uv2,
                         int vertCount, bool useTexture, const sf::Color& color);
// Restore the fixed-function pipeline (glUseProgram 0) for subsequent legacy/SFML draws.
void Shader2D_End();

// --- M2: 3D shader draw path (terrain first) ---
// Desktop-first strategy: rather than intercept every matrix/light/fog call
// scattered through the engine, we SNAPSHOT the live fixed-function GL state
// (proj/modelview matrices, light 0, global ambient, material, linear fog,
// object-linear texgen, enables) via glGet* at draw time and reproduce it in
// the 3D program. This guarantees a pixel match with the fixed-function
// pipeline on desktop. On GLES2 (Android) glGet of this state is unavailable,
// so this snapshot is replaced by tracked matrices/uniforms in a later pass.
//
// Begin for an interleaved VNC vertex buffer (position 3f, normal 3f, color
// 4ub) — the terrain layout. Snapshots state, binds the 3D program, uploads
// all uniforms, and points the generic attribs into the buffer.
void Shader3D_BeginVNC(const void* base, int stride, int posOff, int normOff, int colOff);
// Re-read the dynamic fog enable and re-upload u_useFog (the terrain env-map
// pass toggles GL_FOG between draws). Cheap; call before each draw.
void Shader3D_SyncFog();
// glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, indices) through the program.
void Shader3D_DrawElementsU32(unsigned int count, const unsigned int* indices);

// Object/billboard path (trees, items). Begin3D snapshots the env + base
// proj/view once; SetModel3D applies a per-object model (view*model), replacing
// the fixed-function glPushMatrix/glTranslate/glRotate. SetObjectArrays binds
// float-xyz positions + short-st texcoords + a constant normal. DrawQuadArray
// draws nQuads consecutive quads (4 verts each) as triangles (no GL_QUADS in GLES2).
void Shader3D_Begin3D();
void Shader3D_SetModel3D(const TMatrix<4, 4>& model);
void Shader3D_SetObjectArrays(const float* pos, const short* tex, float nx, float ny, float nz);
void Shader3D_DrawQuadArray(int nQuads);

// Environment (M3): unlit primitives. TexturedArray = position + texcoord +
// constant colour (skybox sides). ColoredArray = position + per-vertex colour
// (fog plane gradient). DrawArrays draws the bound arrays with any GL mode.
void Shader3D_SetTexturedArray(const void* pos, unsigned int posType, const float* tex, const sf::Color& col);
void Shader3D_SetColoredArray(const float* pos, const unsigned char* color);
void Shader3D_DrawArrays(unsigned int mode, int count);

// Character (M4): explicit material + lit untextured mesh (Tux spheres).
void Shader3D_SetMaterial(const sf::Color& diffuse, const sf::Color& specular, float shininess);
void Shader3D_SetPosNormalArrays(const float* pos, const float* normal);
void Shader3D_DrawElementsU16(unsigned int count, const unsigned short* indices);

// Disable the attrib arrays and restore the fixed-function pipeline.
void Shader3D_End();

#endif
