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

#include "env.h"
#include "ogl.h"
#include "glshader.h"
#include "textures.h"
#include "spx.h"
#include "view.h"
#include "course.h"

// --------------------------------------------------------------------
//					defaults
// --------------------------------------------------------------------

static const float def_amb[]    = {0.45f, 0.53f, 0.75f, 1.f};
static const float def_diff[]   = {1.f,   0.9f,  1.f,   1.f};
static const float def_spec[]   = {0.6f,  0.6f,  0.6f,  1.f};
static const float def_pos[]    = {1.f,   2.f,   2.f,   0.f};
static const float def_fogcol[] = {0.9f,  0.9f,  1.f,   0.f};
static const sf::Color def_partcol(204, 204, 230, 0);

void TLight::Enable(GLenum num) const {
	glLightfv(num, GL_POSITION, position);
	glLightfv(num, GL_AMBIENT, ambient);
	glLightfv(num, GL_DIFFUSE, diffuse);
	glLightfv(num, GL_SPECULAR, specular);
	glEnable(num);
}

CEnvironment Env;

CEnvironment::CEnvironment()
	: lightcond{"sunny", "cloudy", "evening", "night"} {
	EnvID = -1;
	for (std::size_t i = 0; i < 4; i++)
		LightIndex[lightcond[i]] = i;
	Skybox = nullptr;

	default_light.is_on = true;
	for (int i=0; i<4; i++) {
		default_light.ambient[i]  = def_amb[i];
		default_light.diffuse[i]  = def_diff[i];
		default_light.specular[i] = def_spec[i];
		default_light.position[i] = def_pos[i];
	}

	default_fog.is_on = true;
	default_fog.mode = GL_LINEAR;
	default_fog.start = 20.0;
	default_fog.end = 70.0;
	default_fog.height = 0;
	for (int i=0; i<4; i++)
		default_fog.color[i] = def_fogcol[i];
	default_fog.part_color = def_partcol;
}

void CEnvironment::ResetSkybox() {
	delete[] Skybox;
	Skybox = nullptr;
}

void CEnvironment::SetupLight() {
	lights[0].Enable(GL_LIGHT0);
	if (lights[1].is_on)
		lights[1].Enable(GL_LIGHT1);
	if (lights[2].is_on)
		lights[2].Enable(GL_LIGHT2);
	if (lights[3].is_on)
		lights[3].Enable(GL_LIGHT3);
}

void CEnvironment::SetupFog() {
	glEnable(GL_FOG);
	glFogi(GL_FOG_MODE, fog.mode);
	glFogf(GL_FOG_START, fog.start);
	glFogf(GL_FOG_END, fog.end);
	glFogfv(GL_FOG_COLOR, fog.color);

	if (param.perf_level > 1) {
		glHint(GL_FOG_HINT, GL_NICEST);
	} else {
		glHint(GL_FOG_HINT, GL_FASTEST);
	}
}

void CEnvironment::ResetLight() {
	lights[0] = default_light;
	for (int i=1; i<4; i++) lights[i].is_on = false;
	glDisable(GL_LIGHT1);
	glDisable(GL_LIGHT2);
	glDisable(GL_LIGHT3);
}

void CEnvironment::ResetFog() {
	fog = default_fog;
}

void CEnvironment::Reset() {
	EnvID = -1;
	ResetSkybox();
	ResetLight();
	ResetFog();
}

bool CEnvironment::LoadEnvironmentList() {
	CSPList list(true);
	if (!list.Load(param.env_dir2, "environment.lst")) {
		Message("could not load environment.lst");
		return false;
	}

	locs.resize(list.size());
	std::size_t i = 0;
	for (CSPList::const_iterator line = list.cbegin(); line != list.cend(); ++line, i++) {
		locs[i].name = SPStrN(*line, "location");
		locs[i].high_res = SPBoolN(*line, "high_res", false);
	}
	list.MakeIndex(EnvIndex, "location");
	return true;
}

std::string CEnvironment::GetDir(std::size_t location, std::size_t light) const {
	if (location >= locs.size()) return "";
	if (light >= 4) return "";
	std::string res =
	    param.env_dir2 + SEP +
	    locs[location].name + SEP + lightcond[light];
	return res;
}

void CEnvironment::LoadSkyboxSide(std::size_t index, const std::string& EnvDir, const std::string& name, bool high_res) {
	bool loaded = false;
	if (param.perf_level > 3 && high_res)
		loaded = Skybox[index].Load(EnvDir, name + "H.png");

	if (!loaded)
		Skybox[index].Load(EnvDir, name + ".png");
}

