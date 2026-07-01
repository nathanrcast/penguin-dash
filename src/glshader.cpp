/* --------------------------------------------------------------------
PENGUIN DASH (fork of Extreme Tux Racer)

GLES2 port scaffolding — M0. See glshader.h.

Licensed under the GNU General Public License; see COPYING.
---------------------------------------------------------------------*/

#include "glshader.h"
#include "glmatrix.h"
#include "common.h" // Message()
#include <SFML/Window/Context.hpp>
#include <string>
#include <vector>

TCoreShaders CoreShaders;

// --- GL2.0 entry points loaded at runtime (portable across desktop GL / GLES2) ---
namespace {
PFNGLCREATESHADERPROC       pglCreateShader = nullptr;
PFNGLSHADERSOURCEPROC       pglShaderSource = nullptr;
PFNGLCOMPILESHADERPROC      pglCompileShader = nullptr;
PFNGLGETSHADERIVPROC        pglGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC   pglGetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC       pglDeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC      pglCreateProgram = nullptr;
PFNGLATTACHSHADERPROC       pglAttachShader = nullptr;
PFNGLLINKPROGRAMPROC        pglLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC       pglGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC  pglGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC         pglUseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
PFNGLGETATTRIBLOCATIONPROC  pglGetAttribLocation = nullptr;
// M1 draw path
PFNGLUNIFORMMATRIX4FVPROC        pglUniformMatrix4fv = nullptr;
PFNGLUNIFORM1IPROC               pglUniform1i = nullptr;
PFNGLVERTEXATTRIB4FPROC          pglVertexAttrib4f = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC     pglVertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC pglDisableVertexAttribArray = nullptr;

bool g_functionsLoaded = false;

template<typename T>
bool load(T& fn, const char* name) {
	fn = reinterpret_cast<T>(sf::Context::getFunction(name));
	return fn != nullptr;
}

// GLSL preamble. Desktop GL 2.1 uses #version 120; the Android/GLES2 build will
// swap this for "#version 100\nprecision highp float;\n". Shader bodies below use
// the attribute/varying/gl_FragColor syntax common to both.
const char* PREAMBLE = "#version 120\n";

unsigned int compileStage(GLenum type, const char* src, const char* progName) {
	unsigned int sh = pglCreateShader(type);
	const char* parts[2] = { PREAMBLE, src };
	pglShaderSource(sh, 2, parts, nullptr);
	pglCompileShader(sh);

	GLint ok = GL_FALSE;
	pglGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (ok != GL_TRUE) {
		GLint len = 0;
		pglGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len > 1 ? len : 1);
		pglGetShaderInfoLog(sh, (GLsizei)log.size(), nullptr, log.data());
		Message(std::string("shader compile failed (") + progName + "): ", log.data());
		pglDeleteShader(sh);
		return 0;
	}
	return sh;
}
} // namespace

bool InitShaderFunctions() {
	if (g_functionsLoaded) return true;
	bool ok = true;
	ok &= load(pglCreateShader, "glCreateShader");
	ok &= load(pglShaderSource, "glShaderSource");
	ok &= load(pglCompileShader, "glCompileShader");
	ok &= load(pglGetShaderiv, "glGetShaderiv");
	ok &= load(pglGetShaderInfoLog, "glGetShaderInfoLog");
	ok &= load(pglDeleteShader, "glDeleteShader");
	ok &= load(pglCreateProgram, "glCreateProgram");
	ok &= load(pglAttachShader, "glAttachShader");
	ok &= load(pglLinkProgram, "glLinkProgram");
	ok &= load(pglGetProgramiv, "glGetProgramiv");
	ok &= load(pglGetProgramInfoLog, "glGetProgramInfoLog");
	ok &= load(pglUseProgram, "glUseProgram");
	ok &= load(pglGetUniformLocation, "glGetUniformLocation");
	ok &= load(pglGetAttribLocation, "glGetAttribLocation");
	ok &= load(pglUniformMatrix4fv, "glUniformMatrix4fv");
	ok &= load(pglUniform1i, "glUniform1i");
	ok &= load(pglVertexAttrib4f, "glVertexAttrib4f");
	ok &= load(pglVertexAttribPointer, "glVertexAttribPointer");
	ok &= load(pglEnableVertexAttribArray, "glEnableVertexAttribArray");
	ok &= load(pglDisableVertexAttribArray, "glDisableVertexAttribArray");
	g_functionsLoaded = ok;
	return ok;
}

