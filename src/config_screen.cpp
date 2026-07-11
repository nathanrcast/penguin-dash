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
	if (mus_vol->GetValue() != param.music_volume ||
	        sound_vol->GetValue() != param.sound_volume ||
	        language->GetValue() != param.language ||
	        resolution->GetValue() != param.res_type ||
	        detail_level->GetValue() != param.perf_level ||
#ifdef __ANDROID__
	        scale_values[render_scale->GetValue()] != param.render_scale ||
	        control_mode->GetValue() != param.control_mode ||
	        control_sensitivity->GetValue() != param.control_sensitivity ||
#endif
	        fullscreen->checked != param.fullscreen) {

		if (resolution->GetValue() != param.res_type || fullscreen->checked != param.fullscreen) {
			// these changes require a new VideoMode
			param.res_type = resolution->GetValue();
			param.fullscreen = fullscreen->checked;
			Winsys.SetupVideoMode(param.res_type);
			init_ui_snow(); // Reinitialize UI snow to avoid ugly snow-free stripes at the borders
		}

		// the followind config params don't require a new VideoMode
		// they only must stored in the param structure (and saved)
		param.music_volume = mus_vol->GetValue();
		Music.SetVolume(param.music_volume);
		param.sound_volume = sound_vol->GetValue();
		param.perf_level = detail_level->GetValue();
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

void CGameConfig::Enter() {
	Winsys.ShowCursor(!param.ice_cursor);

	for (int i=0; i<NUM_RESOLUTIONS; i++)
		res_names[i] = Winsys.GetResName(i);

	int framewidth = 550 * Winsys.scale;
#ifdef __ANDROID__
	// Extra Android rows (render scale + movement + sensitivity) need more vertical room.
	area = AutoAreaN(22, 82, framewidth);
#else
	area = AutoAreaN(30, 80, framewidth);
#endif
	FT.AutoSizeN(4);
	dd = FT.AutoDistanceN(3);
	if (dd < 36) dd = 36;
#ifdef __ANDROID__
	if (dd > 32) dd = 32; // keep eight rows on-screen
#endif
	int rightpos = area.right -48;

	ResetGUI();
	unsigned int siz = FT.AutoSizeN(5);
	fullscreen = AddCheckbox(area.left, area.top, framewidth-16, Trans.Text(31));
	fullscreen->checked = param.fullscreen;

	resolution = AddUpDown(rightpos, area.top+dd*1, 0, NUM_RESOLUTIONS-1, (int)param.res_type);
	mus_vol = AddUpDown(rightpos, area.top+dd*2, 0, 100, param.music_volume, 2, true);
	sound_vol = AddUpDown(rightpos, area.top+dd*3, 0, 100, param.sound_volume, 2, true);
	language = AddUpDown(rightpos, area.top+dd*4, 0, (int)Trans.languages.size() - 1, (int)param.language);
	detail_level = AddUpDown(rightpos, area.top+dd*5, 1, 4, param.perf_level, 2, true);
#ifdef __ANDROID__
	render_scale = AddUpDown(rightpos, area.top+dd*6, 0, 3, ScaleIndexFromValue(param.render_scale), 2, true);
	control_mode = AddUpDown(rightpos, area.top+dd*7, 0, 1, param.control_mode, 1, true);
	control_sensitivity = AddUpDown(rightpos, area.top+dd*8, 1, 10, param.control_sensitivity, 1, true);
#endif

	textbuttons[0] = AddTextButton(Trans.Text(28), area.left+50, AutoYPosN(80), siz);
	float len = FT.GetTextWidth(Trans.Text(8));
	textbuttons[1] = AddTextButton(Trans.Text(15), area.right-len-50, AutoYPosN(80), siz);

	columnAnchor = 0;
	for (int i = 0; i < 5; i++) {
		descriptions[i] = AddLabel(Trans.Text(32 + i), area.left, area.top + dd*(i + 1), colWhite);
		columnAnchor = std::max(columnAnchor, (int)descriptions[i]->GetSize().x);
	}
#ifdef __ANDROID__
	descriptions[5] = AddLabel("Render scale (restart)", area.left, area.top + dd*6, colWhite);
	descriptions[6] = AddLabel("Movement", area.left, area.top + dd*7, colWhite);
	descriptions[7] = AddLabel("Sensitivity", area.left, area.top + dd*8, colWhite);
	columnAnchor = std::max(columnAnchor, (int)descriptions[5]->GetSize().x);
	columnAnchor = std::max(columnAnchor, (int)descriptions[6]->GetSize().x);
	columnAnchor = std::max(columnAnchor, (int)descriptions[7]->GetSize().x);
#endif
	columnAnchor += area.left + 20*Winsys.scale;

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

	FT.AutoSizeN(4);

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
	FT.DrawString(columnAnchor, area.top + dd + 3, res_names[resolution->GetValue()]);
	FT.DrawString(columnAnchor, area.top + dd * 2 + 3, Int_StrN(mus_vol->GetValue()));
	FT.DrawString(columnAnchor, area.top + dd * 3 + 3, Int_StrN(sound_vol->GetValue()));
	FT.DrawString(columnAnchor, area.top + dd * 4 + 3, Trans.languages[language->GetValue()].language);
	FT.DrawString(columnAnchor, area.top + dd * 5 + 3, Int_StrN(detail_level->GetValue()));
#ifdef __ANDROID__
	FT.DrawString(columnAnchor, area.top + dd * 6 + 3, Int_StrN(scale_values[render_scale->GetValue()]) + "%");
	FT.DrawString(columnAnchor, area.top + dd * 7 + 3, control_mode_names[control_mode->GetValue()]);
	FT.DrawString(columnAnchor, area.top + dd * 8 + 3, Int_StrN(control_sensitivity->GetValue()));
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
