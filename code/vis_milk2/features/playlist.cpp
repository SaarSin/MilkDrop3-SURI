// playlist.cpp
//
// SURI: see playlist.h.

#include "playlist.h"
#include <windows.h>
#include <algorithm>

// <0, 0, >0 like strcmp; ordinal, case-insensitive.
static int CmpNoCase(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE) - CSTR_EQUAL;
}

static bool LessNoCase(const std::wstring& a, const std::wstring& b) { return CmpNoCase(a, b) < 0; }

void Playlist::SetAll() {
    m_all = true;
    m_names.clear();
    m_gen++;
}

void Playlist::SetNames(const std::vector<std::wstring>& names) {
    m_all = false;
    m_names = names;
    std::sort(m_names.begin(), m_names.end(), LessNoCase);
    size_t w = 0;
    for (size_t i = 0; i < m_names.size(); i++) {
        if (w > 0 && CmpNoCase(m_names[w - 1], m_names[i]) == 0) continue;
        if (w != i) m_names[w] = m_names[i];
        w++;
    }
    m_names.resize(w);
    m_gen++;
}

// Index of the match in a case-insensitively sorted list, or -1.
static int SortedFind(const std::vector<std::wstring>& sorted, const std::wstring& name) {
    size_t lo = 0, hi = sorted.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = CmpNoCase(sorted[mid], name);
        if (c == 0) return (int)mid;
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return -1;
}

bool Playlist::Contains(const std::wstring& name) const {
    return m_all || SortedFind(m_names, name) >= 0;
}

std::vector<int> AllowedIndices(const std::vector<std::wstring>& presetNames, int dirs, const Playlist& pl) {
    std::vector<int> out;
    int n = (int)presetNames.size();
    if (dirs < 0) dirs = 0;
    if (!pl.IsAll()) {
        for (int i = dirs; i < n; i++)
            if (SortedFind(pl.Names(), presetNames[i]) >= 0) out.push_back(i);
    }
    if (out.empty())
        for (int i = dirs; i < n; i++) out.push_back(i);
    return out;
}

int PickSequential(const std::vector<int>& allowed, int current, int direction) {
    if (allowed.empty()) return -1;
    if (direction >= 0) {
        for (size_t i = 0; i < allowed.size(); i++)
            if (allowed[i] > current) return allowed[i];
        return allowed.front();
    }
    for (size_t i = allowed.size(); i-- > 0; )
        if (allowed[i] < current) return allowed[i];
    return allowed.back();
}

static float RatingAt(const std::vector<float>& r, int idx) {
    return (idx >= 0 && (size_t)idx < r.size() && r[idx] > 0) ? r[idx] : 0.0f;
}

int PickWeighted(const std::vector<int>& allowed, const std::vector<float>& ratingThis, float r01) {
    if (allowed.empty()) return -1;
    float sum = 0;
    for (size_t i = 0; i < allowed.size(); i++) sum += RatingAt(ratingThis, allowed[i]);
    if (sum < 0.1f) {
        size_t k = (size_t)(r01 * allowed.size());
        if (k >= allowed.size()) k = allowed.size() - 1;
        return allowed[k];
    }
    float target = r01 * sum;
    float cum = 0;
    for (size_t i = 0; i < allowed.size(); i++) {
        cum += RatingAt(ratingThis, allowed[i]);
        if (target < cum) return allowed[i];
    }
    return allowed.back();
}

static std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string SerializeM3u(const std::vector<std::wstring>& names) {
    std::string s = "\xEF\xBB\xBF";
    s += "#EXTM3U\r\n";
    for (size_t i = 0; i < names.size(); i++) {
        s += ToUtf8(names[i]);
        s += "\r\n";
    }
    return s;
}

static bool IsWs(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; }

std::vector<std::wstring> ParseM3u(const std::string& bytes) {
    std::vector<std::wstring> out;
    size_t pos = 0;
    if (bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
    while (pos < bytes.size()) {
        size_t e = bytes.find('\n', pos);
        if (e == std::string::npos) e = bytes.size();
        std::wstring line = FromUtf8(bytes.substr(pos, e - pos));
        pos = e + 1;
        size_t b = 0, t = line.size();
        while (b < t && IsWs(line[b])) b++;
        while (t > b && IsWs(line[t - 1])) t--;
        line = line.substr(b, t - b);
        if (line.empty() || line[0] == L'#') continue;
        size_t sl = line.find_last_of(L"\\/");
        if (sl != std::wstring::npos) line = line.substr(sl + 1);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

LoadCount MatchToLibrary(const std::vector<std::wstring>& wanted, const std::vector<std::wstring>& libraryNames, std::vector<std::wstring>* out) {
    std::vector<std::wstring> sorted = libraryNames;   // lookup copy; keeps the library's spelling
    std::sort(sorted.begin(), sorted.end(), LessNoCase);
    LoadCount c; c.loaded = 0; c.total = wanted.size();
    if (out) out->clear();
    for (size_t i = 0; i < wanted.size(); i++) {
        int k = SortedFind(sorted, wanted[i]);
        if (k < 0) continue;
        c.loaded++;
        if (out) out->push_back(sorted[k]);
    }
    return c;
}

std::wstring LoadedMessage(LoadCount c) {
    return L"Loaded " + std::to_wstring((unsigned long long)c.loaded) + L" of " +
           std::to_wstring((unsigned long long)c.total) + L" presets";
}