bool TShaderProgram::Compile(const char* vsrc, const char* fsrc, const char* name) {
	unsigned int vs = compileStage(GL_VERTEX_SHADER, vsrc, name);
	if (!vs) return false;
	unsigned int fs = compileStage(GL_FRAGMENT_SHADER, fsrc, name);
	if (!fs) { pglDeleteShader(vs); return false; }

	unsigned int prog = pglCreateProgram();
	pglAttachShader(prog, vs);
	pglAttachShader(prog, fs);
	pglLinkProgram(prog);
	// Shaders can be flagged for deletion once attached+linked.
	pglDeleteShader(vs);
	pglDeleteShader(fs);

	GLint ok = GL_FALSE;
	pglGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok != GL_TRUE) {
		GLint len = 0;
		pglGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len > 1 ? len : 1);
		pglGetProgramInfoLog(prog, (GLsizei)log.size(), nullptr, log.data());
		Message(std::string("shader link failed (") + name + "): ", log.data());
		return false;
	}
	program = prog;
	return true;
}

void TShaderProgram::Use() const {
	if (pglUseProgram) pglUseProgram(program);
}

int TShaderProgram::Uniform(const char* name) const {
	return program ? pglGetUniformLocation(program, name) : -1;
}

int TShaderProgram::Attrib(const char* name) const {
	return program ? pglGetAttribLocation(program, name) : -1;
}

// --------------------------------------------------------------------
//  Core shader sources
// --------------------------------------------------------------------
static const char* VS_2D =
	"attribute vec2 a_position;\n"
	"attribute vec2 a_texcoord;\n"
	"attribute vec4 a_color;\n"
	"uniform mat4 u_mvp;\n"
	"varying vec2 v_texcoord;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  v_texcoord = a_texcoord;\n"
	"  v_color = a_color;\n"
	"  gl_Position = u_mvp * vec4(a_position, 0.0, 1.0);\n"
	"}\n";

static const char* FS_2D =
	"uniform sampler2D u_tex;\n"
	"uniform int u_useTexture;\n"
	"varying vec2 v_texcoord;\n"
	"varying vec4 v_color;\n"
	"void main() {\n"
	"  vec4 c = v_color;\n"
	"  if (u_useTexture != 0) c *= texture2D(u_tex, v_texcoord);\n"
	"  gl_FragColor = c;\n"
	"}\n";

static const char* VS_3D =
	"attribute vec3 a_position;\n"
	"attribute vec3 a_normal;\n"
	"attribute vec2 a_texcoord;\n"
	"attribute vec4 a_color;\n"
	"uniform mat4 u_mvp;\n"
	"uniform mat4 u_modelview;\n"
	"uniform mat3 u_normalMatrix;\n"
	"varying vec2 v_texcoord;\n"
	"varying vec4 v_color;\n"
	"varying vec3 v_normal;\n"
	"varying vec3 v_eyePos;\n"
	"void main() {\n"
	"  v_texcoord = a_texcoord;\n"
	"  v_color = a_color;\n"
	"  v_normal = u_normalMatrix * a_normal;\n"
	"  vec4 ep = u_modelview * vec4(a_position, 1.0);\n"
	"  v_eyePos = ep.xyz;\n"
	"  gl_Position = u_mvp * vec4(a_position, 1.0);\n"
	"}\n";

