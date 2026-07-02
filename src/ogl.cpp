/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
Copyright (C) 2010 Extreme Tux Racer Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
---------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "ogl.h"
#include "glmatrix.h"
#include "spx.h"
#include "winsys.h"
#include <stack>
#include <climits> // INT_MAX

static const struct {
	const char* name;
	GLenum value;
	GLenum type;
} gl_values[] = {
	// GLES2 has no fixed-function lights, matrix stacks, or a queryable
	// double-buffer flag (EGL owns buffering), so those are not listed.
	{ "max texture size", GL_MAX_TEXTURE_SIZE, GL_INT },
	{ "red bits", GL_RED_BITS, GL_INT },
	{ "green bits", GL_GREEN_BITS, GL_INT },
	{ "blue bits", GL_BLUE_BITS, GL_INT },
	{ "alpha bits", GL_ALPHA_BITS, GL_INT },
	{ "depth bits", GL_DEPTH_BITS, GL_INT },
	{ "stencil bits", GL_STENCIL_BITS, GL_INT }
};

static const char* gl_error_string(GLenum error) {
	switch (error) {
		case GL_NO_ERROR: return "no error";
		case GL_INVALID_ENUM: return "invalid enum";
		case GL_INVALID_VALUE: return "invalid value";
		case GL_INVALID_OPERATION: return "invalid operation";
		case GL_OUT_OF_MEMORY: return "out of memory";
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "invalid framebuffer operation";
#endif
#ifdef GL_STACK_OVERFLOW
		case GL_STACK_OVERFLOW: return "stack overflow";
#endif
#ifdef GL_STACK_UNDERFLOW
		case GL_STACK_UNDERFLOW: return "stack underflow";
#endif
		default: return "unknown error";
	}
}

static void copy4(float dst[4], const float src[4]) {
	for (int i = 0; i < 4; i++)
		dst[i] = src[i];
}

