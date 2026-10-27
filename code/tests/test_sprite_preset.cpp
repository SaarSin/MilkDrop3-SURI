// test_sprite_preset.cpp
//
// SURI: tests for features/sprite_preset.* (sprite entries stored in .milk presets).

#include "mini_test.h"
#include <math.h>
#include <locale.h>
#include <string.h>
#include <wchar.h>
#include <map>
#include <string>
#include <windows.h>
#include "..\vis_milk2\features\sprite_preset.h"

static bool Near(float a, float b) { return fabsf(a - b) < 1e-4f; }

static PresetLookup MapLookup(const std::map<std::string, std::string>& m) {
    return [&m](const char* key, std::string* value) -> bool {
        std::map<std::string, std::string>::const_iterator it = m.find(key);
        if (it == m.end()) return false;
        *value = it->second;
        return true;
    };
}

static std::map<std::string, std::string> ParseLines(const std::string& s) {
    std::map<std::string, std::string> m;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) nl = s.size();
        std::string line = s.substr(pos, nl - pos);
        size_t eq = line.find('=');
        if (eq != std::string::npos) m[line.substr(0, eq)] = line.substr(eq + 1);
        pos = nl + 1;
    }
    return m;
}

TEST(TestSpritePreset_RoundTrip) {
    SpriteDesc a = {}; wcscpy_s(a.img, L"textures\\OIchess1.jpg");
    a.x = 0.25f; a.y = 0.75f; a.scale = 0.5f; a.rot = 0.1f; a.blend = 2; a.colorkey = 0x102030;
    std::map<std::string, std::string> m = ParseLines(WritePresetSprites(&a, 1));
    CHECK(m["suri_sprite_count"] == "1");
    CHECK(m["suri_sprite0_colorkey"] == "0x102030");
    SpriteDesc b[kMaxPresetSprites];
    CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 1);
    CHECK(wcscmp(b[0].img, a.img) == 0 && Near(b[0].x, 0.25f) && Near(b[0].scale, 0.5f) && b[0].blend == 2 && b[0].colorkey == 0x102030);
}
TEST(TestSpritePreset_HebrewPathRoundTrip) {
    SpriteDesc a = {}; wcscpy_s(a.img, L"C:\\\u05EA\u05DE\u05D5\u05E0\u05D5\u05EA\\\u05E4\u05E8\u05D7.png");
    a.scale = 1; a.blend = 5;
    SpriteDesc b[1];
    CHECK(ReadPresetSprites(MapLookup(ParseLines(WritePresetSprites(&a, 1))), b, 1) == 1);
    CHECK(wcscmp(b[0].img, a.img) == 0);
}
TEST(TestSpritePreset_MissingImgSkipped) {
    std::map<std::string, std::string> m = { {"suri_sprite_count","2"}, {"suri_sprite0_x","0.5"},
                                             {"suri_sprite1_img","a.png"} };
    SpriteDesc b[kMaxPresetSprites];
    CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 1);
    CHECK(wcscmp(b[0].img, L"a.png") == 0);
    CHECK(Near(b[0].x, 0.5f) && Near(b[0].scale, 1.0f) && b[0].blend == 5);  // defaults
}
TEST(TestSpritePreset_ClampsAndLimits) {
    std::map<std::string, std::string> m = { {"suri_sprite_count","99"} };
    for (int i = 0; i < 99; i++) m["suri_sprite" + std::to_string(i) + "_img"] = "a.png";
    m["suri_sprite0_scale"] = "500"; m["suri_sprite0_blend"] = "9"; m["suri_sprite0_x"] = "-7";
    SpriteDesc b[kMaxPresetSprites];
    CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == kMaxPresetSprites);
    CHECK(Near(b[0].scale, 10.0f) && b[0].blend == 5 && Near(b[0].x, -1.0f));
}
TEST(TestSpritePreset_NoKeysNoSprites) {
    std::map<std::string, std::string> m;
    SpriteDesc b[kMaxPresetSprites];
    CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 0);
    CHECK(WritePresetSprites(nullptr, 0) == "");   // old-style preset stays byte-identical
}
TEST(TestSpriteInitCode_Format) {
    SpriteDesc d = {}; d.x = 0.5f; d.y = 0.25f; d.scale = 2; d.rot = 0; d.blend = 5;
    CHECK(SpriteInitCode(d) == "x=0.5000;y=0.2500;sx=2.0000;sy=2.0000;rot=0.0000;blendmode=5;burn=0;");
}
TEST(TestSpriteLoadColorKey_Blend5ZeroIsNone) {
    SpriteDesc d = {}; d.blend = 5; d.colorkey = 0;
    CHECK(SpriteLoadColorKey(d) == kSpriteNoColorKey);
    d.colorkey = 0x00FF00;
    CHECK(SpriteLoadColorKey(d) == 0x00FF00u);      // a saved key is kept
    d.blend = 4; d.colorkey = 0;
    CHECK(SpriteLoadColorKey(d) == 0u);             // other blends key out black as before
    d.blend = 0;
    CHECK(SpriteLoadColorKey(d) == 0u);
}
TEST(TestSpriteInitCode_IgnoresLocale) {
    setlocale(LC_ALL, "de-DE");
    SpriteDesc d = {}; d.x = 0.5f; d.scale = 1; d.blend = 0;
    std::string s = SpriteInitCode(d);
    setlocale(LC_ALL, "C");
    CHECK(s.find("x=0.5000;") != std::string::npos);
}
TEST(TestSpritePreset_ReadIgnoresLocale) {
    setlocale(LC_ALL, "de-DE");
    std::map<std::string, std::string> m = { {"suri_sprite_count","1"}, {"suri_sprite0_img","a.png"}, {"suri_sprite0_x","0.75"} };
    SpriteDesc b[1];
    int n = ReadPresetSprites(MapLookup(m), b, 1);
    std::string w = WritePresetSprites(b, 1);
    setlocale(LC_ALL, "C");
    CHECK(n == 1 && Near(b[0].x, 0.75f));
    CHECK(w.find("suri_sprite0_x=0.7500") != std::string::npos);
}
TEST(TestSpritePreset_RelativePaths) {
    std::wstring base = L"C:\\App\\plugins\\Milkdrop2\\";
    CHECK(MakeSpritePathRelative(base, L"c:\\app\\PLUGINS\\Milkdrop2\\textures\\a.png") == L"textures\\a.png");
    CHECK(MakeSpritePathRelative(base, L"D:\\pics\\a.png") == L"D:\\pics\\a.png");
    CHECK(ResolveSpritePath(base, L"textures\\a.png") == base + L"textures\\a.png");
    CHECK(ResolveSpritePath(base, L"D:\\pics\\a.png") == L"D:\\pics\\a.png");
    CHECK(ResolveSpritePath(base, L"\\\\server\\share\\a.png") == L"\\\\server\\share\\a.png");
}
