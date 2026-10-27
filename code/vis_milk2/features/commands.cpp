// commands.cpp
//
// SURI: command table - see commands.h.

#include "commands.h"

static const KeyBinding g_keyBindings[] = {
    { KeyScope::Char, 'h', Cmd::PresetNext },
    { KeyScope::Char, 'H', Cmd::PresetNext },
    { KeyScope::Char, '`', Cmd::PresetLockToggle },
    { KeyScope::Char, '~', Cmd::PresetLockToggle },
    { KeyScope::Char, 'r', Cmd::PresetOrderToggle },
    { KeyScope::Char, 'R', Cmd::PresetOrderToggle },   // SURI: Shift+R too
    { KeyScope::Char, 'l', Cmd::PresetLoadMenu },
    { KeyScope::Char, 'L', Cmd::PresetLoadMenu },
    { KeyScope::Char, 's', Cmd::PresetSave },
    { KeyScope::Char, 'S', Cmd::PresetSave },
    { KeyScope::Char, 'm', Cmd::PresetEditMenu },
    { KeyScope::Char, 'M', Cmd::PresetEditMenu },
    { KeyScope::Char, '+', Cmd::PresetRateUp },
    { KeyScope::Char, '-', Cmd::PresetRateDown },
    { KeyScope::Char, 'd', Cmd::ShaderLockCycle },
    { KeyScope::Char, 'D', Cmd::ShaderLockCycle },
    { KeyScope::Char, 'p', Cmd::PlaylistScreen },
    { KeyScope::Char, 'P', Cmd::PlaylistScreen },
    { KeyScope::Char, 't', Cmd::ViewSongTitle },
    { KeyScope::Char, 'T', Cmd::ViewSongTitle },
    { KeyScope::Char, 'k', Cmd::SpriteLaunchPrompt },   // SURI: Shift+K (remove) stays in HandleRegularKey
    { KeyScope::Char, 'y', Cmd::MessagePrompt },
    { KeyScope::Char, 'Y', Cmd::MessagePrompt },
    { KeyScope::VKeyGlobal, VK_F2, Cmd::FpsLimitCycle },
    { KeyScope::VKeyGlobal, VK_F3, Cmd::AutoChangeCycle },
    { KeyScope::VKeyGlobal, VK_F4, Cmd::ViewPresetInfo },
    { KeyScope::VKeyGlobal, VK_F5, Cmd::ViewFps },
    { KeyScope::VKeyGlobal, VK_F6, Cmd::ViewRating },
    { KeyScope::VKeyGlobal, VK_F7, Cmd::ViewAlwaysOnTop },
    { KeyScope::VKeyGlobal, VK_F9, Cmd::ViewShaderHelp },
    { KeyScope::VKeyNormal, VK_SPACE, Cmd::PresetRandom },
    { KeyScope::VKeyNormal, VK_BACK, Cmd::PresetPrev },
    { KeyScope::VKeyNormal, VK_F1, Cmd::HelpOverlay },
    { KeyScope::VKeyAlt, VK_RETURN, Cmd::ViewFullscreen },
    { KeyScope::VKeyAlt, VK_SHIFT, Cmd::ViewStretch },
    { KeyScope::VKeyAlt, VK_F4, Cmd::AppQuit },
};

const KeyBinding* KeyBindings(size_t* count)
{
    *count = sizeof(g_keyBindings) / sizeof(g_keyBindings[0]);
    return g_keyBindings;
}

Cmd LookupKey(KeyScope scope, unsigned key)
{
    for (const KeyBinding& b : g_keyBindings)
        if (b.scope == scope && b.key == key)
            return b.cmd;
    return Cmd::None;
}

static std::wstring VKeyName(unsigned short vk)
{
    if (vk >= VK_F1 && vk <= VK_F12)
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk) {
    case VK_SPACE:  return L"Space";
    case VK_BACK:   return L"Backspace";
    case VK_RETURN: return L"Enter";
    case VK_SHIFT:  return L"Shift";
    }
    return std::wstring(1, (wchar_t)vk);
}

std::wstring KeyLabel(const KeyBinding& b)
{
    switch (b.scope) {
    case KeyScope::Char:    return std::wstring(1, (wchar_t)b.key);
    case KeyScope::VKeyAlt: return L"Alt+" + VKeyName(b.key);
    default:                return VKeyName(b.key);
    }
}

std::wstring PrimaryKeyLabel(Cmd c)
{
    for (const KeyBinding& b : g_keyBindings)
        if (b.cmd == c)
            return KeyLabel(b);
    return L"";
}

UINT CmdToMenuId(Cmd c)
{
    return kCmdMenuIdBase + (UINT)c;
}

Cmd MenuIdToCmd(UINT id)
{
    if (id <= kCmdMenuIdBase || id >= kCmdMenuIdBase + (UINT)Cmd::Count)
        return Cmd::None;
    return (Cmd)(id - kCmdMenuIdBase);
}

Cmd FxCmd(FxId id)
{
    return (Cmd)((unsigned)Cmd::FxMirrorX + (unsigned)id);
}

bool CmdToFx(Cmd c, FxId* out)
{
    if (c < Cmd::FxMirrorX || c > Cmd::FxHueSpin)
        return false;
    *out = (FxId)((unsigned)c - (unsigned)Cmd::FxMirrorX);
    return true;
}

int SpriteBlendOf(Cmd c)
{
    switch (c) {
    case Cmd::SpriteBlendImage:     return 5;
    case Cmd::SpriteBlendAlpha:     return 0;
    case Cmd::SpriteBlendDecal:     return 1;
    case Cmd::SpriteBlendAdditive:  return 2;
    case Cmd::SpriteBlendColorMask: return 3;
    case Cmd::SpriteBlendColorKey:  return 4;
    default:                        return -1;
    }
}

