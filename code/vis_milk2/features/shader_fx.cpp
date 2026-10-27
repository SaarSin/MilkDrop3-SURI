// shader_fx.cpp
//
// SURI: see shader_fx.h.

#include "shader_fx.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

const char* const kNames[(int)FxId::Count] = {
    "mirror-x", "mirror-y", "kaleido", "ripple", "zoom-in", "swap-axes", "tunnel",
    "invert", "mono", "posterize", "hue-spin"
};

const char* const kCode[(int)FxId::Count] = {
    "uv.x = 0.5 - abs(uv.x - 0.5);",
    "uv.y = 0.5 - abs(uv.y - 0.5);",
    "float2 kd = uv - 0.5; float kr = length(kd); float ka = atan2(kd.y, kd.x); float ks = 1.0471976; "
        "ka = abs(ka - ks * floor(ka / ks) - 0.5 * ks); uv = 0.5 + kr * float2(cos(ka), sin(ka));",
    "uv += 0.008 * sin(uv.yx * 31.0 + time * float2(1.7, 2.3));",
    "uv = 0.5 + (uv - 0.5) * 0.7;",
    "uv = uv.yx;",
    "float2 td = uv - 0.5; uv = float2(atan2(td.y, td.x) * 0.1591549 + 0.5, frac(0.1 / max(length(td), 0.01)));",
    "ret = 1.0 - ret;",
    "ret = dot(ret, float3(0.3, 0.59, 0.11)).xxx;",
    "ret = floor(ret * 4.0 + 0.5) * 0.25;",
    "float3 hk = 0.57735; float hc = cos(time * 0.6); float hs = sin(time * 0.6); "
        "ret = ret * hc + cross(hk, ret) * hs + hk * dot(hk, ret) * (1.0 - hc);"
};

const char kTag[] = "// SURI-FX:";
// SURI: on the first block's begin line when ApplyFx had to break the line (a '{' or '}' that
// shares its line with other text); StripAllFx then removes that line break too.
const char kJoined[] = "+newline";

// eol: the text's own line ending. joined: see kJoined. The code goes inside "{ }"; StripAllFx
// works from the markers only, so it also removes blocks written before that scoping.
std::string MakeBlock(const char* name, const std::string& code, const std::string& eol, bool joined) {
    std::string b = "    ";
    b += kTag; b += name; b += " begin";
    if (joined) { b += " "; b += kJoined; }
    b += eol; b += "    ";
    b += "{ "; b += code; b += " }";   // SURI: own scope, so the effect's locals cannot clash with the preset's
    b += eol; b += "    "; b += kTag; b += name; b += " end"; b += eol;
    return b;
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// SURI: true if t has word (" begin" / " end") at i, followed by whitespace or the end of the text
bool WordAt(const std::string& t, size_t i, const char* word) {
    size_t n = std::strlen(word);
    if (t.compare(i, n, word) != 0) return false;
    return i + n >= t.size() || IsSpace(t[i + n]);
}

// If a comment starts at i, returns the index just past it; otherwise returns i.
size_t SkipComment(const std::string& t, size_t i) {
    if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '/') {
        size_t e = t.find('\n', i);
        return e == std::string::npos ? t.size() : e + 1;
    }
    if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '*') {
        size_t e = t.find("*/", i + 2);
        return e == std::string::npos ? t.size() : e + 2;
    }
    return i;
}

bool IsIdent(char c) { return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// Finds the '{' of shader_body and its matching '}'.
bool FindBody(const std::string& t, size_t* open, size_t* close) {
    static const char kKey[] = "shader_body";
    const size_t klen = sizeof(kKey) - 1;
    size_t i = 0;
    bool found = false;
    while (i < t.size()) {
        size_t j = SkipComment(t, i);
        if (j != i) { i = j; continue; }
        if (t.compare(i, klen, kKey) == 0 && (i == 0 || !IsIdent(t[i - 1])) &&
            (i + klen >= t.size() || !IsIdent(t[i + klen]))) {
            i += klen;
            found = true;
            break;
        }
        ++i;
    }
    if (!found) return false;
    // first '{' after the keyword
    while (i < t.size() && t[i] != '{') {
        size_t j = SkipComment(t, i);
        i = (j != i) ? j : i + 1;
    }
    if (i >= t.size()) return false;
    *open = i;
    int depth = 0;
    while (i < t.size()) {
        size_t j = SkipComment(t, i);
        if (j != i) { i = j; continue; }
        if (t[i] == '{') ++depth;
        else if (t[i] == '}') {
            if (--depth == 0) { *close = i; return true; }
        }
        ++i;
    }
    return false;
}

} // namespace

bool FxState::Any() const { return mask != 0 || brightness != 1.0f || contrast != 1.0f; }

void ToggleFx(FxState& s, FxId id) { s.mask ^= (1u << (unsigned)id); }

