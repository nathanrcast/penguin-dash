/* --------------------------------------------------------------------
EXTREME TUXRACER

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

/*
If you want to add a new option, do this:
First add the option to the TParam struct (game_config.h).

Then edit the below functions:

- LoadConfigFile. Use
	SPIntN for integer and boolean values
	SPStrN for strings.
	The first value is always 'line', the second defines the tag within the
	brackets [ ], and the last value is the default.

- SetConfigDefaults. These values are used as long as no options file exists.
	It's a good idea to use the same values as the defaults in LoadConfigFile.

- SaveConfigFile. See the other entries; it should be self-explanatory.
	If an options file exists, you will have to change any value at runtime
	on the configuration screen to overwrite the file. Then you will see the
	new entry.
*/

#ifdef HAVE_CONFIG_H
#include <etr_config.h>
#endif

#include "config_screen.h"
#include "spx.h"
#include <algorithm>
#include <cstdlib>
#include "translation.h"
#include "particles.h"
#include "audio.h"
#include "ogl.h"
#include "gui.h"
#include "font.h"
#include "winsys.h"
#ifdef __ANDROID__
#include "native_bridge.h"
#endif

CGameConfig GameConfig;
static std::string res_names[NUM_RESOLUTIONS];

static TCheckbox* fullscreen;
static TUpDown* language;
static TUpDown* resolution;
static TUpDown* mus_vol;
static TUpDown* sound_vol;
static TUpDown* detail_level;
static TWidget* textbuttons[2];
#ifdef __ANDROID__
// Render-scale (P6): 3D surface scale, hardware-upscaled. Applied at next launch.
static TUpDown* render_scale;
static TUpDown* control_mode;
static TUpDown* control_sensitivity;
static TLabel* descriptions[8];
static const int scale_values[4] = { 50, 67, 75, 100 };
static const char* control_mode_names[2] = { "Tilt", "Onscreen" };
static int ScaleIndexFromValue(int value) {
	int best = 3;
	for (int i = 0; i < 4; i++)
		if (std::abs(scale_values[i] - value) < std::abs(scale_values[best] - value))
			best = i;
	return best;
}
#else
static TLabel* descriptions[5];
#endif

void SetConfig() {
	bool changed =
	        mus_vol->GetValue() != param.music_volume ||
	        sound_vol->GetValue() != param.sound_volume ||
	        language->GetValue() != param.language ||
	        resolution->GetValue() != param.res_type ||
	        detail_level->GetValue() != param.perf_level;
#ifdef __ANDROID__
	changed = changed ||
	        scale_values[render_scale->GetValue()] != param.render_scale ||
	        control_mode->GetValue() != param.control_mode ||
	        control_sensitivity->GetValue() != param.control_sensitivity;
#else
	changed = changed || fullscreen->checked != param.fullscreen;
#endif

	if (changed) {
#ifndef __ANDROID__
		if (resolution->GetValue() != param.res_type || fullscreen->checked != param.fullscreen) {
			// these changes require a new VideoMode
			param.res_type = resolution->GetValue();
			param.fullscreen = fullscreen->checked;
			Winsys.SetupVideoMode(param.res_type);
			init_ui_snow(); // Reinitialize UI snow to avoid ugly snow-free stripes at the borders
		}
#endif

		// the followind config params don't require a new VideoMode
		// they only must stored in the param structure (and saved)
		param.music_volume = mus_vol->GetValue();
		Music.SetVolume(param.music_volume);
		param.sound_volume = sound_vol->GetValue();
		param.perf_level = detail_level->GetValue();
		param.res_type = resolution->GetValue();
#ifdef __ANDROID__
		// Native host reads this from options.txt at boot; applies next launch.
		param.render_scale = scale_values[render_scale->GetValue()];
		param.control_mode = control_mode->GetValue();
		param.control_sensitivity = control_sensitivity->GetValue();
		pd::SetAndroidControls(param.control_mode, param.control_sensitivity);
#endif
		FT.SetFontFromSettings();
		if (param.language != language->GetValue()) {
			param.language = language->GetValue();
			Trans.ChangeLanguage(param.language);
		}
		SaveConfigFile();
	}
	State::manager.RequestEnterState(*State::manager.PreviousState());
}

