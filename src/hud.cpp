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

#include "hud.h"
#include "ogl.h"
#include "glshader.h"
#include "textures.h"
#include "spx.h"
#include "particles.h"
#include "font.h"
#include "course.h"
#include "physics.h"
#include "winsys.h"
#include "game_ctrl.h"
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef __ANDROID__
#include "native_bridge.h"
#endif

#define GAUGE_IMG_SIZE 128
#define ENERGY_GAUGE_BOTTOM 3.0
#define ENERGY_GAUGE_HEIGHT 103.0
#define ENERGY_GAUGE_CENTER_X 71.0
#define ENERGY_GAUGE_CENTER_Y 55.0
#define GAUGE_WIDTH 128.0
#define SPEEDBAR_OUTER_RADIUS  (ENERGY_GAUGE_CENTER_X)
#define SPEEDBAR_BASE_ANGLE 225
#define SPEEDBAR_MAX_ANGLE 45
#define SPEEDBAR_GREEN_MAX_SPEED  (MAX_PADDLING_SPEED * 3.6)
#define SPEEDBAR_YELLOW_MAX_SPEED 100
#define SPEEDBAR_RED_MAX_SPEED 160
#define SPEEDBAR_GREEN_FRACTION 0.5
#define SPEEDBAR_YELLOW_FRACTION 0.25
#define SPEEDBAR_RED_FRACTION 0.25
#define CIRCLE_DIVISIONS 10

static const GLubyte energy_background_color[]   = { 51,  51,  51, 0 };
static const GLubyte energy_foreground_color[]   = { 138, 150, 255, 128 };
static const GLubyte speedbar_background_color[] = { 51,  51,  51, 0 };

static void draw_time(double time, sf::Color color) {
	Tex.Draw(T_TIME, 10, 10, 1);

	int min, sec, hundr;
	GetTimeComponents(time, &min, &sec, &hundr);
	std::string timestr = Int_StrN(min, 2);
	std::string secstr = Int_StrN(sec, 2);
	std::string hundrstr = Int_StrN(hundr, 2);

	timestr += ':';
	timestr += secstr;

	if (param.use_papercut_font < 2) {
		Tex.DrawNumStr(timestr, 50, 12, 1, color);
		Tex.DrawNumStr(hundrstr, 170, 12, 0.7f, color);
	} else {
		Winsys.beginSFML();
		FT.SetColor(color);
		FT.SetSize(30);
		FT.DrawString(138, 3, hundrstr);
		FT.SetSize(42);
		FT.DrawString(53, 3, timestr);
		Winsys.endSFML();
	}
}

static void draw_herring_count(int herring_count, sf::Color color) {
	Tex.Draw(HERRING_ICON, Winsys.resolution.width - 59, 12, 1);

	std::string hcountstr = Int_StrN(herring_count, 3);
	if (param.use_papercut_font < 2) {
		Tex.DrawNumStr(hcountstr, Winsys.resolution.width - 130, 12, 1, color);
	} else {
		Winsys.beginSFML();
		FT.SetColor(color);
		FT.DrawString(Winsys.resolution.width - 125, 3, hcountstr);
		Winsys.endSFML();
	}
}

TVector2d calc_new_fan_pt(double angle) {
	return TVector2d(
	           ENERGY_GAUGE_CENTER_X + std::cos(ANGLES_TO_RADIANS(angle)) * SPEEDBAR_OUTER_RADIUS,
	           ENERGY_GAUGE_CENTER_Y + std::sin(ANGLES_TO_RADIANS(angle)) * SPEEDBAR_OUTER_RADIUS);
}

