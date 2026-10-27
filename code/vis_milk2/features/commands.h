// commands.h
//
// SURI: one table of user commands. The keyboard, the right-click menu (and later the
// tray icon) all go through Cmd, so a command is defined once.
// Pure code: no D3D, no window; unit-tested in code/tests/test_commands.cpp.

#pragma once

#include <windows.h>
#include <string>
#include "shader_fx.h"

enum class Cmd : unsigned short {
    None = 0,
    // preset
    PresetNext, PresetPrev, PresetRandom, PresetLockToggle, PresetOrderToggle,
    PresetLoadMenu, PresetSave, PresetDelete, PresetEditMenu,
    PresetRateUp, PresetRateDown, ShaderLockCycle,
    PlaylistScreen,    // SURI: the session playlist (playlist_screen.h)
    // view
    ViewFullscreen, ViewStretch, ViewAlwaysOnTop, ViewFps, ViewPresetInfo,
    ViewRating, ViewShaderHelp, ViewSongTitle, ViewNowPlaying, FpsLimitCycle, AutoChangeCycle,
    ViewDesktopMode,   // SURI: wallpaper mode (desktop_mode.h)
    // effects (right-click menu only)
    FxMirrorX, FxMirrorY, FxKaleido, FxRipple, FxZoomIn, FxSwapAxes, FxTunnel,
    FxInvert, FxMono, FxPosterize, FxHueSpin,
    FxBrightnessUp, FxBrightnessDown, FxContrastUp, FxContrastDown, FxClearAll,
    // SURI: sprites and custom messages (phase 9)
    SpriteLoadImage, SpriteLaunchPrompt, SpriteRemoveSelected, SpriteRemoveAll,
    SpriteBlendImage, SpriteBlendAlpha, SpriteBlendDecal, SpriteBlendAdditive,
    SpriteBlendColorMask, SpriteBlendColorKey, SpriteEditList,
    MessageRandom, MessagePrompt, MessageEditList,
    // help / app
    HelpOverlay, HelpUserGuide, HelpAuthoringGuide, HelpAbout, AppQuit,
    Count
};

// Where a key is routed from; each scope is looked up at one place in the window procs.
enum class KeyScope : unsigned char {
    Char,        // WM_CHAR, in CPlugin::HandleRegularKey
    VKeyGlobal,  // WM_KEYDOWN, top of CPlugin::MyWindowProc (works even while editing text, like F2-F9)
    VKeyNormal,  // WM_KEYDOWN, CPlugin::MyWindowProc "case 3" (after waitstring and on-screen menu)
    VKeyAlt      // WM_SYSKEYDOWN, StaticWndProc
};

struct KeyBinding { KeyScope scope; unsigned short key; Cmd cmd; };

const KeyBinding* KeyBindings(size_t* count);
Cmd  LookupKey(KeyScope scope, unsigned key);          // Cmd::None if unbound
std::wstring KeyLabel(const KeyBinding& b);            // "F7", "Alt+Enter", "Space", "Backspace", "`"
std::wstring PrimaryKeyLabel(Cmd c);                   // label of c's first binding, L"" if none

const UINT kCmdMenuIdBase = 41000;
UINT CmdToMenuId(Cmd c);
Cmd  MenuIdToCmd(UINT id);                             // Cmd::None outside (None, Count)

// What the menu needs to know to grey out / check items.
struct AppSnapshot {
    bool presetLockedByUser = false, presetLockedByCode = false, sequentialOrder = false;
    bool showFps = false, showPresetInfo = false, showRating = false, showShaderHelp = false, showHelp = false;
    bool alwaysOnTop = false, fullscreen = false, stretch = false;
    bool desktopMode = false, desktopAvailable = false;   // SURI: running as wallpaper / Progman exists
    bool loadMenuOpen = false, editMenuOpen = false;
    bool playlistOpen = false;     // SURI: m_UI_mode == UI_PLAYLIST
    bool textEditActive = false;   // waitstring active, or a Y/N prompt (UI_LOAD_DEL, UI_SAVE_OVERWRITE, UI_UPGRADE_PIXEL_SHADER)
    bool canDeleteCurrent = false; // m_nDirs <= m_nCurrentPreset < m_nPresets
    int  fpsLimit = 0;             // m_max_fps_w, 0 = unlimited
    float autoChangeSeconds = 0;   // m_fTimeBetweenPresets
    bool  shadersAvailable = false; // m_nMaxPSVersion > 0: effects need pixel shaders
    unsigned fxMask = 0;           // CPlugin::m_fx
    float fxBrightness = 1, fxContrast = 1;
    bool nowPlayingAvailable = false; // the media-controls reader has not failed (NowPlayingState != Unavailable)
    bool nowPlayingOn = false;        // CPlugin::m_bNowPlaying
    bool hasSongTitle = false;        // m_szSongTitle is not empty: T has something to show
    int spriteCount = 0;              // SURI: sprite slots with a texture
    int selectedSprite = -1;          // SURI: CPlugin::m_nSelectedSprite (-1 = none)
    int selectedSpriteBlend = -1;     // SURI: blend mode of the selected sprite (0..5), -1 = none
};
// Effect commands <-> FxId (FxMirrorX.. FxHueSpin are in FxId order).
Cmd  FxCmd(FxId id);
bool CmdToFx(Cmd c, FxId* out);
// SURI: sprite blend commands -> var_blendmode value: Image 5, Alpha 0, Decal 1, Additive 2,
// ColorMask 3, ColorKey 4; -1 for any other command.
int  SpriteBlendOf(Cmd c);

struct CmdState { bool enabled; bool checked; };
CmdState QueryState(Cmd c, const AppSnapshot& s);

// F2: max fps 30 -> 60 -> 90 -> 120 -> 144 -> 240 -> 360 -> unlimited (0) -> 30
int          NextFpsLimit(int current);
std::wstring FpsLimitMessage(int limit);        // L"60 fps", L"unlimited fps"

// F3: time between presets 15 (16) -> 30 -> 60 -> 120 -> 180 sec -> off (preset locked)
struct AutoChangeStep { float seconds; float randSeconds; bool lockPreset; const wchar_t* message; };
AutoChangeStep NextAutoChange(float currentSeconds);
