// context_menu.cpp
//
// SURI: right-click menu layout - see context_menu.h.

#include "context_menu.h"
#include "..\resource.h"
#include <windowsx.h>

UINT CmdLabelId(Cmd c)
{
    switch (c) {
    case Cmd::PresetNext:         return IDS_MENU_PRESET_NEXT;
    case Cmd::PresetPrev:         return IDS_MENU_PRESET_PREV;
    case Cmd::PresetRandom:       return IDS_MENU_PRESET_RANDOM;
    case Cmd::PresetLockToggle:   return IDS_MENU_PRESET_LOCK;
    case Cmd::PresetOrderToggle:  return IDS_MENU_PRESET_ORDER;
    case Cmd::PresetLoadMenu:     return IDS_MENU_PRESET_BROWSE;
    case Cmd::PresetSave:         return IDS_MENU_PRESET_SAVE;
    case Cmd::PresetDelete:       return IDS_MENU_PRESET_DELETE;
    case Cmd::PresetEditMenu:     return IDS_MENU_PRESET_EDIT;
    case Cmd::PresetRateUp:       return IDS_MENU_PRESET_RATE_UP;
    case Cmd::PresetRateDown:     return IDS_MENU_PRESET_RATE_DOWN;
    case Cmd::ShaderLockCycle:    return IDS_MENU_SHADER_LOCK;
    case Cmd::PlaylistScreen:     return IDS_MENU_PRESET_PLAYLIST;
    case Cmd::ViewFullscreen:     return IDS_MENU_VIEW_FULLSCREEN;
    case Cmd::ViewStretch:        return IDS_MENU_VIEW_STRETCH;
    case Cmd::ViewDesktopMode:    return IDS_MENU_VIEW_DESKTOP;   // SURI
    case Cmd::ViewAlwaysOnTop:    return IDS_MENU_VIEW_ON_TOP;
    case Cmd::ViewFps:            return IDS_MENU_VIEW_FPS;
    case Cmd::ViewPresetInfo:     return IDS_MENU_VIEW_PRESET_NAME;
    case Cmd::ViewRating:         return IDS_MENU_VIEW_RATING;
    case Cmd::ViewShaderHelp:     return IDS_MENU_VIEW_SHADER_HELP;
    case Cmd::ViewSongTitle:      return IDS_MENU_VIEW_SONG_TITLE;
    case Cmd::ViewNowPlaying:     return IDS_MENU_VIEW_NOW_PLAYING;
    case Cmd::FpsLimitCycle:      return IDS_MENU_FPS_LIMIT;
    case Cmd::AutoChangeCycle:    return IDS_MENU_AUTO_CHANGE;
    case Cmd::FxMirrorX: return IDS_MENU_FX_MIRROR_X;
    case Cmd::FxMirrorY: return IDS_MENU_FX_MIRROR_Y;
    case Cmd::FxKaleido: return IDS_MENU_FX_KALEIDO;
    case Cmd::FxRipple: return IDS_MENU_FX_RIPPLE;
    case Cmd::FxZoomIn: return IDS_MENU_FX_ZOOM_IN;
    case Cmd::FxSwapAxes: return IDS_MENU_FX_SWAP_AXES;
    case Cmd::FxTunnel: return IDS_MENU_FX_TUNNEL;
    case Cmd::FxInvert: return IDS_MENU_FX_INVERT;
    case Cmd::FxMono: return IDS_MENU_FX_MONO;
    case Cmd::FxPosterize: return IDS_MENU_FX_POSTERIZE;
    case Cmd::FxHueSpin: return IDS_MENU_FX_HUE_SPIN;
    case Cmd::FxBrightnessUp: return IDS_MENU_FX_BRIGHTER;
    case Cmd::FxBrightnessDown: return IDS_MENU_FX_DARKER;
    case Cmd::FxContrastUp: return IDS_MENU_FX_CONTRAST_UP;
    case Cmd::FxContrastDown: return IDS_MENU_FX_CONTRAST_DOWN;
    case Cmd::FxClearAll: return IDS_MENU_FX_CLEAR;
    case Cmd::SpriteLoadImage:      return IDS_MENU_SPRITE_LOAD;
    case Cmd::SpriteLaunchPrompt:   return IDS_MENU_SPRITE_LAUNCH;
    case Cmd::SpriteRemoveSelected: return IDS_MENU_SPRITE_REMOVE;
    case Cmd::SpriteRemoveAll:      return IDS_MENU_SPRITE_REMOVE_ALL;
    case Cmd::SpriteBlendImage:     return IDS_MENU_SPRITE_BLEND_IMAGE;
    case Cmd::SpriteBlendAlpha:     return IDS_MENU_SPRITE_BLEND_ALPHA;
    case Cmd::SpriteBlendDecal:     return IDS_MENU_SPRITE_BLEND_DECAL;
    case Cmd::SpriteBlendAdditive:  return IDS_MENU_SPRITE_BLEND_ADDITIVE;
    case Cmd::SpriteBlendColorMask: return IDS_MENU_SPRITE_BLEND_COLOR_MASK;
    case Cmd::SpriteBlendColorKey:  return IDS_MENU_SPRITE_BLEND_COLOR_KEY;
    case Cmd::SpriteEditList:       return IDS_MENU_SPRITE_EDIT_LIST;
    case Cmd::MessageRandom:        return IDS_MENU_MSG_RANDOM;
    case Cmd::MessagePrompt:        return IDS_MENU_MSG_SHOW;
    case Cmd::MessageEditList:      return IDS_MENU_MSG_EDIT_LIST;
    case Cmd::HelpOverlay:        return IDS_MENU_HELP_KEYS;
    case Cmd::HelpUserGuide:      return IDS_MENU_HELP_GUIDE;
    case Cmd::HelpAuthoringGuide: return IDS_MENU_HELP_AUTHORING;
    case Cmd::HelpAbout:          return IDS_MENU_HELP_ABOUT;
    case Cmd::AppQuit:            return IDS_MENU_EXIT;
    default:                      return 0;
    }
}

