// now_playing.cpp
//
// SURI: see now_playing.h.

#include "now_playing.h"

std::wstring FormatSongInfo(const SongInfo& s) {
    if (s.title.empty()) return std::wstring();
    if (s.artist.empty()) return s.title;
    return s.artist + L" \u2013 " + s.title;
}

std::wstring TruncateUtf16(const std::wstring& s, size_t maxUnits) {
    if (s.size() <= maxUnits) return s;
    size_t n = maxUnits;
    if (n > 0 && s[n - 1] >= 0xD800 && s[n - 1] <= 0xDBFF) --n;   // do not split a surrogate pair
    return s.substr(0, n);
}

bool SongChanged(const std::wstring& prev, const std::wstring& cur) {
    return !cur.empty() && cur != prev;
}

void SongInfoBox::Set(const SongInfo& s) {
    std::wstring t = FormatSongInfo(s);
    std::lock_guard<std::mutex> lk(m_mutex);
    m_text = t;
}
std::wstring SongInfoBox::Text() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_text;
}
void SongInfoBox::SetState(NowPlayingState st) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_state = st;
}
NowPlayingState SongInfoBox::State() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_state;
}
