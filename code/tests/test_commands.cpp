// test_commands.cpp
//
// SURI: tests for the command table (features/commands.*): key bindings, menu ids,
// key labels, command state and the F2/F3 cycles.

#include "mini_test.h"
#include "..\vis_milk2\features\commands.h"

TEST(TestCommands_LegacyKeysPreserved) {
    CHECK(LookupKey(KeyScope::Char, 'h') == Cmd::PresetNext);
    CHECK(LookupKey(KeyScope::Char, 'H') == Cmd::PresetNext);
    CHECK(LookupKey(KeyScope::Char, '`') == Cmd::PresetLockToggle);
    CHECK(LookupKey(KeyScope::Char, '~') == Cmd::PresetLockToggle);
    CHECK(LookupKey(KeyScope::Char, 'r') == Cmd::PresetOrderToggle);
    CHECK(LookupKey(KeyScope::Char, 'R') == Cmd::PresetOrderToggle);   // SURI: Shift+R too
    CHECK(LookupKey(KeyScope::Char, 'l') == Cmd::PresetLoadMenu);
    CHECK(LookupKey(KeyScope::Char, 'L') == Cmd::PresetLoadMenu);
    CHECK(LookupKey(KeyScope::Char, 's') == Cmd::PresetSave);
    CHECK(LookupKey(KeyScope::Char, 'S') == Cmd::PresetSave);
    CHECK(LookupKey(KeyScope::Char, 'm') == Cmd::PresetEditMenu);
    CHECK(LookupKey(KeyScope::Char, 'M') == Cmd::PresetEditMenu);
    CHECK(LookupKey(KeyScope::Char, '+') == Cmd::PresetRateUp);
    CHECK(LookupKey(KeyScope::Char, '-') == Cmd::PresetRateDown);
    CHECK(LookupKey(KeyScope::Char, 'd') == Cmd::ShaderLockCycle);
    CHECK(LookupKey(KeyScope::Char, 'D') == Cmd::ShaderLockCycle);
    CHECK(LookupKey(KeyScope::Char, 't') == Cmd::ViewSongTitle);
    CHECK(LookupKey(KeyScope::Char, 'T') == Cmd::ViewSongTitle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F2) == Cmd::FpsLimitCycle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F3) == Cmd::AutoChangeCycle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F4) == Cmd::ViewPresetInfo);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F5) == Cmd::ViewFps);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F6) == Cmd::ViewRating);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F7) == Cmd::ViewAlwaysOnTop);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F9) == Cmd::ViewShaderHelp);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_SPACE) == Cmd::PresetRandom);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_BACK) == Cmd::PresetPrev);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_F1) == Cmd::HelpOverlay);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_RETURN) == Cmd::ViewFullscreen);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_SHIFT) == Cmd::ViewStretch);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_F4) == Cmd::AppQuit);
}

TEST(TestCommands_UnboundKeysReturnNone) {
    CHECK(LookupKey(KeyScope::Char, 'q') == Cmd::None);         // preset-tuning keys stay in the legacy switch
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_F7) == Cmd::None); // scope matters
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F1) == Cmd::None);
}

TEST(TestCommands_NoKeyBoundTwice) {
    size_t n = 0;
    const KeyBinding* b = KeyBindings(&n);
    CHECK(n > 0);
    for (size_t i = 0; i < n; i++) {
        CHECK(b[i].cmd != Cmd::None);
        for (size_t j = i + 1; j < n; j++)
            CHECK(!(b[i].scope == b[j].scope && b[i].key == b[j].key));
    }
}

TEST(TestCommands_MenuIdRoundTrip) {
    for (unsigned c = 1; c < (unsigned)Cmd::Count; c++)
        CHECK(MenuIdToCmd(CmdToMenuId((Cmd)c)) == (Cmd)c);
    CHECK(MenuIdToCmd(0) == Cmd::None);
    CHECK(MenuIdToCmd(kCmdMenuIdBase) == Cmd::None);
    CHECK(MenuIdToCmd(kCmdMenuIdBase + (UINT)Cmd::Count) == Cmd::None);
    CHECK(MenuIdToCmd('s') == Cmd::None);
}

TEST(TestCommands_KeyLabels) {
    CHECK(PrimaryKeyLabel(Cmd::ViewAlwaysOnTop) == L"F7");
    CHECK(PrimaryKeyLabel(Cmd::ViewFullscreen) == L"Alt+Enter");
    CHECK(PrimaryKeyLabel(Cmd::ViewStretch) == L"Alt+Shift");
    CHECK(PrimaryKeyLabel(Cmd::AppQuit) == L"Alt+F4");
    CHECK(PrimaryKeyLabel(Cmd::PresetPrev) == L"Backspace");
    CHECK(PrimaryKeyLabel(Cmd::PresetRandom) == L"Space");
    CHECK(PrimaryKeyLabel(Cmd::PresetLockToggle) == L"`");
    CHECK(PrimaryKeyLabel(Cmd::PresetNext) == L"h");
    CHECK(PrimaryKeyLabel(Cmd::PresetDelete) == L"");
}