void CGameConfig::Keyb(sf::Keyboard::Key key, bool release, int x, int y) {
	if (release) return;

	switch (key) {
		case sf::Keyboard::U:
			param.ui_snow = !param.ui_snow;
			break;
		case sf::Keyboard::Escape:
			State::manager.RequestEnterState(*State::manager.PreviousState());
			break;
		case sf::Keyboard::Return:
			if (textbuttons[0]->focussed())
				State::manager.RequestEnterState(*State::manager.PreviousState());
			else if (textbuttons[1]->focussed())
				SetConfig();
			break;
		default:
			KeyGUI(key, release);
			break;
	}
}

void CGameConfig::Mouse(int button, int state, int x, int y) {
	if (state == 1) {
		TWidget* focussed = ClickGUI(x, y);

		if (focussed == textbuttons[0])
			State::manager.RequestEnterState(*State::manager.PreviousState());
		else if (focussed == textbuttons[1])
			SetConfig();
	}
}

void CGameConfig::Motion(int x, int y) {
	MouseMoveGUI(x, y);

	if (param.ui_snow) push_ui_snow(cursor_pos);
}

// ------------------ Init --------------------------------------------

static TArea area;
static int dd;
static int columnAnchor;
static int firstRow; // 0 on Android (no fullscreen row), 1 on desktop

void CGameConfig::Enter() {
	Winsys.ShowCursor(!param.ice_cursor);

	for (int i=0; i<NUM_RESOLUTIONS; i++)
		res_names[i] = Winsys.GetResName(i);

#ifdef __ANDROID__
	// Tablet / render-scaled surfaces are short. The old `dd > 32` cap packed
	// eight rows into overlapping text + fighting UpDown hitboxes. Pitch rows
	// from the real arrow widget height, use most of the screen, and skip the
	// useless fullscreen checkbox (NativeActivity is always full-window).
	int framewidth = static_cast<int>(620 * Winsys.scale);
	if (framewidth > static_cast<int>(Winsys.resolution.width) - 32)
		framewidth = static_cast<int>(Winsys.resolution.width) - 32;
	area = AutoAreaN(12, 88, framewidth);
	FT.AutoSizeN(3);
	const float arrowScale = Winsys.scale / 0.70f; // matches GuiArrowScale in gui.cpp
	const int avail = area.bottom - area.top;
	const int maxRow = std::max(1, avail / 8);
	// Gap between the two chevrons — shrink if the surface is short (render scale).
	int udDistance = 8;
	int udHeight = static_cast<int>((32 + udDistance) * arrowScale);
	const int padY = static_cast<int>(6 * Winsys.scale);
	if (udHeight + padY * 2 + 2 > maxRow) {
		udDistance = std::max(2, static_cast<int>(maxRow / arrowScale) - 32 - 2);
		udHeight = static_cast<int>((32 + udDistance) * arrowScale);
	}
	dd = FT.AutoDistanceN(4);
	const int minPitch = udHeight + padY * 2 + 2;
	if (dd < minPitch) dd = minPitch;
	if (dd > maxRow) dd = maxRow;
	const int rightpos = area.right - static_cast<int>(36 * arrowScale);
	const int btnY = std::min(AutoYPosN(93), area.top + dd * 8 + static_cast<int>(4 * Winsys.scale));
	firstRow = 0; // options start at area.top (no checkbox above)
#else
	int framewidth = 550 * Winsys.scale;
	area = AutoAreaN(30, 80, framewidth);
	FT.AutoSizeN(4);
	dd = FT.AutoDistanceN(3);
	if (dd < 36) dd = 36;
	const int rightpos = area.right - 48;
	const int btnY = AutoYPosN(80);
	const int udDistance = 2;
	firstRow = 1; // row 0 is fullscreen checkbox
#endif

	ResetGUI();
	unsigned int siz = FT.AutoSizeN(5);
#ifndef __ANDROID__
	fullscreen = AddCheckbox(area.left, area.top, framewidth-16, Trans.Text(31));
	fullscreen->checked = param.fullscreen;
#else
	fullscreen = nullptr;
#endif

	resolution = AddUpDown(rightpos, area.top+dd*(firstRow+0), 0, NUM_RESOLUTIONS-1, (int)param.res_type, udDistance, true);
	mus_vol = AddUpDown(rightpos, area.top+dd*(firstRow+1), 0, 100, param.music_volume, udDistance, true);
	sound_vol = AddUpDown(rightpos, area.top+dd*(firstRow+2), 0, 100, param.sound_volume, udDistance, true);
	language = AddUpDown(rightpos, area.top+dd*(firstRow+3), 0, (int)Trans.languages.size() - 1, (int)param.language, udDistance, true);
	detail_level = AddUpDown(rightpos, area.top+dd*(firstRow+4), 1, 4, param.perf_level, udDistance, true);
#ifdef __ANDROID__
	render_scale = AddUpDown(rightpos, area.top+dd*(firstRow+5), 0, 3, ScaleIndexFromValue(param.render_scale), udDistance, true);
	control_mode = AddUpDown(rightpos, area.top+dd*(firstRow+6), 0, 1, param.control_mode, udDistance, true);
	control_sensitivity = AddUpDown(rightpos, area.top+dd*(firstRow+7), 1, 10, param.control_sensitivity, udDistance, true);
#endif

	textbuttons[0] = AddTextButton(Trans.Text(28), area.left+50, btnY, siz);
	float len = FT.GetTextWidth(Trans.Text(8));
	textbuttons[1] = AddTextButton(Trans.Text(15), area.right-len-50, btnY, siz);

#ifdef __ANDROID__
	FT.AutoSizeN(3);
#else
	FT.AutoSizeN(4);
#endif
	columnAnchor = 0;
	for (int i = 0; i < 5; i++) {
		descriptions[i] = AddLabel(Trans.Text(32 + i), area.left, area.top + dd*(firstRow + i), colWhite);
		columnAnchor = std::max(columnAnchor, (int)descriptions[i]->GetSize().x);
	}
#ifdef __ANDROID__
	// Keep labels short so the value column stays clear of the arrow column.
	descriptions[5] = AddLabel("Render scale", area.left, area.top + dd*(firstRow+5), colWhite);
	descriptions[6] = AddLabel("Movement", area.left, area.top + dd*(firstRow+6), colWhite);
	descriptions[7] = AddLabel("Sensitivity", area.left, area.top + dd*(firstRow+7), colWhite);
	columnAnchor = std::max(columnAnchor, (int)descriptions[5]->GetSize().x);
	columnAnchor = std::max(columnAnchor, (int)descriptions[6]->GetSize().x);
	columnAnchor = std::max(columnAnchor, (int)descriptions[7]->GetSize().x);
#endif
	columnAnchor += area.left + static_cast<int>(20 * Winsys.scale);
#ifdef __ANDROID__
	// Leave a clear gutter between values and the UpDown arrows.
	const int valueMax = rightpos - static_cast<int>(100 * Winsys.scale);
	if (columnAnchor > valueMax) columnAnchor = valueMax;
#endif

	Music.Play(param.config_music, true);
}

