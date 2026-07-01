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
// M2 3D path
PFNGLUNIFORM1FPROC               pglUniform1f = nullptr;
PFNGLUNIFORM3FVPROC              pglUniform3fv = nullptr;
PFNGLUNIFORM4FVPROC              pglUniform4fv = nullptr;
PFNGLUNIFORMMATRIX3FVPROC        pglUniformMatrix3fv = nullptr;
PFNGLVERTEXATTRIB3FPROC          pglVertexAttrib3f = nullptr;

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
	ok &= load(pglUniform1f, "glUniform1f");
	ok &= load(pglUniform3fv, "glUniform3fv");
	ok &= load(pglUniform4fv, "glUniform4fv");
	ok &= load(pglUniformMatrix3fv, "glUniformMatrix3fv");
	ok &= load(pglVertexAttrib3f, "glVertexAttrib3f");
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

// Per-vertex (Gouraud) lighting to match the fixed-function pipeline exactly.
// Reproduces: GL_COLOR_MATERIAL (vertex color = ambient+diffuse material),
// single directional light 0 (ambient+diffuse+optional specular, infinite
// viewer H = normalize(L + z)), object-linear texgen, and linear eye-distance
// fog — all fed from glGet snapshots.
static const char* VS_3D =
	"attribute vec3 a_position;\n"
	"attribute vec3 a_normal;\n"
	"attribute vec2 a_texcoord;\n"
	"attribute vec4 a_color;\n"
	"uniform mat4 u_mvp;\n"
	"uniform mat4 u_modelview;\n"
	"uniform mat3 u_normalMatrix;\n"
	"uniform int u_useLighting;\n"
	"uniform int u_useColorMaterial;\n"
	"uniform vec3 u_lightDir;\n"
	"uniform vec4 u_globalAmbient;\n"
	"uniform vec4 u_lightAmbient;\n"
	"uniform vec4 u_lightDiffuse;\n"
	"uniform vec4 u_lightSpecular;\n"
	"uniform vec4 u_matAmbient;\n"
	"uniform vec4 u_matDiffuse;\n"
	"uniform vec4 u_matSpecular;\n"
	"uniform float u_shininess;\n"
	"uniform int u_useTexGen;\n"
	"uniform vec4 u_texGenS;\n"
	"uniform vec4 u_texGenT;\n"
	"uniform int u_useFog;\n"
	"uniform float u_fogStart;\n"
	"uniform float u_fogEnd;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord;\n"
	"varying float v_fog;\n"
	"void main() {\n"
	"  vec4 amb = (u_useColorMaterial != 0) ? a_color : u_matAmbient;\n"
	"  vec4 dif = (u_useColorMaterial != 0) ? a_color : u_matDiffuse;\n"
	"  vec4 lit;\n"
	"  if (u_useLighting != 0) {\n"
	"    vec3 n = normalize(u_normalMatrix * a_normal);\n"
	"    vec3 l = normalize(u_lightDir);\n"
	"    float ndl = max(dot(n, l), 0.0);\n"
	"    vec3 rgb = amb.rgb * (u_globalAmbient.rgb + u_lightAmbient.rgb)\n"
	"             + dif.rgb * u_lightDiffuse.rgb * ndl;\n"
	"    if (u_matSpecular.r + u_matSpecular.g + u_matSpecular.b > 0.0) {\n"
	"      vec3 h = normalize(l + vec3(0.0, 0.0, 1.0));\n"
	"      float sp = pow(max(dot(n, h), 0.0), max(u_shininess, 1.0));\n"
	"      rgb += u_matSpecular.rgb * u_lightSpecular.rgb * sp;\n"
	"    }\n"
	"    lit = vec4(rgb, dif.a);\n"
	"  } else {\n"
	"    lit = a_color;\n" // lighting off: fixed-function uses the vertex color directly
	"  }\n"
	"  v_color = clamp(lit, 0.0, 1.0);\n"
	"  vec4 p4 = vec4(a_position, 1.0);\n"
	"  v_texcoord = (u_useTexGen != 0) ? vec2(dot(u_texGenS, p4), dot(u_texGenT, p4)) : a_texcoord;\n"
	"  float dist = length((u_modelview * p4).xyz);\n"
	"  v_fog = (u_useFog != 0) ? clamp((u_fogEnd - dist) / (u_fogEnd - u_fogStart), 0.0, 1.0) : 1.0;\n"
	"  gl_Position = u_mvp * p4;\n"
	"}\n";