// GLES2: build the fan as a vertex array through the 2D shader. Texcoords
// reproduce the old glTexGen object planes (s = x/128, t = y/128). Caller must
// have an active Shader2D pass with the gauge model transform + speed texture bound.
void draw_partial_tri_fan(double fraction, const sf::Color& col) {
	double angle = SPEEDBAR_BASE_ANGLE +
	               (SPEEDBAR_MAX_ANGLE - SPEEDBAR_BASE_ANGLE) * fraction;

	int divs = (int)((SPEEDBAR_BASE_ANGLE - angle) * CIRCLE_DIVISIONS / 360.0) + 1;
	double cur_angle = SPEEDBAR_BASE_ANGLE;
	double angle_incr = 360.0 / CIRCLE_DIVISIONS;

	std::vector<float> pos, uv;
	auto push = [&](double px, double py) {
		pos.push_back((float)px);
		pos.push_back((float)py);
		uv.push_back((float)(px / GAUGE_IMG_SIZE));
		uv.push_back((float)(py / GAUGE_IMG_SIZE));
	};

	push(ENERGY_GAUGE_CENTER_X, ENERGY_GAUGE_CENTER_Y);

	for (int i=0; i<divs; i++) {
		TVector2d pt = calc_new_fan_pt(cur_angle);
		push(pt.x, pt.y);
		cur_angle -= angle_incr;
	}

	if (cur_angle+angle_incr > angle + EPS) {
		cur_angle = angle;
		TVector2d pt = calc_new_fan_pt(cur_angle);
		push(pt.x, pt.y);
	}

	Shader2D_DrawArrays(GL_TRIANGLE_FAN, pos.data(), uv.data(), (int)(pos.size() / 2), true, col);
}

void draw_gauge(double speed, double energy) {
	ScopedRenderMode rm(GAUGE_BARS);

	if (Tex.GetTexture(GAUGE_ENERGY) == nullptr) return;
	if (Tex.GetTexture(GAUGE_SPEED) == nullptr) return;
	if (Tex.GetTexture(GAUGE_OUTLINE) == nullptr) return;

	// GLES2 path: shader-driven, gauge translated to the bottom-right corner via
	// a model transform (replaces glPushMatrix/glTranslatef). Texcoords are the
	// old glTexGen object planes computed inline (s = x/128, t = y/128).
	Shader2D_Begin(Winsys.resolution.width, Winsys.resolution.height);
	TMatrix<4, 4> model;
	model.SetTranslationMatrix(Winsys.resolution.width - GAUGE_WIDTH, 0, 0);
	Shader2D_SetModel(model);

	Tex.BindTex(GAUGE_ENERGY);
	float y = ENERGY_GAUGE_BOTTOM + energy * ENERGY_GAUGE_HEIGHT;
	const float yt = y / GAUGE_IMG_SIZE;

	const float vtx1 [] = {
		0.f, y,
		GAUGE_IMG_SIZE, y,
		GAUGE_IMG_SIZE, GAUGE_IMG_SIZE,
		0.f, GAUGE_IMG_SIZE
	};
	const float uv1 [] = {
		0.f, yt,
		1.f, yt,
		1.f, 1.f,
		0.f, 1.f
	};
	const float vtx2 [] = {
		0.f, 0.f,
		GAUGE_IMG_SIZE, 0.f,
		GAUGE_IMG_SIZE, y,
		0.f, y
	};
	const float uv2 [] = {
		0.f, 0.f,
		1.f, 0.f,
		1.f, yt,
		0.f, yt
	};

	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx1, uv1, 4, true,
	                    sf::Color(energy_background_color[0], energy_background_color[1], energy_background_color[2], energy_background_color[3]));
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx2, uv2, 4, true,
	                    sf::Color(energy_foreground_color[0], energy_foreground_color[1], energy_foreground_color[2], energy_foreground_color[3]));

	double speedbar_frac = 0.0;

	if (speed > SPEEDBAR_GREEN_MAX_SPEED) {
		speedbar_frac = SPEEDBAR_GREEN_FRACTION;

		if (speed > SPEEDBAR_YELLOW_MAX_SPEED) {
			speedbar_frac += SPEEDBAR_YELLOW_FRACTION;
			if (speed > SPEEDBAR_RED_MAX_SPEED) {
				speedbar_frac += SPEEDBAR_RED_FRACTION;
			} else {
				speedbar_frac += (speed - SPEEDBAR_YELLOW_MAX_SPEED) /
				                 (SPEEDBAR_RED_MAX_SPEED - SPEEDBAR_YELLOW_MAX_SPEED) * SPEEDBAR_RED_FRACTION;
			}
		} else {
			speedbar_frac += (speed - SPEEDBAR_GREEN_MAX_SPEED) /
			                 (SPEEDBAR_YELLOW_MAX_SPEED - SPEEDBAR_GREEN_MAX_SPEED) * SPEEDBAR_YELLOW_FRACTION;
		}
	} else {
		speedbar_frac +=  speed/SPEEDBAR_GREEN_MAX_SPEED * SPEEDBAR_GREEN_FRACTION;
	}

	Tex.BindTex(GAUGE_SPEED);
	draw_partial_tri_fan(1.0, sf::Color(speedbar_background_color[0], speedbar_background_color[1], speedbar_background_color[2], speedbar_background_color[3]));
	draw_partial_tri_fan(std::min(1.0, speedbar_frac), sf::Color::White);

	Tex.BindTex(GAUGE_OUTLINE);
	static const float vtx3 [] = {
		0.f, 0.f,
		GAUGE_IMG_SIZE, 0.f,
		GAUGE_IMG_SIZE, GAUGE_IMG_SIZE,
		0.f, GAUGE_IMG_SIZE
	};
	static const float uv3 [] = {
		0.f, 0.f,
		1.f, 0.f,
		1.f, 1.f,
		0.f, 1.f
	};
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx3, uv3, 4, true, sf::Color::White);

	Shader2D_End();
}