static const char* FS_3D =
	"uniform sampler2D u_tex;\n"
	"uniform int u_useTexture;\n"
	"uniform int u_useLighting;\n"
	"uniform vec3 u_lightDir;\n"
	"uniform vec4 u_matSpecular;\n"
	"uniform float u_shininess;\n"
	"uniform int u_useFog;\n"
	"uniform vec4 u_fogColor;\n"
	"uniform float u_fogNear;\n"
	"uniform float u_fogFar;\n"
	"varying vec2 v_texcoord;\n"
	"varying vec4 v_color;\n"
	"varying vec3 v_normal;\n"
	"varying vec3 v_eyePos;\n"
	"void main() {\n"
	"  vec4 base = v_color;\n"
	"  if (u_useTexture != 0) base *= texture2D(u_tex, v_texcoord);\n"
	"  vec3 col = base.rgb;\n"
	"  if (u_useLighting != 0) {\n"
	"    vec3 n = normalize(v_normal);\n"
	"    vec3 l = normalize(u_lightDir);\n"
	"    float diff = max(dot(n, l), 0.0);\n"
	"    vec3 viewDir = normalize(-v_eyePos);\n"
	"    vec3 h = normalize(l + viewDir);\n"
	"    float spec = pow(max(dot(n, h), 0.0), max(u_shininess, 1.0));\n"
	"    col = base.rgb * (0.3 + 0.7 * diff) + u_matSpecular.rgb * spec;\n"
	"  }\n"
	"  vec4 outc = vec4(col, base.a);\n"
	"  if (u_useFog != 0) {\n"
	"    float dist = length(v_eyePos);\n"
	"    float f = clamp((u_fogFar - dist) / (u_fogFar - u_fogNear), 0.0, 1.0);\n"
	"    outc.rgb = mix(u_fogColor.rgb, outc.rgb, f);\n"
	"  }\n"
	"  gl_FragColor = outc;\n"
	"}\n";

void InitCoreShaders() {
	CoreShaders.ready = false;
	if (!InitShaderFunctions()) {
		Message("GLES2 scaffold (M0): shader entry points unavailable — staying on fixed-function");
		return;
	}
	bool ok = CoreShaders.shader2d.Compile(VS_2D, FS_2D, "core2d")
	          && CoreShaders.shader3d.Compile(VS_3D, FS_3D, "core3d");
	CoreShaders.ready = ok;
	if (ok)
		Message("GLES2 scaffold (M0): core 2D/3D shaders compiled + linked OK");
	else
		Message("GLES2 scaffold (M0): core shader build FAILED — staying on fixed-function");

	// M0 does not use the programs; make sure nothing is left bound.
	if (pglUseProgram) pglUseProgram(0);
}

// --------------------------------------------------------------------
//  M1: 2D shader draw path
// --------------------------------------------------------------------
static GLint a2d_pos = -1, a2d_uv = -1, a2d_col = -1;
static GLint u2d_mvp = -1, u2d_useTex = -1, u2d_tex = -1;
static bool  a2d_cached = false;

void Shader2D_Begin(float screenW, float screenH) {
	if (!CoreShaders.ready) return;
	CoreShaders.shader2d.Use();
	if (!a2d_cached) {
		a2d_pos    = CoreShaders.shader2d.Attrib("a_position");
		a2d_uv     = CoreShaders.shader2d.Attrib("a_texcoord");
		a2d_col    = CoreShaders.shader2d.Attrib("a_color");
		u2d_mvp    = CoreShaders.shader2d.Uniform("u_mvp");
		u2d_useTex = CoreShaders.shader2d.Uniform("u_useTexture");
		u2d_tex    = CoreShaders.shader2d.Uniform("u_tex");
		a2d_cached = true;
	}
	// Match legacy Setup2dScene: glOrtho(0, w, 0, h, -1, 1), origin bottom-left.
	TMatrix<4, 4> ortho = MakeOrtho(0, screenW, 0, screenH, -1, 1);
	float m[16];
	MatrixToGL(ortho, m);
	pglUniformMatrix4fv(u2d_mvp, 1, GL_FALSE, m);
	pglUniform1i(u2d_tex, 0);
}

void Shader2D_DrawArrays(unsigned int mode, const float* pos2, const float* uv2,
                         int vertCount, bool useTexture, const sf::Color& color) {
	if (!CoreShaders.ready || a2d_pos < 0) return;
	pglUniform1i(u2d_useTex, useTexture ? 1 : 0);
	if (a2d_col >= 0)
		pglVertexAttrib4f(a2d_col, color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);

	pglEnableVertexAttribArray(a2d_pos);
	pglVertexAttribPointer(a2d_pos, 2, GL_FLOAT, GL_FALSE, 0, pos2);
	if (useTexture && a2d_uv >= 0) {
		pglEnableVertexAttribArray(a2d_uv);
		pglVertexAttribPointer(a2d_uv, 2, GL_FLOAT, GL_FALSE, 0, uv2);
	}

	glDrawArrays(mode, 0, vertCount);

	pglDisableVertexAttribArray(a2d_pos);
	if (useTexture && a2d_uv >= 0)
		pglDisableVertexAttribArray(a2d_uv);
}

void Shader2D_End() {
	if (pglUseProgram) pglUseProgram(0);
}