static TRenderState make_default_render_state() {
	TRenderState state;
	static const float globalAmbient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
	static const float lightPosition[4] = {0.0f, 0.0f, 1.0f, 0.0f};
	static const float lightAmbient[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	static const float lightDiffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	static const float lightSpecular[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	static const float matAmbient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
	static const float matDiffuse[4] = {0.8f, 0.8f, 0.8f, 1.0f};
	static const float matSpecular[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	static const float texGenS[4] = {1.0f, 0.0f, 0.0f, 0.0f};
	static const float texGenT[4] = {0.0f, 1.0f, 0.0f, 0.0f};
	static const float fogColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	copy4(state.globalAmbient, globalAmbient);
	copy4(state.lightPosition, lightPosition);
	copy4(state.lightAmbient, lightAmbient);
	copy4(state.lightDiffuse, lightDiffuse);
	copy4(state.lightSpecular, lightSpecular);
	copy4(state.matAmbient, matAmbient);
	copy4(state.matDiffuse, matDiffuse);
	copy4(state.matSpecular, matSpecular);
	copy4(state.texGenS, texGenS);
	copy4(state.texGenT, texGenT);
	copy4(state.fogColor, fogColor);
	state.projection.SetIdentity();
	state.modelview.SetIdentity();
	return state;
}

static TRenderState renderState = make_default_render_state();

static void reset_render_mode_state() {
	renderState.texture2d = false;
	renderState.lighting = false;
	renderState.colorMaterial = false;
	renderState.texGen = false;
	renderState.alphaTest = false;
	renderState.alphaRef = 0.0f;
}

static void transform_light_position(const float in[4], float out[4]) {
	for (int r = 0; r < 4; r++) {
		out[r] = 0.0f;
		for (int c = 0; c < 4; c++)
			out[r] += static_cast<float>(renderState.modelview[c][r]) * in[c];
	}
}

void check_gl_error() {
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		Message("OpenGL Error: ", gl_error_string(error));
	}
}

#ifndef __ANDROID__
// GL_EXT_compiled_vertex_array is a desktop-GL optimization absent from GLES2.
// The lock/unlock pointers were already unused after the GLES rewrite; skip on Android.
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT_p = nullptr;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT_p = nullptr;

void InitOpenglExtensions() {
	glLockArraysEXT_p = (PFNGLLOCKARRAYSEXTPROC)sf::Context::getFunction("glLockArraysEXT");
	glUnlockArraysEXT_p = (PFNGLUNLOCKARRAYSEXTPROC)sf::Context::getFunction("glUnlockArraysEXT");

	if (glLockArraysEXT_p == nullptr || glUnlockArraysEXT_p == nullptr) {
		Message("GL_EXT_compiled_vertex_array extension NOT supported");
		glLockArraysEXT_p = nullptr;
		glUnlockArraysEXT_p = nullptr;
	}
}
#else
void InitOpenglExtensions() {}
#endif

void PrintGLInfo() {
	Message("Gl vendor: ", (char*)glGetString(GL_VENDOR));
	Message("Gl renderer: ", (char*)glGetString(GL_RENDERER));
	Message("Gl version: ", (char*)glGetString(GL_VERSION));
	std::string extensions = (char*)glGetString(GL_EXTENSIONS);
	Message("");
	Message("Gl extensions:");
	Message("");

	std::size_t oldpos = 0;
	std::size_t pos;
	while ((pos = extensions.find(' ', oldpos)) != std::string::npos) {
		std::string s = extensions.substr(oldpos, pos-oldpos);
		Message(s);
		oldpos = pos+1;
	}
	Message(extensions.substr(oldpos));

	Message("");
	for (int i=0; i<(int)(sizeof(gl_values)/sizeof(gl_values[0])); i++) {
		switch (gl_values[i].type) {
			case GL_INT: {
				GLint int_val;
				glGetIntegerv(gl_values[i].value, &int_val);
				std::string ss = Int_StrN(int_val);
				Message(gl_values[i].name, ss);
				break;
			}
			case GL_FLOAT: {
				GLfloat float_val;
				glGetFloatv(gl_values[i].value, &float_val);
				std::string ss = Float_StrN(float_val, 2);
				Message(gl_values[i].name, ss);
				break;
			}
			case GL_UNSIGNED_BYTE: {
				GLboolean boolean_val;
				glGetBooleanv(gl_values[i].value, &boolean_val);
				std::string ss = Int_StrN(boolean_val);
				Message(gl_values[i].name, ss);
				break;
			}
			default:
				Message("");
		}
	}
}

void set_material_diffuse(const sf::Color& diffuse_colour) {
	const float scale = 1.0 / 255.0;
	GLfloat mat_amb_diff[4] = {
		diffuse_colour.r * scale,
		diffuse_colour.g * scale,
		diffuse_colour.b * scale,
		diffuse_colour.a * scale
	};
	copy4(renderState.matAmbient, mat_amb_diff);
	copy4(renderState.matDiffuse, mat_amb_diff);
}

void set_material(const sf::Color& diffuse_colour, const sf::Color& specular_colour, float specular_exp) {
	set_material_diffuse(diffuse_colour);

	const float scale = 1.0 / 255.0;
	GLfloat mat_specular[4] = {
		specular_colour.r * scale,
		specular_colour.g * scale,
		specular_colour.b * scale,
		specular_colour.a * scale
	};
	copy4(renderState.matSpecular, mat_specular);
	renderState.shininess = specular_exp;
}

void ClearRenderContext() {
	glDepthMask(GL_TRUE);
	glClearColor(colBackgr.r / 255.f, colBackgr.g / 255.f, colBackgr.b / 255.f, colBackgr.a / 255.f);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void ClearRenderContext(const sf::Color& col) {
	glDepthMask(GL_TRUE);
	glClearColor(col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Setup2dScene() {
	// M6: converted 2D draw paths set their own shader ortho matrix. This
	// function remains as a compatibility marker for legacy call sites.
}

void Reshape(int w, int h) {
	glViewport(0, 0, (GLint) w, (GLint) h);
	double far_clip_dist = param.forward_clip_distance + FAR_CLIP_FUDGE_AMOUNT;
	renderState.projection = MakePerspective(param.fov, (double)w / h, NEAR_CLIP_DIST, far_clip_dist);
}
// ====================================================================
//					GL options
// ====================================================================

static TRenderMode currentMode = RM_UNINITIALIZED;

const TRenderState& RenderStateSnapshot() {
	return renderState;
}

void RenderSetLight(int num, const float* position, const float* ambient,
                    const float* diffuse, const float* specular) {
	if (num != 0)
		return;
	transform_light_position(position, renderState.lightPosition);
	copy4(renderState.lightAmbient, ambient);
	copy4(renderState.lightDiffuse, diffuse);
	copy4(renderState.lightSpecular, specular);
}

void RenderSetFog(bool enabled, float start, float end, const float* color) {
	renderState.fog = enabled;
	renderState.fogStart = start;
	renderState.fogEnd = end;
	copy4(renderState.fogColor, color);
}

void RenderSetFogEnabled(bool enabled) {
	renderState.fog = enabled;
}

void RenderSetTexGenPlanes(const float* s, const float* t) {
	copy4(renderState.texGenS, s);
	copy4(renderState.texGenT, t);
}

void ResetRenderMode() {
	if (currentMode == GUI)
		Winsys.endSFML();

	currentMode = RM_UNINITIALIZED;
	reset_render_mode_state();
}

void set_gl_options(TRenderMode mode) {
	if (currentMode == GUI)
		Winsys.endSFML();

	currentMode = mode;
	reset_render_mode_state();
	switch (mode) {
		case GUI:
			Winsys.beginSFML();
			break;

		// GLES2: lighting / texturing / texgen / color-material live in the
		// tracked renderState (read by the shader in glshader.cpp); only the
		// still-valid GLES2 fixed state (depth / cull / blend / stencil) is set
		// on the GL context here.
		case GAUGE_BARS:
			renderState.texture2d = true;
			renderState.texGen = true;
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case TEXFONT:
			renderState.texture2d = true;
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case COURSE:
			renderState.texture2d = true;
			renderState.lighting = true;
			renderState.colorMaterial = true;
			renderState.texGen = true;
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LEQUAL);
			break;

		case TREES:
			renderState.texture2d = true;
			renderState.lighting = true;
			renderState.alphaTest = true;
			renderState.alphaRef = 0.5f;
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case PARTICLES:
			renderState.texture2d = true;
			renderState.alphaTest = true;
			renderState.alphaRef = 0.5f;
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case SKY:
			renderState.texture2d = true;
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LESS);
			break;

		case FOG_PLANE:
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case TUX:
			renderState.lighting = true;
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			break;

		case TUX_SHADOW:
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glDepthFunc(GL_LESS);
#ifdef USE_STENCIL_BUFFER
			glDisable(GL_CULL_FACE);
			glEnable(GL_STENCIL_TEST);
			glDepthMask(GL_FALSE);

			glStencilFunc(GL_EQUAL, 0, ~0U);
			glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
#else
			glEnable(GL_CULL_FACE);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_TRUE);
#endif
			break;

		case TRACK_MARKS:
			renderState.texture2d = true;
			renderState.lighting = true;
			renderState.colorMaterial = true;
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glDisable(GL_STENCIL_TEST);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);
			break;

		default:
			Message("not a valid render mode");
	}
}

static std::stack<TRenderMode> modestack;
void PushRenderMode(TRenderMode mode) {
	if (currentMode != mode)
		set_gl_options(mode);
	modestack.push(mode);
}

void PopRenderMode() {
	TRenderMode mode = modestack.top();
	modestack.pop();
	if (!modestack.empty() && modestack.top() != mode)
		set_gl_options(modestack.top());
}


void glLoadMatrix(const TMatrix<4, 4>& mat) {
	renderState.modelview = mat;
}

void glMultMatrix(const TMatrix<4, 4>& mat) {
	renderState.modelview = renderState.modelview * mat;
}