void DrawSpeed(double speed) {
	std::string speedstr = Int_StrN((int)speed, 3);
	if (param.use_papercut_font < 2) {
		Tex.DrawNumStr(speedstr,
		               Winsys.resolution.width - 87, Winsys.resolution.height-73, 1, colWhite);
	} else {
		Winsys.beginSFML();
		FT.SetColor(colDDYell);
		FT.DrawString(Winsys.resolution.width - 82, Winsys.resolution.height - 80, speedstr);
		Winsys.endSFML();
	}
}

void DrawWind(float dir, float speed, const CControl *ctrl) {
	if (g_game.wind_id < 1) return;

	static const int texHeight = Tex.GetSFTexture(SPEEDMETER).getSize().y;
	static const int texWidth = Tex.GetSFTexture(SPEEDMETER).getSize().x;

	Tex.Draw(SPEEDMETER, 5, Winsys.resolution.height-5-texHeight, 1.0);


	float alpha, red, blue;
	if (speed <= 50) {
		alpha = speed / 50;
		red = 0;
	} else {
		alpha = 1.f;
		red = (speed - 50) / 50;
	}
	blue = 1.f - red;

	// GLES2: untextured colored arrows through the 2D shader. Model transform
	// replaces glTranslatef/glRotatef; z-rotations about the same axis compose
	// additively, so the direction indicator uses translate * rotZ(dir_angle).
	Shader2D_Begin(Winsys.resolution.width, Winsys.resolution.height);
	TMatrix<4, 4> trans, rot;
	trans.SetTranslationMatrix(5 + texWidth / 2, 5 + texHeight / 2, 0);

	static const int len = 45;
	static const float vtx1 [] = {
		-5.f, 0.f,
		 5.f, 0.f,
		 5.f, -len,
		-5.f, -len
	};
	rot.SetRotationMatrix(dir, 'z');
	Shader2D_SetModel(trans * rot);
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx1, nullptr, 4, false, sf::Color(255 * red, 0, 255 * blue, 255 * alpha));

	// direction indicator
	float dir_angle = RADIANS_TO_ANGLES(std::atan2(ctrl->cvel.x, ctrl->cvel.z));

	static const float vtx2 [] = {
		-2.f, 0.f,
		 2.f, 0.f,
		 2.f, -50.f,
		-2.f, -50.f
	};
	rot.SetRotationMatrix(dir_angle, 'z');
	Shader2D_SetModel(trans * rot);
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx2, nullptr, 4, false, sf::Color(0, 128, 0, 255));
	Shader2D_End();

	Tex.Draw(SPEED_KNOB, 5 + texWidth / 2 - 8, Winsys.resolution.height - 5 - texWidth / 2 - 8, 1.0);
	std::string windstr = Int_StrN((int)speed, 3);
	if (param.use_papercut_font < 2) {
		Tex.DrawNumStr(windstr, 120, Winsys.resolution.height - 45, 1, colWhite);
	} else {
		Winsys.beginSFML();
		FT.SetColor(colDDYell);
		FT.DrawString(120, Winsys.resolution.height - 50, windstr);
		Winsys.endSFML();
	}
}