static const char* FS_3D =
	"uniform sampler2D u_tex;\n"
	"uniform int u_useTexture;\n"
	"uniform int u_alphaTest;\n"   // GLES2 has no glAlphaFunc: discard instead
	"uniform float u_alphaRef;\n"
	"uniform vec4 u_fogColor;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord;\n"
	"varying float v_fog;\n"
	"void main() {\n"
	"  vec4 c = v_color;\n"
	"  if (u_useTexture != 0) c *= texture2D(u_tex, v_texcoord);\n"
	"  if (u_alphaTest != 0 && c.a < u_alphaRef) discard;\n"
	"  c.rgb = mix(u_fogColor.rgb, c.rgb, v_fog);\n"
	"  gl_FragColor = c;\n"
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
static TMatrix<4, 4> g_ortho2d; // current 2D projection; SetModel composes onto it

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
	g_ortho2d = MakeOrtho(0, screenW, 0, screenH, -1, 1);
	float m[16];
	MatrixToGL(g_ortho2d, m);
	pglUniformMatrix4fv(u2d_mvp, 1, GL_FALSE, m);
	pglUniform1i(u2d_tex, 0);
}

void Shader2D_SetModel(const TMatrix<4, 4>& model) {
	if (!CoreShaders.ready || u2d_mvp < 0) return;
	TMatrix<4, 4> mvp = g_ortho2d * model; // column-major: mvp*v = ortho*(model*v)
	float m[16];
	MatrixToGL(mvp, m);
	pglUniformMatrix4fv(u2d_mvp, 1, GL_FALSE, m);
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

// --------------------------------------------------------------------
//  M2: 3D shader draw path (terrain)
// --------------------------------------------------------------------
namespace {
struct A3D {
	GLint pos = -1, normal = -1, texcoord = -1, color = -1;
	GLint mvp = -1, modelview = -1, normalMatrix = -1;
	GLint useLighting = -1, useColorMaterial = -1;
	GLint lightDir = -1, globalAmbient = -1, lightAmbient = -1, lightDiffuse = -1, lightSpecular = -1;
	GLint matAmbient = -1, matDiffuse = -1, matSpecular = -1, shininess = -1;
	GLint useTexGen = -1, texGenS = -1, texGenT = -1;
	GLint useTexture = -1, tex = -1, alphaTest = -1, alphaRef = -1;
	GLint useFog = -1, fogColor = -1, fogStart = -1, fogEnd = -1;
	bool cached = false;
} a3d;

void normalMatrixFromMV(const double mv[16], float out[9]); // defined below

// Base proj/view captured by Shader3D_Begin3D; Shader3D_SetModel3D composes a
// per-object model onto them (avoids the fixed-function matrix stack for objects).
TMatrix<4, 4> g3d_proj, g3d_view;

void cacheLocations3D(const TShaderProgram& p) {
	if (a3d.cached) return;
	a3d.pos          = p.Attrib("a_position");
	a3d.normal       = p.Attrib("a_normal");
	a3d.texcoord     = p.Attrib("a_texcoord");
	a3d.color        = p.Attrib("a_color");
	a3d.mvp          = p.Uniform("u_mvp");
	a3d.modelview    = p.Uniform("u_modelview");
	a3d.normalMatrix = p.Uniform("u_normalMatrix");
	a3d.useLighting  = p.Uniform("u_useLighting");
	a3d.useColorMaterial = p.Uniform("u_useColorMaterial");
	a3d.lightDir     = p.Uniform("u_lightDir");
	a3d.globalAmbient = p.Uniform("u_globalAmbient");
	a3d.lightAmbient = p.Uniform("u_lightAmbient");
	a3d.lightDiffuse = p.Uniform("u_lightDiffuse");
	a3d.lightSpecular = p.Uniform("u_lightSpecular");
	a3d.matAmbient   = p.Uniform("u_matAmbient");
	a3d.matDiffuse   = p.Uniform("u_matDiffuse");
	a3d.matSpecular  = p.Uniform("u_matSpecular");
	a3d.shininess    = p.Uniform("u_shininess");
	a3d.useTexGen    = p.Uniform("u_useTexGen");
	a3d.texGenS      = p.Uniform("u_texGenS");
	a3d.texGenT      = p.Uniform("u_texGenT");
	a3d.useTexture   = p.Uniform("u_useTexture");
	a3d.tex          = p.Uniform("u_tex");
	a3d.alphaTest    = p.Uniform("u_alphaTest");
	a3d.alphaRef     = p.Uniform("u_alphaRef");
	a3d.useFog       = p.Uniform("u_useFog");
	a3d.fogColor     = p.Uniform("u_fogColor");
	a3d.fogStart     = p.Uniform("u_fogStart");
	a3d.fogEnd       = p.Uniform("u_fogEnd");
	a3d.cached = true;
}

// Snapshot all non-matrix fixed-function state into the 3D program uniforms:
// lighting, material, texgen, texture, alpha test, fog.
void snapEnv3D() {
	pglUniform1i(a3d.useLighting, glIsEnabled(GL_LIGHTING) ? 1 : 0);
	pglUniform1i(a3d.useColorMaterial, glIsEnabled(GL_COLOR_MATERIAL) ? 1 : 0);

	GLfloat lpos[4], la[4], ld[4], ls[4], ga[4];
	glGetLightfv(GL_LIGHT0, GL_POSITION, lpos); // eye-space (w=0 -> direction)
	glGetLightfv(GL_LIGHT0, GL_AMBIENT, la);
	glGetLightfv(GL_LIGHT0, GL_DIFFUSE, ld);
	glGetLightfv(GL_LIGHT0, GL_SPECULAR, ls);
	glGetFloatv(GL_LIGHT_MODEL_AMBIENT, ga);
	pglUniform3fv(a3d.lightDir, 1, lpos);
	pglUniform4fv(a3d.globalAmbient, 1, ga);
	pglUniform4fv(a3d.lightAmbient, 1, la);
	pglUniform4fv(a3d.lightDiffuse, 1, ld);
	pglUniform4fv(a3d.lightSpecular, 1, ls);

	GLfloat ma[4], md[4], ms[4], sh = 1.f;
	glGetMaterialfv(GL_FRONT, GL_AMBIENT, ma);
	glGetMaterialfv(GL_FRONT, GL_DIFFUSE, md);
	glGetMaterialfv(GL_FRONT, GL_SPECULAR, ms);
	glGetMaterialfv(GL_FRONT, GL_SHININESS, &sh);
	pglUniform4fv(a3d.matAmbient, 1, ma);
	pglUniform4fv(a3d.matDiffuse, 1, md);
	pglUniform4fv(a3d.matSpecular, 1, ms);
	pglUniform1f(a3d.shininess, sh);

	bool texGen = glIsEnabled(GL_TEXTURE_GEN_S) != 0;
	pglUniform1i(a3d.useTexGen, texGen ? 1 : 0);
	if (texGen) {
		GLfloat ps[4], pt[4];
		glGetTexGenfv(GL_S, GL_OBJECT_PLANE, ps);
		glGetTexGenfv(GL_T, GL_OBJECT_PLANE, pt);
		pglUniform4fv(a3d.texGenS, 1, ps);
		pglUniform4fv(a3d.texGenT, 1, pt);
	}
	pglUniform1i(a3d.useTexture, glIsEnabled(GL_TEXTURE_2D) ? 1 : 0);
	pglUniform1i(a3d.tex, 0);

	GLfloat aref = 0.f;
	glGetFloatv(GL_ALPHA_TEST_REF, &aref);
	pglUniform1i(a3d.alphaTest, glIsEnabled(GL_ALPHA_TEST) ? 1 : 0);
	pglUniform1f(a3d.alphaRef, aref);

	GLfloat fc[4], fs = 0.f, fe = 1.f;
	glGetFloatv(GL_FOG_COLOR, fc);
	glGetFloatv(GL_FOG_START, &fs);
	glGetFloatv(GL_FOG_END, &fe);
	pglUniform1i(a3d.useFog, glIsEnabled(GL_FOG) ? 1 : 0);
	pglUniform4fv(a3d.fogColor, 1, fc);
	pglUniform1f(a3d.fogStart, fs);
	pglUniform1f(a3d.fogEnd, fe);
}

// Read a GL matrix (column-major double[16]) into a TMatrix.
void glMatrixToTMatrix(GLenum which, TMatrix<4, 4>& out) {
	GLdouble m[16];
	glGetDoublev(which, m);
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++)
			out[c][r] = m[c * 4 + r];
}

// Upload mvp/modelview/normalMatrix given a modelview TMatrix.
void uploadMatrices3D(const TMatrix<4, 4>& projM, const TMatrix<4, 4>& mvM) {
	float m16[16];
	MatrixToGL(projM * mvM, m16);
	pglUniformMatrix4fv(a3d.mvp, 1, GL_FALSE, m16);
	MatrixToGL(mvM, m16);
	pglUniformMatrix4fv(a3d.modelview, 1, GL_FALSE, m16);
	double mvd[16];
	for (int i = 0; i < 16; i++) mvd[i] = mvM.data()[i];
	float nm[9];
	normalMatrixFromMV(mvd, nm);
	pglUniformMatrix3fv(a3d.normalMatrix, 1, GL_FALSE, nm);
}

// normalMatrix = (upper-left 3x3 of modelview)^-T, column-major for glUniformMatrix3fv.
void normalMatrixFromMV(const double mv[16], float out[9]) {
	double m[3][3];
	for (int c = 0; c < 3; c++)
		for (int r = 0; r < 3; r++)
			m[r][c] = mv[c * 4 + r];
	double cof[3][3];
	cof[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
	cof[0][1] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]);
	cof[0][2] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
	cof[1][0] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]);
	cof[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]);
	cof[1][2] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]);
	cof[2][0] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
	cof[2][1] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]);
	cof[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]);
	double det = m[0][0] * cof[0][0] + m[0][1] * cof[0][1] + m[0][2] * cof[0][2];
	if (det > -1e-12 && det < 1e-12) det = 1.0;
	double inv = 1.0 / det;
	// normalMatrix[r][c] = cof[r][c]/det; column-major out[c*3+r].
	for (int c = 0; c < 3; c++)
		for (int r = 0; r < 3; r++)
			out[c * 3 + r] = (float)(cof[r][c] * inv);
}
} // namespace