float StepLevel(float current, int direction) {
    float step = direction > 0 ? 0.1f : (direction < 0 ? -0.1f : 0.0f);
    float v = std::floor((current + step) * 10.0f + 0.5f) / 10.0f;
    if (v < 0.2f) v = 0.2f;
    if (v > 3.0f) v = 3.0f;
    return v;
}

const char* FxName(FxId id) {
    return (unsigned)id < (unsigned)FxId::Count ? kNames[(int)id] : "";
}

bool StripAllFx(std::string& t) {
    bool removed = false;
    size_t pos = 0;
    const size_t taglen = sizeof(kTag) - 1;
    for (;;) {
        size_t b = t.find(kTag, pos);
        if (b == std::string::npos) break;
        size_t nameStart = b + taglen, nameEnd = nameStart;
        while (nameEnd < t.size() && (IsIdent(t[nameEnd]) || t[nameEnd] == '-')) ++nameEnd;
        if (nameEnd == nameStart || !WordAt(t, nameEnd, " begin")) { pos = nameStart; continue; }
        std::string endMarker = std::string(kTag) + t.substr(nameStart, nameEnd - nameStart);
        size_t e = t.find(endMarker, nameEnd);
        while (e != std::string::npos && !WordAt(t, e + endMarker.size(), " end"))
            e = t.find(endMarker, e + endMarker.size());
        if (e == std::string::npos) { pos = nameEnd; continue; }
        // another begin before this end means this one is stray: skip it, keep its text
        bool stray = false;
        for (size_t q = t.find(kTag, nameEnd); q != std::string::npos && q < e; q = t.find(kTag, q + taglen)) {
            size_t n = q + taglen;
            while (n < t.size() && (IsIdent(t[n]) || t[n] == '-')) ++n;
            if (WordAt(t, n, " begin")) { stray = true; break; }
        }
        if (stray) { pos = nameEnd; continue; }
        // SURI: the begin line's own words, to see whether ApplyFx broke a line for this block
        size_t beginEol = t.find('\n', nameEnd);
        if (beginEol == std::string::npos) beginEol = t.size();
        std::string beginRest = t.substr(nameEnd + 6, beginEol - (nameEnd + 6));
        bool joined = beginRest.find(kJoined) != std::string::npos;
        // whole lines: from the start of the begin line to just past the end line
        size_t from = b;
        while (from > 0 && (t[from - 1] == ' ' || t[from - 1] == '\t')) --from;
        if (joined && from > 0 && t[from - 1] == '\n') {   // SURI: and the line break ApplyFx added
            --from;
            if (from > 0 && t[from - 1] == '\r') --from;
        }
        size_t to = t.find('\n', e);
        to = (to == std::string::npos) ? t.size() : to + 1;
        t.erase(from, to - from);
        removed = true;
        pos = from;
    }
    return removed;
}

bool ApplyFx(std::string& text, const FxState& s) {
    std::string t = text;
    StripAllFx(t);
    size_t open = 0, close = 0;
    if (!FindBody(t, &open, &close)) return false;

    // SURI: inserted lines use the text's own line ending
    const std::string eol = t.find("\r\n") != std::string::npos ? "\r\n" : "\n";

    // colour first (later in the text), so the earlier index stays valid
    size_t ls = close;
    while (ls > 0 && (t[ls - 1] == ' ' || t[ls - 1] == '\t')) --ls;
    const bool colJoined = !(ls == 0 || t[ls - 1] == '\n');   // '}' shares its line with code
    size_t p = open + 1;
    if (p < t.size() && t[p] == '\r') ++p;
    const bool geoJoined = !(p < t.size() && t[p] == '\n');   // '{' shares its line with code

    std::string geo, col;
    for (int i = 0; i <= (int)FxId::Tunnel; ++i)
        if (s.mask & (1u << i)) geo += MakeBlock(kNames[i], kCode[i], eol, geoJoined && geo.empty());
    for (int i = (int)FxId::Invert; i < (int)FxId::Count; ++i)
        if (s.mask & (1u << i)) col += MakeBlock(kNames[i], kCode[i], eol, colJoined && col.empty());
    char buf[64];
    if (s.brightness != 1.0f) {
        std::snprintf(buf, sizeof(buf), "ret *= %.2f;", s.brightness);
        col += MakeBlock("brightness", buf, eol, colJoined && col.empty());
    }
    if (s.contrast != 1.0f) {
        std::snprintf(buf, sizeof(buf), "ret = (ret - 0.5) * %.2f + 0.5;", s.contrast);
        col += MakeBlock("contrast", buf, eol, colJoined && col.empty());
    }

    if (!col.empty()) {
        if (!colJoined) t.insert(ls, col);
        else t.insert(close, eol + col);
    }
    if (!geo.empty()) {
        if (!geoJoined) t.insert(p + 1, geo);
        else t.insert(open + 1, eol + geo);
    }
    text = t;
    return true;
}