void DrawFps() {
	const  int   maxFrames = 50;
	static int   numFrames = 0;
	static float averagefps = 0;
	static float sumTime = 0;

	if (!param.display_fps)
		return;

	if (numFrames >= maxFrames) {
		averagefps = 1 / sumTime * maxFrames;
		numFrames = 0;
		sumTime = 0;
	} else {
		sumTime += g_game.time_step;
		numFrames++;
	}
	if (averagefps < 1) return;

	std::string fpsstr = Int_StrN((int)averagefps);
	if (param.use_papercut_font < 2) {
		Tex.DrawNumStr(fpsstr, (Winsys.resolution.width - 60) / 2, 10, 1, colWhite);
	} else {
		Winsys.beginSFML();
		if (averagefps >= 35)
			FT.SetColor(colWhite);
		else
			FT.SetColor(colRed);
		FT.DrawString(-1, 3, fpsstr);
		Winsys.endSFML();
	}
}

void DrawPercentBar(float fact, float x, float y) {
	Tex.BindTex(T_ENERGY_MASK);

	const float tex[] = {
		0.f, 1.f,
		1.f, 1.f,
		1.f, 1.f - fact,
		0.f, 1.f - fact
	};
	const float vtx[] = {
		x, y,
		x + 32, y,
		x + 32, y + fact * 128,
		x, y + fact * 128
	};

	Shader2D_Begin(Winsys.resolution.width, Winsys.resolution.height);
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, vtx, tex, 4, true, sf::Color::White);
	Shader2D_End();
}

void DrawCoursePosition(const CControl *ctrl) {
	double fact = ctrl->cpos.z / Course.GetPlayDimensions().y;
	if (fact > 1.0) fact = 1.0;
	DrawPercentBar(-fact, Winsys.resolution.width - 48, 280-128);
	Tex.Draw(T_MASK_OUTLINE, Winsys.resolution.width - 48, Winsys.resolution.height - 280, 1.0);
}

#ifdef __ANDROID__
static void DrawAndroidTouchButtonRect(float left, float top, float right, float bottom) {
	const float h = Winsys.resolution.height;
	const float y0 = h - bottom;
	const float y1 = h - top;
	const float rect[] = {
		left,  y0,
		right, y0,
		right, y1,
		left,  y1
	};

	// Dark fill + bright border so pads read against snow.
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, rect, nullptr, 4, false, sf::Color(20, 28, 40, 150));
	Shader2D_DrawArrays(GL_LINE_LOOP, rect, nullptr, 4, false, sf::Color(255, 255, 255, 230));
}

static void DrawAndroidTouchLabel(float left, float top, float width, float height, const char* label) {
	const float h = Winsys.resolution.height;
	FT.SetSize(std::max(22u, static_cast<unsigned int>(h * 0.032f)));
	float textW = FT.GetTextWidth(label);
	float x = left + (width - textW) * 0.5f;
	float y = top + height * 0.38f;
	// Dark shadow then bright label for contrast on snow.
	FT.SetColor(sf::Color(0, 0, 0, 200));
	FT.DrawString(x + 2.f, y + 2.f, label);
	FT.SetColor(sf::Color(255, 255, 255, 245));
	FT.DrawString(x, y, label);
}

// Screen-space (y down) → GL y-up used by Shader2D.
static inline float AndroidGlY(float screenY) {
	return Winsys.resolution.height - screenY;
}