void Shader3D_BeginVNC(const void* base, int stride, int posOff, int normOff, int colOff) {
	if (!CoreShaders.ready) return;
	const TShaderProgram& p = CoreShaders.shader3d;
	p.Use();
	cacheLocations3D(p);

	// Snapshot the live fixed-function state, using the current modelview
	// directly (terrain is drawn in world space, model = identity).
	TMatrix<4, 4> projM, mvM;
	glMatrixToTMatrix(GL_PROJECTION_MATRIX, projM);
	glMatrixToTMatrix(GL_MODELVIEW_MATRIX, mvM);
	uploadMatrices3D(projM, mvM);
	snapEnv3D();

	// Point the generic attribs into the interleaved buffer.
	const GLubyte* b = static_cast<const GLubyte*>(base);
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, GL_FLOAT, GL_FALSE, stride, b + posOff);
	pglEnableVertexAttribArray(a3d.normal);
	pglVertexAttribPointer(a3d.normal, 3, GL_FLOAT, GL_FALSE, stride, b + normOff);
	pglEnableVertexAttribArray(a3d.color);
	pglVertexAttribPointer(a3d.color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, b + colOff);
}

void Shader3D_SyncFog() {
	if (!CoreShaders.ready || a3d.useFog < 0) return;
	pglUniform1i(a3d.useFog, glIsEnabled(GL_FOG) ? 1 : 0);
}