TEST(TestCommands_FpsCycle) {
    CHECK(NextFpsLimit(0) == 30);    CHECK(NextFpsLimit(30) == 60);   CHECK(NextFpsLimit(60) == 90);
    CHECK(NextFpsLimit(90) == 120);  CHECK(NextFpsLimit(120) == 144); CHECK(NextFpsLimit(144) == 240);
    CHECK(NextFpsLimit(240) == 360); CHECK(NextFpsLimit(360) == 0);   CHECK(NextFpsLimit(75) == 30);
    CHECK(FpsLimitMessage(0) == L"unlimited fps");
    CHECK(FpsLimitMessage(144) == L"144 fps");
}

TEST(TestCommands_AutoChangeCycle) {
    AutoChangeStep a = NextAutoChange(0.0f);
    CHECK(a.seconds == 16.0f && a.randSeconds == 5.0f && !a.lockPreset && std::wstring(a.message) == L"15 sec");
    CHECK(NextAutoChange(16.0f).seconds == 30.0f);
    CHECK(NextAutoChange(30.0f).seconds == 60.0f);
    CHECK(NextAutoChange(60.0f).seconds == 120.0f);
    CHECK(NextAutoChange(120.0f).seconds == 180.0f);
    AutoChangeStep off = NextAutoChange(180.0f);
    CHECK(off.seconds == 0.0f && off.randSeconds == 0.0f && off.lockPreset && std::wstring(off.message) == L"unlimited sec");
    AutoChangeStep odd = NextAutoChange(45.0f);
    CHECK(odd.seconds == 30.0f && !odd.lockPreset && std::wstring(odd.message) == L"30 sec");
}

TEST(TestCommands_StateChecks) {
    AppSnapshot s;
    CHECK(!QueryState(Cmd::PresetLockToggle, s).checked);
    s.presetLockedByUser = true; s.fullscreen = true; s.alwaysOnTop = true; s.showFps = true;
    s.sequentialOrder = true; s.loadMenuOpen = true;
    CHECK(QueryState(Cmd::PresetLockToggle, s).checked);
    CHECK(QueryState(Cmd::ViewFullscreen, s).checked);
    CHECK(QueryState(Cmd::ViewAlwaysOnTop, s).checked);
    CHECK(QueryState(Cmd::ViewFps, s).checked);
    CHECK(QueryState(Cmd::PresetOrderToggle, s).checked);
    CHECK(QueryState(Cmd::PresetLoadMenu, s).checked);
    CHECK(!QueryState(Cmd::PresetNext, s).checked);
    CHECK(QueryState(Cmd::PresetNext, s).enabled);
}

TEST(TestCommands_TextEditDisablesPresetCommands) {   // Review Focus 1
    AppSnapshot s; s.textEditActive = true; s.canDeleteCurrent = true;
    for (unsigned c = (unsigned)Cmd::PresetNext; c <= (unsigned)Cmd::ShaderLockCycle; c++)
        CHECK(!QueryState((Cmd)c, s).enabled);
    CHECK(!QueryState(Cmd::ViewSongTitle, s).enabled);
    CHECK(QueryState(Cmd::ViewFullscreen, s).enabled);
    CHECK(QueryState(Cmd::HelpOverlay, s).enabled);
    CHECK(QueryState(Cmd::AppQuit, s).enabled);
}

TEST(TestCommands_RandomDisabledWhenLockedByCode) {
    AppSnapshot s; s.presetLockedByCode = true;
    CHECK(!QueryState(Cmd::PresetRandom, s).enabled);
    CHECK(QueryState(Cmd::PresetNext, s).enabled);
}

TEST(TestCommands_DeleteNeedsPresetFile) {
    AppSnapshot s;
    CHECK(!QueryState(Cmd::PresetDelete, s).enabled);
    s.canDeleteCurrent = true;
    CHECK(QueryState(Cmd::PresetDelete, s).enabled);
}

TEST(TestCommands_FxMapping) {
    for (unsigned i = 0; i < (unsigned)FxId::Count; i++) {
        FxId back; CHECK(CmdToFx(FxCmd((FxId)i), &back) && back == (FxId)i);
    }
    FxId x; CHECK(!CmdToFx(Cmd::PresetNext, &x));
    CHECK(FxCmd(FxId::MirrorX) == Cmd::FxMirrorX);   // SURI: ends of the range
    CHECK(FxCmd(FxId::HueSpin) == Cmd::FxHueSpin);
}
TEST(TestCommands_FxState) {
    AppSnapshot s; s.shadersAvailable = true; s.fxMask = 1u << (unsigned)FxId::Mono; s.fxBrightness = 3.0f;
    CHECK(QueryState(Cmd::FxMono, s).checked);
    CHECK(!QueryState(Cmd::FxInvert, s).checked);
    CHECK(!QueryState(Cmd::FxBrightnessUp, s).enabled);    // at the 3.0 ceiling
    CHECK(QueryState(Cmd::FxBrightnessDown, s).enabled);
    CHECK(QueryState(Cmd::FxClearAll, s).enabled);
    s.shadersAvailable = false;
    CHECK(!QueryState(Cmd::FxMono, s).enabled);
}
TEST(TestCommands_FxClearDisabledWhenIdle) {
    AppSnapshot s; s.shadersAvailable = true;
    CHECK(!QueryState(Cmd::FxClearAll, s).enabled);
    CHECK(QueryState(Cmd::FxInvert, s).enabled);
}