static void DrawAndroidCircle(float cx, float cy, float radius, const sf::Color& fill, const sf::Color& border) {
	constexpr int N = 28;
	// Fan needs the first rim vertex repeated at the end to close the disc
	// (otherwise the last wedge is missing → Pac-Man cutout).
	float glFan[(N + 2) * 2];
	float glRim[N * 2];
	glFan[0] = cx;
	glFan[1] = AndroidGlY(cy);
	for (int i = 0; i < N; i++) {
		float a = static_cast<float>(i) * 6.2831853f / static_cast<float>(N);
		float x = cx + std::cos(a) * radius;
		float y = cy + std::sin(a) * radius;
		glFan[(i + 1) * 2] = x;
		glFan[(i + 1) * 2 + 1] = AndroidGlY(y);
		glRim[i * 2] = x;
		glRim[i * 2 + 1] = AndroidGlY(y);
	}
	glFan[(N + 1) * 2] = glFan[2];
	glFan[(N + 1) * 2 + 1] = glFan[3];
	Shader2D_DrawArrays(GL_TRIANGLE_FAN, glFan, nullptr, N + 2, false, fill);
	Shader2D_DrawArrays(GL_LINE_LOOP, glRim, nullptr, N, false, border);
}

// dir: 0=up, 1=down, 2=left, 3=right. tip points outward from (cx,cy).
static void DrawAndroidArrow(float cx, float cy, float size, int dir, const sf::Color& col) {
	float tipX = cx, tipY = cy, b1x = cx, b1y = cy, b2x = cx, b2y = cy;
	const float half = size * 0.55f;
	switch (dir) {
		case 0: // up
			tipY = cy - size;
			b1x = cx - half; b1y = cy + size * 0.35f;
			b2x = cx + half; b2y = cy + size * 0.35f;
			break;
		case 1: // down
			tipY = cy + size;
			b1x = cx - half; b1y = cy - size * 0.35f;
			b2x = cx + half; b2y = cy - size * 0.35f;
			break;
		case 2: // left
			tipX = cx - size;
			b1x = cx + size * 0.35f; b1y = cy - half;
			b2x = cx + size * 0.35f; b2y = cy + half;
			break;
		default: // right
			tipX = cx + size;
			b1x = cx - size * 0.35f; b1y = cy - half;
			b2x = cx - size * 0.35f; b2y = cy + half;
			break;
	}
	const float tri[] = {
		tipX, tipY,
		b1x, b1y,
		b2x, b2y
	};
	float gl[6];
	for (int i = 0; i < 3; i++) {
		gl[i * 2] = tri[i * 2];
		gl[i * 2 + 1] = AndroidGlY(tri[i * 2 + 1]);
	}
	Shader2D_DrawArrays(GL_TRIANGLES, gl, nullptr, 3, false, col);
}

static void DrawAndroidTiltZones() {
	const float w = Winsys.resolution.width;
	const float h = Winsys.resolution.height;

	const float leftX = 0.f;
	const float leftR = w * 0.30f;
	const float rightL = w * 0.70f;
	const float rightR = w;
	const float top = h * 0.58f;
	const float split = h * 0.78f;
	const float bottom = h;

	Shader2D_Begin(w, h);
	DrawAndroidTouchButtonRect(leftX, top, leftR, bottom);
	DrawAndroidTouchButtonRect(rightL, top, rightR, split);
	DrawAndroidTouchButtonRect(rightL, split, rightR, bottom);
	Shader2D_End();

	Winsys.beginSFML();
	DrawAndroidTouchLabel(leftX, top, leftR - leftX, bottom - top, "BRAKE");
	DrawAndroidTouchLabel(rightL, top, rightR - rightL, split - top, "PADDLE");
	DrawAndroidTouchLabel(rightL, split, rightR - rightL, bottom - split, "JUMP");
	Winsys.endSFML();
}