void Shader3D_DrawElementsU32(unsigned int count, const unsigned int* indices) {
	if (!CoreShaders.ready || a3d.pos < 0) return;
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, indices);
}

// --- object path (billboards): base proj/view captured once, per-object model ---
void Shader3D_Begin3D() {
	if (!CoreShaders.ready) return;
	const TShaderProgram& p = CoreShaders.shader3d;
	p.Use();
	cacheLocations3D(p);
	glMatrixToTMatrix(GL_PROJECTION_MATRIX, g3d_proj);
	glMatrixToTMatrix(GL_MODELVIEW_MATRIX, g3d_view);
	snapEnv3D();
}

void Shader3D_SetModel3D(const TMatrix<4, 4>& model) {
	if (!CoreShaders.ready) return;
	uploadMatrices3D(g3d_proj, g3d_view * model); // modelview = view * model
}

// Bind a per-object billboard: float xyz positions, short st texcoords (tight),
// and a constant normal (trees/items provide one normal, not an array).
void Shader3D_SetObjectArrays(const float* pos, const GLshort* tex,
                              float nx, float ny, float nz) {
	if (!CoreShaders.ready) return;
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, GL_FLOAT, GL_FALSE, 0, pos);
	pglEnableVertexAttribArray(a3d.texcoord);
	pglVertexAttribPointer(a3d.texcoord, 2, GL_SHORT, GL_FALSE, 0, tex);
	if (a3d.color >= 0)  pglDisableVertexAttribArray(a3d.color);   // material via uniform
	if (a3d.normal >= 0) {
		pglDisableVertexAttribArray(a3d.normal);
		pglVertexAttrib3f(a3d.normal, nx, ny, nz);
	}
}