static MenuNode Item(Cmd c)       { return MenuNode{ MenuNodeKind::Item, c, CmdLabelId(c), {} }; }
static MenuNode Separator()       { return MenuNode{ MenuNodeKind::Separator, Cmd::None, 0, {} }; }
static MenuNode Submenu(UINT labelId, std::vector<MenuNode> children)
{
    return MenuNode{ MenuNodeKind::Submenu, Cmd::None, labelId, children };
}

const std::vector<MenuNode>& MenuLayout()
{
    static const std::vector<MenuNode> layout = {
        Item(Cmd::PresetNext),
        Item(Cmd::PresetPrev),
        Item(Cmd::PresetRandom),
        Separator(),
        Submenu(IDS_MENU_PRESET, {
            Item(Cmd::PresetLockToggle),
            Item(Cmd::PresetOrderToggle),
            Item(Cmd::PresetLoadMenu),
            Item(Cmd::PlaylistScreen),
            Item(Cmd::PresetSave),
            Item(Cmd::PresetDelete),
            Separator(),
            Item(Cmd::PresetRateUp),
            Item(Cmd::PresetRateDown),
            Item(Cmd::ShaderLockCycle),
            Item(Cmd::PresetEditMenu),
        }),
        Submenu(IDS_MENU_VIEW, {
            Item(Cmd::ViewFullscreen),
            Item(Cmd::ViewStretch),
            Item(Cmd::ViewDesktopMode),
            Item(Cmd::ViewAlwaysOnTop),
            Separator(),
            Item(Cmd::ViewFps),
            Item(Cmd::ViewPresetInfo),
            Item(Cmd::ViewRating),
            Item(Cmd::ViewShaderHelp),
            Item(Cmd::ViewSongTitle),
            Item(Cmd::ViewNowPlaying),
            Separator(),
            Item(Cmd::FpsLimitCycle),
            Item(Cmd::AutoChangeCycle),
        }),
        Submenu(IDS_MENU_EFFECTS, {
            Item(Cmd::FxMirrorX),
            Item(Cmd::FxMirrorY),
            Item(Cmd::FxKaleido),
            Item(Cmd::FxRipple),
            Item(Cmd::FxZoomIn),
            Item(Cmd::FxSwapAxes),
            Item(Cmd::FxTunnel),
            Separator(),
            Item(Cmd::FxInvert),
            Item(Cmd::FxMono),
            Item(Cmd::FxPosterize),
            Item(Cmd::FxHueSpin),
            Separator(),
            Item(Cmd::FxBrightnessUp),
            Item(Cmd::FxBrightnessDown),
            Item(Cmd::FxContrastUp),
            Item(Cmd::FxContrastDown),
            Separator(),
            Item(Cmd::FxClearAll),
        }),
        Submenu(IDS_MENU_SPRITES, {   // SURI: phase 9
            Item(Cmd::SpriteLoadImage),
            Item(Cmd::SpriteLaunchPrompt),
            Item(Cmd::SpriteRemoveSelected),
            Item(Cmd::SpriteRemoveAll),
            Separator(),
            Submenu(IDS_MENU_SPRITE_BLEND, {
                Item(Cmd::SpriteBlendImage),
                Item(Cmd::SpriteBlendAlpha),
                Item(Cmd::SpriteBlendDecal),
                Item(Cmd::SpriteBlendAdditive),
                Item(Cmd::SpriteBlendColorMask),
                Item(Cmd::SpriteBlendColorKey),
            }),
            Separator(),
            Item(Cmd::SpriteEditList),
        }),
        Submenu(IDS_MENU_MESSAGES, {
            Item(Cmd::MessageRandom),
            Item(Cmd::MessagePrompt),
            Item(Cmd::MessageEditList),
        }),
        Submenu(IDS_MENU_HELP, {
            Item(Cmd::HelpOverlay),
            Item(Cmd::HelpUserGuide),
            Item(Cmd::HelpAuthoringGuide),
            Separator(),
            Item(Cmd::HelpAbout),
        }),
        Separator(),
        Item(Cmd::AppQuit),
    };
    return layout;
}

POINT MenuAnchor(LPARAM lParam, const RECT& windowRect)
{
    POINT p;
    if (lParam == (LPARAM)-1) {
        // opened from the keyboard (Shift+F10 / Apps key): centre of the window
        p.x = (windowRect.left + windowRect.right) / 2;
        p.y = (windowRect.top + windowRect.bottom) / 2;
    } else {
        // signed: monitors left of / above the primary one have negative coordinates
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
    }
    return p;
}