static void DrawAndroidDpad() {
	const float w = Winsys.resolution.width;
	const float h = Winsys.resolution.height;
	const sf::Color fill(20, 28, 40, 160);
	const sf::Color fillActive(20, 28, 40, 200);
	const sf::Color border(255, 255, 255, 230);
	const sf::Color knobFill(240, 240, 245, 220);
	const sf::Color knobBorder(255, 255, 255, 245);
	const sf::Color arrow(255, 255, 255, 235);

	bool stickActive = false;
	float restNx = 0.15f, restNy = 0.68f;
	float baseNx = restNx, baseNy = restNy;
	float knobNx = restNx, knobNy = restNy;
	pd::GetAndroidStickVisual(stickActive, restNx, restNy, baseNx, baseNy, knobNx, knobNy);

	// Floating stick: rest marker when idle; base + knob follow the finger when held.
	const float cx = w * (stickActive ? baseNx : restNx);
	const float cy = h * (stickActive ? baseNy : restNy);
	const float radius = std::min(w, h) * 0.125f;
	const float knobCx = w * (stickActive ? knobNx : restNx);
	const float knobCy = h * (stickActive ? knobNy : restNy);
	const float knobR = radius * 0.38f;

	const float jumpCx = w * 0.90f;
	const float jumpCy = h * 0.88f;
	const float jumpR = std::min(w, h) * 0.070f;

	Shader2D_Begin(w, h);
	DrawAndroidCircle(cx, cy, radius, stickActive ? fillActive : fill, border);
	if (stickActive)
		DrawAndroidCircle(knobCx, knobCy, knobR, knobFill, knobBorder);
	else {
		const float arrowSize = radius * 0.28f;
		const float arrowOut = radius * 0.48f;
		DrawAndroidArrow(cx, cy - arrowOut, arrowSize, 0, arrow);
		DrawAndroidArrow(cx, cy + arrowOut, arrowSize, 1, arrow);
		DrawAndroidArrow(cx - arrowOut, cy, arrowSize, 2, arrow);
		DrawAndroidArrow(cx + arrowOut, cy, arrowSize, 3, arrow);
	}
	DrawAndroidCircle(jumpCx, jumpCy, jumpR, fill, border);
	Shader2D_End();

	Winsys.beginSFML();
	FT.SetSize(std::max(14u, static_cast<unsigned int>(jumpR * 0.50f)));
	const char* jumpLabel = "JUMP";
	float textW = FT.GetTextWidth(jumpLabel);
	float tx = jumpCx - textW * 0.5f;
	float ty = jumpCy - FT.GetSize() * 0.35f;
	FT.SetColor(sf::Color(0, 0, 0, 200));
	FT.DrawString(tx + 1.5f, ty + 1.5f, jumpLabel);
	FT.SetColor(sf::Color(255, 255, 255, 245));
	FT.DrawString(tx, ty, jumpLabel);
	Winsys.endSFML();
}

static void DrawAndroidTouchControls() {
	const float w = Winsys.resolution.width;
	const float h = Winsys.resolution.height;
	if (w <= 0 || h <= 0) return;

	if (param.control_mode == 1)
		DrawAndroidDpad();
	else
		DrawAndroidTiltZones();
}
#endif

// -------------------------------------------------------
void DrawHud(const CControl *ctrl) {
	if (!param.show_hud) {
#ifdef __ANDROID__
		Setup2dScene();
		DrawAndroidTouchControls();
#endif
		return;
	}

	double speed = ctrl->cvel.Length();
	Setup2dScene();

	draw_gauge(speed * 3.6, ctrl->jump_amt);
	ScopedRenderMode rm(TEXFONT);

	if (g_game.game_type == CUPRACING) {
		if (g_game.time < g_game.race->time.z)
			draw_time(g_game.race->time.z - g_game.time, colGold);
		else if (g_game.time < g_game.race->time.y)
			draw_time(g_game.race->time.y - g_game.time, colSilver);
		else if (g_game.time < g_game.race->time.x)
			draw_time(g_game.race->time.x - g_game.time, colBronze);
		else
			draw_time(g_game.time, colDRed);

		if (g_game.herring < g_game.race->herrings.x)
			draw_herring_count(g_game.race->herrings.x - g_game.herring, colBronze);
		else if (g_game.herring < g_game.race->herrings.y)
			draw_herring_count(g_game.race->herrings.y - g_game.herring, colSilver);
		else if (g_game.herring < g_game.race->herrings.z)
			draw_herring_count(g_game.race->herrings.z - g_game.herring, colGold);
		else
			draw_herring_count(g_game.herring, colGreen);
	} else {
		draw_time(g_game.time, param.use_papercut_font < 2 ? colWhite : colDYell);
		draw_herring_count(g_game.herring, param.use_papercut_font < 2 ? colWhite : colDYell);
	}

	DrawSpeed(speed * 3.6);
	DrawFps();
	DrawCoursePosition(ctrl);
	DrawWind(Wind.Angle(), Wind.Speed(), ctrl);
#ifdef __ANDROID__
	DrawAndroidTouchControls();
#endif
}