CmdState QueryState(Cmd c, const AppSnapshot& s)
{
    CmdState st = { true, false };

    switch (c) {
    case Cmd::PresetLockToggle:  st.checked = s.presetLockedByUser; break;
    case Cmd::PresetOrderToggle: st.checked = s.sequentialOrder; break;
    case Cmd::PresetLoadMenu:    st.checked = s.loadMenuOpen; break;
    case Cmd::PresetEditMenu:    st.checked = s.editMenuOpen; break;
    case Cmd::PlaylistScreen:    // SURI: it does nothing while text is typed or a prompt is open
        st.checked = s.playlistOpen; st.enabled = s.playlistOpen || !s.textEditActive; break;
    case Cmd::ViewFullscreen:    st.checked = s.fullscreen; break;
    case Cmd::ViewStretch:       st.checked = s.stretch; break;
    case Cmd::ViewAlwaysOnTop:   st.checked = s.alwaysOnTop; break;
    case Cmd::ViewDesktopMode:   // SURI
        st.enabled = s.desktopAvailable; st.checked = s.desktopMode; break;
    case Cmd::ViewFps:           st.checked = s.showFps; break;
    case Cmd::ViewPresetInfo:    st.checked = s.showPresetInfo; break;
    case Cmd::ViewRating:        st.checked = s.showRating; break;
    case Cmd::ViewShaderHelp:    st.checked = s.showShaderHelp; break;
    case Cmd::HelpOverlay:       st.checked = s.showHelp; break;
    case Cmd::PresetRandom:      st.enabled = !s.presetLockedByCode; break;
    case Cmd::PresetDelete:      st.enabled = s.canDeleteCurrent; break;
    case Cmd::ViewSongTitle:     st.enabled = s.hasSongTitle; break;
    case Cmd::ViewNowPlaying:    st.enabled = s.nowPlayingAvailable; st.checked = s.nowPlayingOn; break;
    // SURI: sprites and messages. File dialogs and Notepad would hide behind a full-screen window.
    case Cmd::SpriteLoadImage:
    case Cmd::SpriteEditList:
    case Cmd::MessageEditList:      st.enabled = !s.fullscreen; break;
    case Cmd::SpriteLaunchPrompt:
    case Cmd::MessagePrompt:        st.enabled = !s.textEditActive; break;
    case Cmd::SpriteRemoveSelected: st.enabled = s.selectedSprite >= 0; break;
    case Cmd::SpriteRemoveAll:      st.enabled = s.spriteCount > 0; break;
    default: break;
    }

    if (SpriteBlendOf(c) >= 0) {
        st.enabled = s.selectedSprite >= 0;
        st.checked = st.enabled && s.selectedSpriteBlend == SpriteBlendOf(c);
    }

    // SURI: in wallpaper mode the window is not ours to resize
    if (s.desktopMode && (c == Cmd::ViewFullscreen || c == Cmd::ViewStretch))
        st.enabled = false;

    // effects: need pixel shaders; the +/- items stop at the ends of the range
    if (c >= Cmd::FxMirrorX && c <= Cmd::FxClearAll) {
        FxId id;
        if (!s.shadersAvailable) st.enabled = false;
        if (CmdToFx(c, &id)) st.checked = (s.fxMask & (1u << (unsigned)id)) != 0;
        switch (c) {
        case Cmd::FxBrightnessUp:   st.enabled = st.enabled && StepLevel(s.fxBrightness, +1) != s.fxBrightness; break;
        case Cmd::FxBrightnessDown: st.enabled = st.enabled && StepLevel(s.fxBrightness, -1) != s.fxBrightness; break;
        case Cmd::FxContrastUp:     st.enabled = st.enabled && StepLevel(s.fxContrast, +1) != s.fxContrast; break;
        case Cmd::FxContrastDown:   st.enabled = st.enabled && StepLevel(s.fxContrast, -1) != s.fxContrast; break;
        case Cmd::FxClearAll:
            st.enabled = st.enabled && (s.fxMask != 0 || s.fxBrightness != 1.0f || s.fxContrast != 1.0f); break;
        default: break;
        }
    }

    // while the user is typing (or answering Y/N), or the playlist screen is open,
    // preset commands would fight it
    bool presetCmd = (c >= Cmd::PresetNext && c <= Cmd::ShaderLockCycle) || c == Cmd::ViewSongTitle;
    if ((s.textEditActive || s.playlistOpen) && presetCmd)
        st.enabled = false;

    return st;
}

int NextFpsLimit(int current)
{
    static const int steps[] = { 0, 30, 60, 90, 120, 144, 240, 360 };
    const int n = sizeof(steps) / sizeof(steps[0]);
    for (int i = 0; i < n; i++)
        if (steps[i] == current)
            return steps[(i + 1) % n];
    return 30;
}

std::wstring FpsLimitMessage(int limit)
{
    if (limit == 0)
        return L"unlimited fps";
    return std::to_wstring(limit) + L" fps";
}

AutoChangeStep NextAutoChange(float currentSeconds)
{
    // 16 (shown as "15 sec") is what the F3 key has always used
    static const AutoChangeStep steps[] = {
        {   0.0f, 0.0f, true,  L"unlimited sec" },
        {  16.0f, 5.0f, false, L"15 sec" },
        {  30.0f, 5.0f, false, L"30 sec" },
        {  60.0f, 5.0f, false, L"60 sec" },
        { 120.0f, 5.0f, false, L"120 sec" },
        { 180.0f, 5.0f, false, L"180 sec" },
    };
    const int n = sizeof(steps) / sizeof(steps[0]);
    for (int i = 0; i < n; i++)
        if (steps[i].seconds == currentSeconds)
            return steps[(i + 1) % n];
    return steps[2];
}