TEST(TestCommands_DesktopModeState) {   // SURI
    AppSnapshot s;
    CHECK(!QueryState(Cmd::ViewDesktopMode, s).enabled);   // no Progman
    s.desktopAvailable = true;
    CmdState d = QueryState(Cmd::ViewDesktopMode, s);
    CHECK(d.enabled && !d.checked);
    CHECK(QueryState(Cmd::ViewFullscreen, s).enabled);
    s.desktopMode = true;
    CHECK(QueryState(Cmd::ViewDesktopMode, s).checked);
    CHECK(!QueryState(Cmd::ViewFullscreen, s).enabled);
    CHECK(!QueryState(Cmd::ViewStretch, s).enabled);
}

TEST(TestCommands_NowPlayingState) {
    AppSnapshot s;
    s.nowPlayingAvailable = false; s.nowPlayingOn = true;
    CHECK(!QueryState(Cmd::ViewNowPlaying, s).enabled);
    s.nowPlayingAvailable = true;
    CHECK(QueryState(Cmd::ViewNowPlaying, s).enabled);
    CHECK(QueryState(Cmd::ViewNowPlaying, s).checked);
}
TEST(TestCommands_SongTitleNeedsTitle) {
    AppSnapshot s;
    s.hasSongTitle = false;
    CHECK(!QueryState(Cmd::ViewSongTitle, s).enabled);
    s.hasSongTitle = true;
    CHECK(QueryState(Cmd::ViewSongTitle, s).enabled);
}

// SURI: phase 9 - sprites and messages menus
TEST(TestCommands_SpriteSelectionGates) {
    AppSnapshot s;
    CHECK(QueryState(Cmd::SpriteLoadImage, s).enabled);
    CHECK(!QueryState(Cmd::SpriteRemoveSelected, s).enabled);
    CHECK(!QueryState(Cmd::SpriteRemoveAll, s).enabled);
    CHECK(!QueryState(Cmd::SpriteBlendAlpha, s).enabled);
    s.spriteCount = 1; s.selectedSprite = 3; s.selectedSpriteBlend = 2;
    CHECK(QueryState(Cmd::SpriteRemoveSelected, s).enabled);
    CHECK(QueryState(Cmd::SpriteRemoveAll, s).enabled);
    CHECK(QueryState(Cmd::SpriteBlendAdditive, s).checked);
    CHECK(!QueryState(Cmd::SpriteBlendImage, s).checked);
}
TEST(TestCommands_FileDialogsNotInFullscreen) {
    AppSnapshot s; s.fullscreen = true;
    CHECK(!QueryState(Cmd::SpriteLoadImage, s).enabled);
    CHECK(!QueryState(Cmd::SpriteEditList, s).enabled);
    CHECK(!QueryState(Cmd::MessageEditList, s).enabled);
}
TEST(TestCommands_PromptsBlockedWhileTyping) {
    AppSnapshot s; s.textEditActive = true;
    CHECK(!QueryState(Cmd::SpriteLaunchPrompt, s).enabled);
    CHECK(!QueryState(Cmd::MessagePrompt, s).enabled);
}
TEST(TestCommands_SpriteBlendOf) {
    CHECK(SpriteBlendOf(Cmd::SpriteBlendImage) == 5);
    CHECK(SpriteBlendOf(Cmd::SpriteBlendAlpha) == 0);
    CHECK(SpriteBlendOf(Cmd::SpriteBlendColorKey) == 4);
    CHECK(SpriteBlendOf(Cmd::AppQuit) == -1);
}
TEST(TestCommands_PlaylistKey) {
    CHECK(LookupKey(KeyScope::Char, 'p') == Cmd::PlaylistScreen);
    CHECK(LookupKey(KeyScope::Char, 'P') == Cmd::PlaylistScreen);
}
TEST(TestCommands_PlaylistOpenDisablesPresetCommands) {
    AppSnapshot s; s.playlistOpen = true;
    CHECK(!QueryState(Cmd::PresetRandom, s).enabled);
    CHECK(!QueryState(Cmd::PresetNext, s).enabled);
    CHECK(QueryState(Cmd::PlaylistScreen, s).checked);
}
TEST(TestCommands_PlaylistGreyedDuringPrompt) {
    AppSnapshot s; s.textEditActive = true;            // e.g. the drop's "new presets" prompt
    CHECK(!QueryState(Cmd::PlaylistScreen, s).enabled);
    s.playlistOpen = true;                             // open: it still closes the screen
    CHECK(QueryState(Cmd::PlaylistScreen, s).enabled);
    AppSnapshot r;
    CHECK(QueryState(Cmd::PlaylistScreen, r).enabled);
}
