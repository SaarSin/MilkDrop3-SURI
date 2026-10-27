// sprite_preset.cpp
//
// SURI: see sprite_preset.h.

#include "sprite_preset.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static _locale_t CLocale() {
    static _locale_t loc = _create_locale(LC_NUMERIC, "C");
    return loc;
}

static std::string Fmt4(float v) {
    char buf[64];
    _snprintf_s_l(buf, sizeof(buf), _TRUNCATE, "%.4f", CLocale(), (double)v);
    return buf;
}

static std::string ToUtf8(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    s.resize((size_t)n - 1);
    return s;
}

static bool FromUtf8(const std::string& s, wchar_t* out, int cap) {
    if (s.empty()) return false;
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out, cap);
    return n > 1;   // 0 = failure (including does not fit), 1 = empty
}

std::string SpriteInitCode(const SpriteDesc& d) {
    std::string s;
    s += "x=" + Fmt4(d.x) + ";";
    s += "y=" + Fmt4(d.y) + ";";
    s += "sx=" + Fmt4(d.scale) + ";";
    s += "sy=" + Fmt4(d.scale) + ";";
    s += "rot=" + Fmt4(d.rot) + ";";
    s += "blendmode=" + std::to_string(d.blend) + ";";
    s += "burn=0;";     // a clean overlay: not burned into the feedback buffer (no trails)
    return s;
}

unsigned SpriteLoadColorKey(const SpriteDesc& d) {
    if (d.blend == kSpriteBlendImageAlpha && (d.colorkey & 0xFFFFFFu) == 0)
        return kSpriteNoColorKey;
    return d.colorkey & 0xFFFFFFu;
}

std::string WritePresetSprites(const SpriteDesc* s, int n) {
    if (!s || n <= 0) return std::string();
    std::string out = "suri_sprite_count=" + std::to_string(n) + "\n";
    for (int i = 0; i < n; i++) {
        std::string p = "suri_sprite" + std::to_string(i) + "_";
        char ck[32];
        _snprintf_s(ck, sizeof(ck), _TRUNCATE, "0x%06X", s[i].colorkey & 0xFFFFFFu);
        out += p + "img=" + ToUtf8(s[i].img) + "\n";
        out += p + "x=" + Fmt4(s[i].x) + "\n";
        out += p + "y=" + Fmt4(s[i].y) + "\n";
        out += p + "scale=" + Fmt4(s[i].scale) + "\n";
        out += p + "rot=" + Fmt4(s[i].rot) + "\n";
        out += p + "blend=" + std::to_string(s[i].blend) + "\n";
        out += p + "colorkey=" + ck + "\n";
    }
    return out;
}

static float GetFloat(const PresetLookup& get, const std::string& key, float def, float lo, float hi) {
    std::string v;
    if (!get(key.c_str(), &v) || v.empty()) return def;
    char* end = nullptr;
    float f = _strtof_l(v.c_str(), &end, CLocale());
    if (end == v.c_str() || f != f) return def;
    if (f < lo) f = lo;
    if (f > hi) f = hi;
    return f;
}

static long GetLong(const PresetLookup& get, const std::string& key, long def, int base) {
    std::string v;
    if (!get(key.c_str(), &v) || v.empty()) return def;
    char* end = nullptr;
    long r = strtol(v.c_str(), &end, base);
    if (end == v.c_str()) return def;
    return r;
}

int ReadPresetSprites(const PresetLookup& get, SpriteDesc* out, int maxOut) {
    if (!get || !out || maxOut <= 0) return 0;
    long count = GetLong(get, "suri_sprite_count", 0, 10);
    if (count < 0) count = 0;
    if (count > maxOut) count = maxOut;
    int got = 0;
    for (long i = 0; i < count; i++) {
        std::string p = "suri_sprite" + std::to_string(i) + "_";
        std::string img;
        SpriteDesc d;
        memset(&d, 0, sizeof(d));
        if (!get((p + "img").c_str(), &img) || !FromUtf8(img, d.img, MAX_PATH)) continue;
        d.x = GetFloat(get, p + "x", 0.5f, -1.0f, 2.0f);
        d.y = GetFloat(get, p + "y", 0.5f, -1.0f, 2.0f);
        d.scale = GetFloat(get, p + "scale", 1.0f, 0.05f, 10.0f);
        d.rot = GetFloat(get, p + "rot", 0.0f, -1e6f, 1e6f);
        long b = GetLong(get, p + "blend", kSpriteBlendImageAlpha, 10);
        d.blend = (b < 0 || b > 5) ? kSpriteBlendImageAlpha : (int)b;
        d.colorkey = (unsigned)GetLong(get, p + "colorkey", 0, 16) & 0xFFFFFFu;
        out[got++] = d;
    }
    return got;
}

static std::wstring WithSlash(const std::wstring& dir) {
    if (!dir.empty() && dir[dir.size() - 1] != L'\\' && dir[dir.size() - 1] != L'/') return dir + L"\\";
    return dir;
}

std::wstring MakeSpritePathRelative(const std::wstring& baseDir, const std::wstring& path) {
    std::wstring base = WithSlash(baseDir);
    if (base.empty() || path.size() <= base.size()) return path;
    if (_wcsnicmp(path.c_str(), base.c_str(), base.size()) != 0) return path;
    return path.substr(base.size());
}

std::wstring ResolveSpritePath(const std::wstring& baseDir, const std::wstring& img) {
    if (img.size() >= 2 && (img[1] == L':' || (img[0] == L'\\' && img[1] == L'\\'))) return img;
    return WithSlash(baseDir) + img;
}
