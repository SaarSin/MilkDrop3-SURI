// sprite_preset.h
//
// SURI: pure code for storing sprites in .milk presets: the SpriteDesc record, the init code
// that places a sprite, writing/reading the suri_sprite* keys, and relative image paths.
// Unit-tested in code/tests/test_sprite_preset.cpp.

#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <type_traits>

const int kMaxPresetSprites = 8;
const int kSpriteBlendImageAlpha = 5;

struct SpriteDesc {                 // trivially copyable: lives inside CState (which is memcpy'd)
    wchar_t img[MAX_PATH];          // relative to the Milkdrop2 dir, or absolute
    float x, y, scale, rot;         // x,y in 0..1 like var_x/var_y
    int blend;                      // 0..5
    unsigned colorkey;              // 0x00RRGGBB
};
static_assert(std::is_trivially_copyable<SpriteDesc>::value, "SpriteDesc must be memcpy-safe");

// "x=0.5000;y=0.5000;sx=0.5000;sy=0.5000;rot=0.0000;blendmode=5;burn=0;" (always "C" locale
// numbers; burn=0 keeps the sprite out of the feedback buffer, so it leaves no trails)
std::string SpriteInitCode(const SpriteDesc& d);

// Passed to texmgr::LoadTex for "no colour key" (same value as TEXMGR_NO_COLORKEY).
const unsigned kSpriteNoColorKey = 0xFFFFFFFFu;
// The colour key to load a SpriteDesc with. Rule: blend 5 (image alpha) with colorkey 0 means
// "no colour key", so pure black stays opaque; any other combination keys out d.colorkey (as
// milk2_img.ini sprites always did). Sprites loaded without a key are saved as colorkey 0, so
// with blend 5 they reload without a key.
unsigned SpriteLoadColorKey(const SpriteDesc& d);

// "suri_sprite_count=N\n" followed by the per-sprite keys; empty string when n <= 0.
std::string WritePresetSprites(const SpriteDesc* s, int n);

typedef std::function<bool(const char* key, std::string* value)> PresetLookup;
// Returns the number of sprites stored in out (<= maxOut); entries without an image are skipped.
int ReadPresetSprites(const PresetLookup& get, SpriteDesc* out, int maxOut);

// Strips baseDir (case-insensitive prefix) from path; otherwise returns path unchanged.
std::wstring MakeSpritePathRelative(const std::wstring& baseDir, const std::wstring& path);
// Prepends baseDir unless img is absolute ("X:" or "\\\\").
std::wstring ResolveSpritePath(const std::wstring& baseDir, const std::wstring& img);
