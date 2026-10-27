// playlist.h
//
// SURI: pure core of the preset playlist (which presets the visualizer may pick,
// sequential / weighted picking, M3U read/write, matching a loaded list to the library).

#pragma once

#include <string>
#include <vector>

class Playlist {
public:
    bool IsAll() const { return m_all; }                         // default true: whole library
    void SetAll();
    void SetNames(const std::vector<std::wstring>& names);       // stored sorted + unique, case-insensitive; IsAll() becomes false
    bool Contains(const std::wstring& name) const;               // IsAll() -> true
    const std::vector<std::wstring>& Names() const { return m_names; }   // empty when IsAll()
    unsigned Generation() const { return m_gen; }                // +1 on every SetAll/SetNames
private:
    bool m_all = true;
    std::vector<std::wstring> m_names;
    unsigned m_gen = 0;
};

// presetNames = m_presets filenames in list order; the first `dirs` entries are directories ("*name").
// Indices of files the playlist allows. Never empty while there is at least one file:
// an empty match (empty playlist, or every name gone) means "all files".
std::vector<int> AllowedIndices(const std::vector<std::wstring>& presetNames, int dirs, const Playlist& pl);
// Next (direction +1) / previous (-1) allowed index after `current` in list order, wrapping.
// `current` need not be allowed (or valid). allowed must be sorted ascending and non-empty.
int PickSequential(const std::vector<int>& allowed, int current, int direction);
// Rating-weighted pick over `allowed` using ratingThis[i]; r01 in [0,1).
// If the allowed ratings sum to < 0.1 -> uniform: allowed[(size_t)(r01 * allowed.size())].
int PickWeighted(const std::vector<int>& allowed, const std::vector<float>& ratingThis, float r01);

std::string SerializeM3u(const std::vector<std::wstring>& names);  // UTF-8 BOM, "#EXTM3U", one name per line, CRLF
std::vector<std::wstring> ParseM3u(const std::string& bytes);      // BOM optional, LF/CRLF, skips # lines and blanks, trims, keeps text after the last backslash or slash
struct LoadCount { size_t loaded; size_t total; };
// out = wanted names that exist in libraryNames (case-insensitive), with the library's spelling.
LoadCount MatchToLibrary(const std::vector<std::wstring>& wanted, const std::vector<std::wstring>& libraryNames, std::vector<std::wstring>* out);
std::wstring LoadedMessage(LoadCount c);                           // L"Loaded 31 of 34 presets"
