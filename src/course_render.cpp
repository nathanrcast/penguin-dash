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

#include "textures.h"
#include "course_render.h"
#include "course.h"
#include "ogl.h"
#include "glshader.h"
#include "quadtree.h"
#include "particles.h"
#include "env.h"
#include "game_ctrl.h"
#include "physics.h"

#define TEX_SCALE 6
static const bool clip_course = true;

void setup_course_tex_gen() {
	static const GLfloat xplane[4] = {1.f / TEX_SCALE, 0.f, 0.f, 0.f };
	static const GLfloat zplane[4] = {0.f, 0.f, 1.f / TEX_SCALE, 0.f };
	RenderSetTexGenPlanes(xplane, zplane);
}

void RenderCourse() {
	ScopedRenderMode rm(COURSE);
	setup_course_tex_gen();
	set_material(colWhite, colBlack, 1.0);
	const CControl *ctrl = g_game.player->ctrl;
	UpdateQuadtree(ctrl->viewpos, param.course_detail_level);
	RenderQuadtree();
}

void DrawTrees() {
	std::size_t tree_type = -1;
	const CControl*	ctrl = g_game.player->ctrl;

	ScopedRenderMode rm(TREES);
	double fwd_clip_limit = param.forward_clip_distance;
	double bwd_clip_limit = param.backward_clip_distance;

	set_material(colWhite, colBlack, 1.0);

	// GLES2: billboards through the 3D shader. Snapshot env + base proj/view
	// once; the constant 1° y-rotation (perf_level > 1) is folded into the
	// shared model base, so each object costs only a translation upload —
	// no per-tree matrix multiply or normal-matrix inverse. The TREES render
	// mode's alpha test is reproduced as a discard in the shader.
	Shader3D_Begin3D();

	if (param.perf_level > 1) {
		TMatrix<4, 4> rot;
		rot.SetRotationMatrix(1, 'y');
		Shader3D_SetModelRotation(rot);
	}

	// Trees
	for (std::size_t i = 0; i< Course.CollArr.size(); i++) {
		if (clip_course) {
			if (ctrl->viewpos.z - Course.CollArr[i].pt.z > fwd_clip_limit) continue;
			if (Course.CollArr[i].pt.z - ctrl->viewpos.z > bwd_clip_limit) continue;
		}

		if (Course.CollArr[i].tree_type != tree_type) {
			tree_type = Course.CollArr[i].tree_type;
			Course.ObjTypes[tree_type].texture->Bind();
		}

		Shader3D_SetModelTranslation(Course.CollArr[i].pt.x, Course.CollArr[i].pt.y, Course.CollArr[i].pt.z);

		float treeRadius = Course.CollArr[i].diam / 2.0;
		float treeHeight = Course.CollArr[i].height;

		static const GLshort tex[] = {
			0, 1,
			1, 1,
			1, 0,
			0, 0,
			0, 1,
			1, 1,
			1, 0,
			0, 0
		};

		const GLfloat vtx[] = {
			-treeRadius, 0.0,        0.0,
			    treeRadius,  0.0,        0.0,
			    treeRadius,  treeHeight, 0.0,
			    -treeRadius, treeHeight, 0.0,
			    0.0,         0.0,        -treeRadius,
			    0.0,         0.0,        treeRadius,
			    0.0,         treeHeight, treeRadius,
			    0.0,         treeHeight, -treeRadius
		    };

		Shader3D_SetObjectArrays(vtx, tex, 0.f, 0.f, 1.f);
		Shader3D_DrawQuadArray(2); // 8 verts = 2 crossed quads
	}

	// Items (translation only: reset the shared base if the trees rotated it)
	if (param.perf_level > 1)
		Shader3D_SetModelRotation(TMatrix<4, 4>::getIdentity());

	const TObjectType* item_type = nullptr;

	for (std::size_t i = 0; i< Course.NocollArr.size(); i++) {
		if (Course.NocollArr[i].collectable == 0 || Course.NocollArr[i].type.drawable == false) continue;
		if (clip_course) {
			if (ctrl->viewpos.z - Course.NocollArr[i].pt.z > fwd_clip_limit) continue;
			if (Course.NocollArr[i].pt.z - ctrl->viewpos.z > bwd_clip_limit) continue;
		}

		if (&Course.NocollArr[i].type != item_type) {
			item_type = &Course.NocollArr[i].type;
			item_type->texture->Bind();
		}

		Shader3D_SetModelTranslation(Course.NocollArr[i].pt.x, Course.NocollArr[i].pt.y, Course.NocollArr[i].pt.z);

		double itemRadius = Course.NocollArr[i].diam / 2;
		double itemHeight = Course.NocollArr[i].height;

		TVector3d normal;
		if (item_type->use_normal) {
			normal = item_type->normal;
		} else {
			normal = ctrl->viewpos - Course.NocollArr[i].pt;
			normal.Norm();
		}
		// full normal drives lighting; a flattened copy orients the billboard
		float lnx = normal.x, lny = normal.y, lnz = normal.z;
		normal.y = 0.0;
		normal.Norm();

		static const GLshort tex[] = {
			0, 1,
			1, 1,
			1, 0,
			0, 0
		};

		const GLfloat vtx[] = {
			static_cast<GLfloat>(-itemRadius*normal.z),
			0.f,
			static_cast<GLfloat>(itemRadius*normal.x),

			static_cast<GLfloat>(itemRadius*normal.z),
			0.f,
			static_cast<GLfloat>(-itemRadius*normal.x),
			static_cast<GLfloat>(itemRadius*normal.z),
			static_cast<GLfloat>(itemHeight),
			static_cast<GLfloat>(-itemRadius*normal.x),
			static_cast<GLfloat>(-itemRadius*normal.z),
			static_cast<GLfloat>(itemHeight),
			static_cast<GLfloat>(itemRadius*normal.x)
		};

		Shader3D_SetObjectArrays(vtx, tex, lnx, lny, lnz);
		Shader3D_DrawQuadArray(1); // 4 verts = 1 quad
	}

	Shader3D_End();
}