void CGameConfig::Loop(float time_step) {
	ScopedRenderMode rm(GUI);
	Winsys.clear();

	if (param.ui_snow) {
		update_ui_snow(time_step);
		draw_ui_snow();
	}

	DrawGUIBackground(Winsys.scale);

#ifdef __ANDROID__
	FT.AutoSizeN(3);
#else
	FT.AutoSizeN(4);
#endif

	descriptions[0]->Focussed(resolution->focussed());
	descriptions[1]->Focussed(mus_vol->focussed());
	descriptions[2]->Focussed(sound_vol->focussed());
	descriptions[3]->Focussed(language->focussed());
	descriptions[4]->Focussed(detail_level->focussed());
#ifdef __ANDROID__
	descriptions[5]->Focussed(render_scale->focussed());
	descriptions[6]->Focussed(control_mode->focussed());
	descriptions[7]->Focussed(control_sensitivity->focussed());
#endif

	FT.SetColor(colWhite);
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 0) + 3, res_names[resolution->GetValue()]);
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 1) + 3, Int_StrN(mus_vol->GetValue()));
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 2) + 3, Int_StrN(sound_vol->GetValue()));
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 3) + 3, Trans.languages[language->GetValue()].language);
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 4) + 3, Int_StrN(detail_level->GetValue()));
#ifdef __ANDROID__
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 5) + 3, Int_StrN(scale_values[render_scale->GetValue()]) + "%");
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 6) + 3, control_mode_names[control_mode->GetValue()]);
	FT.DrawString(columnAnchor, area.top + dd * (firstRow + 7) + 3, Int_StrN(control_sensitivity->GetValue()));
#endif

#ifndef __ANDROID__
	// "edit options.txt" hint — not reachable on Android (app-private storage),
	// and it collides with the render-scale row added there.
	FT.SetColor(colLGrey);
	FT.AutoSizeN(3);
	FT.DrawString(CENTER, AutoYPosN(68), Trans.Text(41));
	FT.DrawString(CENTER, AutoYPosN(72), Trans.Text(42));
#endif

	DrawGUI();

	Winsys.SwapBuffers();
}
