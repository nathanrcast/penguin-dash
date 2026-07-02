// Penguin Dash — ETR engine bootstrap on Android (port step 3, A1c).
//
// Replaces src/main.cpp (excluded from the Android build): defines the g_game
// singleton and runs the same init sequence + State::manager loop, but on the
// live EGL surface owned by native_main.cpp. Called from android_main once the
// surface is ready. Asset loading (LoadTextureList/fonts/music) is best-effort
// here — the real APK-asset IO lands in A4 — so a failed load no longer aborts;
// the loop still runs so the EGL/winsys/input integration is observable.

#include "bh.h"
#include "textures.h"
#include "ogl.h"
#include "glshader.h"
#include "splash_screen.h"
#include "audio.h"
#include "font.h"
#include "winsys.h"
#include "states.h"
#include "game_config.h"

#include <android/log.h>
#include <cstdlib>
#include <ctime>

#include "native_bridge.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PenguinDash", __VA_ARGS__)

// Defined in src/main.cpp on desktop; that TU is excluded on Android, so the
// single definition lives here.
TGameData g_game;

static void InitGame() {
	g_game.toolmode = NONE;
	g_game.argument = 0;
	g_game.player = nullptr;
	g_game.start_player = 0;
	g_game.course = nullptr;
	g_game.mirrorred = false;
	g_game.character = nullptr;
	g_game.location_id = 0;
	g_game.light_id = 0;
	g_game.snow_id = 0;
	g_game.cup = 0;
	g_game.theme_id = 0;
	g_game.force_treemap = false;
	g_game.treesize = 3;
	g_game.treevar = 3;
}

void pd_engine_main() {
	LOGI("pd_engine_main: booting ETR engine");
	std::srand(static_cast<unsigned>(std::time(nullptr)));

	InitConfig();
	InitGame();
	Winsys.Init();                 // creates the (EGL-backed) window, sets resolution
	InitOpenglExtensions();
	InitCoreShaders();

	// Best-effort until A4 (APK assets). Log, but keep going so the loop runs.
	if (!Tex.LoadTextureList())
		LOGI("LoadTextureList failed (expected until A4 assets) — continuing");
	FT.LoadFontlist();
	FT.SetFontFromSettings();
	Music.LoadMusicList();
	Music.SetVolume(param.music_volume);

	LOGI("pd_engine_main: entering main loop");
	State::manager.Run(SplashScreen);

	Winsys.Quit();
	LOGI("pd_engine_main: loop exited");
}