// Draw nQuads consecutive quads (4 verts each) as triangles.
void Shader3D_DrawQuadArray(int nQuads) {
	if (!CoreShaders.ready || a3d.pos < 0 || nQuads <= 0) return;
	std::vector<GLushort> idx;
	idx.reserve(nQuads * 6);
	for (int q = 0; q < nQuads; q++) {
		GLushort b = (GLushort)(q * 4);
		idx.push_back(b); idx.push_back(b + 1); idx.push_back(b + 2);
		idx.push_back(b); idx.push_back(b + 2); idx.push_back(b + 3);
	}
	glDrawElements(GL_TRIANGLES, nQuads * 6, GL_UNSIGNED_SHORT, idx.data());
}

// Unlit textured primitive (skybox side): position array + texcoord array +
// a constant vertex colour (no per-vertex colour / normal). posType lets the
// caller pass GL_SHORT positions.
void Shader3D_SetTexturedArray(const void* pos, unsigned int posType, const float* tex,
                               const sf::Color& col) {
	if (!CoreShaders.ready) return;
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, posType, GL_FALSE, 0, pos);
	pglEnableVertexAttribArray(a3d.texcoord);
	pglVertexAttribPointer(a3d.texcoord, 2, GL_FLOAT, GL_FALSE, 0, tex);
	if (a3d.normal >= 0) pglDisableVertexAttribArray(a3d.normal);
	if (a3d.color >= 0) {
		pglDisableVertexAttribArray(a3d.color);
		pglVertexAttrib4f(a3d.color, col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);
	}
}

