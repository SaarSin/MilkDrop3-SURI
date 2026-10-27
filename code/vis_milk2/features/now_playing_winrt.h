// now_playing_winrt.h
//
// SURI: background thread that asks Windows (GlobalSystemMediaTransportControlsSessionManager,
// the media flyout) which song is playing and puts it into a SongInfoBox. The C++/WinRT code
// lives only in now_playing_winrt.cpp (C++17); this header is plain C++14 for plugin.h.

#pragma once

#include <windows.h>

class SongInfoBox;

const DWORD kNowPlayingPollMs = 2000, kNowPlayingCallTimeoutMs = 1500, kNowPlayingJoinMs = 3000;

class CNowPlaying {
public:
    CNowPlaying();
    ~CNowPlaying();
    void Start(SongInfoBox* box);   // starts the poll thread; returns at once
    void Stop();                    // signals and joins; at most kNowPlayingJoinMs
private:
    CNowPlaying(const CNowPlaying&);
    CNowPlaying& operator=(const CNowPlaying&);
    HANDLE m_thread;
    HANDLE m_stopEvent;
};
