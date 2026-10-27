// now_playing.h
//
// SURI: pure part of the "Now Playing" feature: song info formatting, UTF-16 safe truncation,
// change detection and a thread-safe box that a background thread fills and the render thread
// reads. No WinRT and no window here; unit-tested in code/tests/test_now_playing.cpp.

#pragma once

#include <string>
#include <mutex>

struct SongInfo { std::wstring title, artist; };

// L"Artist \u2013 Title", L"Title" (no artist), or L"" (no title).
std::wstring FormatSongInfo(const SongInfo& s);

// At most maxUnits UTF-16 code units; never ends on a lone high surrogate.
std::wstring TruncateUtf16(const std::wstring& s, size_t maxUnits);

// True when cur is non-empty and differs from prev (a closed player is not announced).
bool SongChanged(const std::wstring& prev, const std::wstring& cur);

enum class NowPlayingState { Unknown, Available, Unavailable };

// Thread-safe (std::mutex).
class SongInfoBox {
public:
    void Set(const SongInfo& s);          // stores FormatSongInfo(s)
    std::wstring Text() const;
    void SetState(NowPlayingState st);
    NowPlayingState State() const;
private:
    mutable std::mutex m_mutex;
    std::wstring m_text;
    NowPlayingState m_state = NowPlayingState::Unknown;
};