// Unlit coloured primitive (fog plane): position array (3 float) + colour array
// (4 unsigned byte, normalised), no texcoord / normal.
void Shader3D_SetColoredArray(const float* pos, const unsigned char* color) {
	if (!CoreShaders.ready) return;
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, GL_FLOAT, GL_FALSE, 0, pos);
	pglEnableVertexAttribArray(a3d.color);
	pglVertexAttribPointer(a3d.color, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, color);
	if (a3d.normal >= 0)   pglDisableVertexAttribArray(a3d.normal);
	if (a3d.texcoord >= 0) pglDisableVertexAttribArray(a3d.texcoord);
}

// Unlit primitive with a constant colour. Used by the projected Tux shadow:
// positions are already in world space and TUX_SHADOW has lighting/texture off.
void Shader3D_SetPositionColorArray(const float* pos, const sf::Color& col) {
	if (!CoreShaders.ready) return;
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, GL_FLOAT, GL_FALSE, 0, pos);
	if (a3d.normal >= 0)   pglDisableVertexAttribArray(a3d.normal);
	if (a3d.texcoord >= 0) pglDisableVertexAttribArray(a3d.texcoord);
	if (a3d.color >= 0) {
		pglDisableVertexAttribArray(a3d.color);
		pglVertexAttrib4f(a3d.color, col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);
	}
}

void Shader3D_DrawArrays(unsigned int mode, int count) {
	if (!CoreShaders.ready || a3d.pos < 0) return;
	glDrawArrays(mode, 0, count);
}

// Per-object material (Tux nodes): set ambient+diffuse (= diffuse, as
// GL_AMBIENT_AND_DIFFUSE does) and specular/shininess. Used when color-material
// is off (lit objects with an explicit material).
void Shader3D_SetMaterial(const sf::Color& diffuse, const sf::Color& specular, float shininess) {
	if (!CoreShaders.ready) return;
	float d[4] = { diffuse.r / 255.f, diffuse.g / 255.f, diffuse.b / 255.f, diffuse.a / 255.f };
	float s[4] = { specular.r / 255.f, specular.g / 255.f, specular.b / 255.f, specular.a / 255.f };
	pglUniform4fv(a3d.matAmbient, 1, d);
	pglUniform4fv(a3d.matDiffuse, 1, d);
	pglUniform4fv(a3d.matSpecular, 1, s);
	pglUniform1f(a3d.shininess, shininess);
}

// Lit untextured mesh (Tux sphere): separate position + normal arrays (both
// tight float3; for a unit sphere the caller passes the same buffer for both).
void Shader3D_SetPosNormalArrays(const float* pos, const float* normal) {
	if (!CoreShaders.ready) return;
	pglEnableVertexAttribArray(a3d.pos);
	pglVertexAttribPointer(a3d.pos, 3, GL_FLOAT, GL_FALSE, 0, pos);
	pglEnableVertexAttribArray(a3d.normal);
	pglVertexAttribPointer(a3d.normal, 3, GL_FLOAT, GL_FALSE, 0, normal);
	if (a3d.color >= 0)    pglDisableVertexAttribArray(a3d.color);
	if (a3d.texcoord >= 0) pglDisableVertexAttribArray(a3d.texcoord);
}

void Shader3D_DrawElementsU16(unsigned int count, const unsigned short* indices) {
	if (!CoreShaders.ready || a3d.pos < 0) return;
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices);
}

void Shader3D_End() {
	if (!CoreShaders.ready) return;
	if (a3d.pos >= 0)      pglDisableVertexAttribArray(a3d.pos);
	if (a3d.normal >= 0)   pglDisableVertexAttribArray(a3d.normal);
	if (a3d.color >= 0)    pglDisableVertexAttribArray(a3d.color);
	if (a3d.texcoord >= 0) pglDisableVertexAttribArray(a3d.texcoord);
	if (pglUseProgram) pglUseProgram(0);
}