void CEnvironment::LoadSkybox(const std::string& EnvDir, bool high_res) {
	Skybox = new TTexture[param.full_skybox ? 6 : 3];
	LoadSkyboxSide(0, EnvDir, "front", high_res);
	LoadSkyboxSide(1, EnvDir, "left", high_res);
	LoadSkyboxSide(2, EnvDir, "right", high_res);
	if (param.full_skybox) {
		LoadSkyboxSide(3, EnvDir, "top", high_res);
		LoadSkyboxSide(4, EnvDir, "bottom", high_res);
		LoadSkyboxSide(5, EnvDir, "back", high_res);
	}
}

void CEnvironment::LoadLight(const std::string& EnvDir) {
	static const std::string idxstr = "[fog]-1[0]0[1]1[2]2[3]3[4]4[5]5[6]6";

	CSPList list;
	if (!list.Load(EnvDir, "light.lst")) {
		Message("could not load light file");
		return;
	}

	for (CSPList::const_iterator line = list.cbegin(); line != list.cend(); ++line) {
		std::string item = SPStrN(*line, "light", "none");
		int idx = SPIntN(idxstr, item, -1);
		if (idx < 0) {
			fog.is_on = SPBoolN(*line, "fog", true);
			fog.start = SPFloatN(*line, "fogstart", 20);
			fog.end = SPFloatN(*line, "fogend", param.forward_clip_distance);
			fog.height = SPFloatN(*line, "fogheight", 0);
			SPArrN(*line, "fogcol", fog.color, 4, 1);
			fog.part_color = SPColorN(*line, "partcol", def_partcol);
		} else if (idx < 4) {
			lights[idx].is_on = true;
			SPArrN(*line, "amb", lights[idx].ambient, 4, 1);
			SPArrN(*line, "diff", lights[idx].diffuse, 4, 1);
			SPArrN(*line, "spec", lights[idx].specular, 4, 1);
			SPArrN(*line, "pos", lights[idx].position, 4, 1);
		}
	}
}

void CEnvironment::DrawSkybox(const TVector3d& pos) const {
	ScopedRenderMode rm(SKY);

#if defined (OS_LINUX)
	static const float aa = 0.0f;
	static const float bb = 1.0f;
#else
	static const float aa = 0.005f;
	static const float bb = 0.995f;
#endif

	// GLES2: unlit textured quads through the 3D shader. DECAL with an opaque
	// skybox texture == modulate by white, which the shader does for a white
	// constant colour. Model transform (translate to camera) replaces
	// glPushMatrix/glTranslate.
	static const GLfloat tex[] = {
		aa, bb,
		bb, bb,
		bb, aa,
		aa, aa
	};
	const sf::Color white(255, 255, 255, 255);

	Shader3D_Begin3D();
	TMatrix<4, 4> model;
	model.SetTranslationMatrix(pos.x, pos.y, pos.z);
	Shader3D_SetModel3D(model);

	// front
	static const GLshort front[] = {
		-1, -1, -1,
		    1, -1, -1,
		    1,  1, -1,
		    -1,  1, -1
	    };
	Skybox[0].Bind();
	Shader3D_SetTexturedArray(front, GL_SHORT, tex, white);
	Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);

	// left
	static const GLshort left[] = {
		-1, -1,  1,
		    -1, -1, -1,
		    -1,  1, -1,
		    -1,  1,  1
	    };
	Skybox[1].Bind();
	Shader3D_SetTexturedArray(left, GL_SHORT, tex, white);
	Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);

	// right
	static const GLshort right[] = {
		1, -1, -1,
		1, -1, 1,
		1,  1, 1,
		1,  1, -1
	};
	Skybox[2].Bind();
	Shader3D_SetTexturedArray(right, GL_SHORT, tex, white);
	Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);

	// normally, the following textures are unvisible
	// see game_config.cpp (param.full_skybox)
	if (param.full_skybox) {
		// top
		static const GLshort top[] = {
			-1, 1, -1,
			    1, 1, -1,
			    1, 1,  1,
			    -1, 1,  1
		    };
		Skybox[3].Bind();
		Shader3D_SetTexturedArray(top, GL_SHORT, tex, white);
		Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);

		// bottom
		static const GLshort bottom[] = {
			-1, -1,  1,
			    1, -1,  1,
			    1, -1, -1,
			    -1, -1, -1
		    };
		Skybox[4].Bind();
		Shader3D_SetTexturedArray(bottom, GL_SHORT, tex, white);
		Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);

		// back
		static const GLshort back[] = {
			1, -1, 1,
			-1, -1, 1,
			-1,  1, 1,
			1,  1, 1
		};
		Skybox[5].Bind();
		Shader3D_SetTexturedArray(back, GL_SHORT, tex, white);
		Shader3D_DrawArrays(GL_TRIANGLE_FAN, 4);
	}
	Shader3D_End();
}

void CEnvironment::DrawFog() const {
	if (!fog.is_on)
		return;

	TPlane bottom_plane, top_plane;
	TVector3d left, right;
	TVector3d topleft, topright;
	TVector3d bottomleft, bottomright;

	// the clipping planes are calculated by view frustum (view.cpp)
	const TPlane& leftclip = get_left_clip_plane();
	const TPlane& rightclip = get_right_clip_plane();
	const TPlane& farclip = get_far_clip_plane();
	const TPlane& bottomclip = get_bottom_clip_plane();

	// --------------- calculate the planes ---------------------------

	double slope = std::tan(ANGLES_TO_RADIANS(Course.GetCourseAngle()));
//	TPlane left_edge_plane = MakePlane (1.0, 0.0, 0.0, 0.0);
//	TPlane right_edge_plane = MakePlane (-1.0, 0.0, 0.0, Course.width);

	bottom_plane.nml = TVector3d(0.0, 1, -slope);
	double height = Course.GetBaseHeight(0);
	bottom_plane.d = -height * bottom_plane.nml.y;

	top_plane.nml = bottom_plane.nml;
	height = Course.GetMaxHeight(0) + fog.height;
	top_plane.d = -height * top_plane.nml.y;


	if (!IntersectPlanes(bottom_plane, farclip, leftclip,  &left)) return;
	if (!IntersectPlanes(bottom_plane, farclip, rightclip, &right)) return;
	if (!IntersectPlanes(top_plane,    farclip, leftclip,  &topleft)) return;
	if (!IntersectPlanes(top_plane,    farclip, rightclip, &topright)) return;
	if (!IntersectPlanes(bottomclip,   farclip, leftclip,  &bottomleft)) return;
	if (!IntersectPlanes(bottomclip,   farclip, rightclip, &bottomright)) return;

	TVector3d leftvec  = topleft - left;
	TVector3d rightvec = topright - right;

	// --------------- draw the fog plane -----------------------------

	ScopedRenderMode rm(FOG_PLANE);
	glEnable(GL_FOG);

	// only the alpha channel is used; RGB comes from the fog colour via the
	// shader's fog term (the plane is unlit black, fogged by eye distance).
	static const GLubyte bottom_dens[4]     = { 0, 0, 0, 255 };
	static const GLubyte top_dens[4]        = { 0, 0, 0, 230 };
	static const GLubyte leftright_dens[4]  = { 0, 0, 0, 77 };
	static const GLubyte top_bottom_dens[4] = { 0, 0, 0, 0 };

	// GLES2: the old GL_QUAD_STRIP becomes a triangle strip with the same
	// vertex order (identical tessellation; FOG_PLANE has cull disabled).
	float pos[30];
	GLubyte col[40];
	auto put = [&](int i, const TVector3d& p, const GLubyte c[4]) {
		pos[i * 3] = p.x; pos[i * 3 + 1] = p.y; pos[i * 3 + 2] = p.z;
		col[i * 4] = c[0]; col[i * 4 + 1] = c[1]; col[i * 4 + 2] = c[2]; col[i * 4 + 3] = c[3];
	};
	put(0, bottomleft, bottom_dens);
	put(1, bottomright, bottom_dens);
	put(2, left, bottom_dens);
	put(3, right, bottom_dens);
	put(4, topleft, top_dens);
	put(5, topright, top_dens);
	put(6, topleft + leftvec, leftright_dens);
	put(7, topright + rightvec, leftright_dens);
	put(8, topleft + 3.0 * leftvec, top_bottom_dens);
	put(9, topright + 3.0 * rightvec, top_bottom_dens);

	Shader3D_Begin3D();
	TMatrix<4, 4> id;
	id.SetIdentity();
	Shader3D_SetModel3D(id); // fog-plane verts are already in world space
	Shader3D_SetColoredArray(pos, col);
	Shader3D_DrawArrays(GL_TRIANGLE_STRIP, 10);
	Shader3D_End();
}


void CEnvironment::LoadEnvironment(std::size_t loc, std::size_t light) {
	if (loc >= locs.size()) loc = 0;
	if (light >= 4) light = 0;
	// remember: with (example) 3 locations and 4 lights there
	// are 12 different environments
	std::size_t env_id = loc * 100 + light;

	if (env_id == EnvID)
		return; // Already loaded
	EnvID = env_id;

	// Set directory. The dir is used several times.
	std::string EnvDir = GetDir(loc, light);

	// Load skybox. If the sky can't be loaded for any reason, the
	// texture id's are set to 0 and the sky will not be drawn.
	// There is no error handling, you see the result on the screen.
	ResetSkybox();
	LoadSkybox(EnvDir, locs[loc].high_res);

	// Load light conditions.
	ResetFog();
	ResetLight();
	LoadLight(EnvDir);
}

std::size_t CEnvironment::GetEnvIdx(const std::string& tag) const {
	return EnvIndex.at(tag);
}

std::size_t CEnvironment::GetLightIdx(const std::string& tag) const {
	return LightIndex.at(tag);
}
