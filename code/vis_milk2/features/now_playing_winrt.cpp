// now_playing_winrt.cpp
//
// SURI: see now_playing_winrt.h. This is the only C++17 file in the plugin (C++/WinRT needs
// it); the per-file settings are in plugin.vcxproj. No co_await and no .get(): every async
// call is waited with wait_for(kNowPlayingCallTimeoutMs) and cancelled if it did not finish,
// so a stuck media player can never hang the poll thread or Stop().

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "now_playing_winrt.h"
#include "now_playing.h"
#include "..\..\audio\log.h"
#include <process.h>
#include <errno.h>
#include <chrono>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Control;

namespace {

// Owned by the thread; the event handle itself is owned (and closed) by CNowPlaying.
struct ThreadArgs { SongInfoBox* box; HANDLE stopEvent; };

// Waits at most kNowPlayingCallTimeoutMs; anything but Completed is cancelled.
template <typename Op>
AsyncStatus WaitDone(Op const& op)
{
    AsyncStatus st = op.wait_for(std::chrono::milliseconds(kNowPlayingCallTimeoutMs));
    if (st != AsyncStatus::Completed) {
        try { op.Cancel(); } catch (...) {}
    }
    return st;
}

std::wstring ToWide(hstring const& h) { return std::wstring(h.c_str(), h.size()); }

bool StopRequested(HANDLE stopEvent) { return WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0; }

// One poll: errors keep the previous value in the box. After Stop() nothing is written.
void PollOnce(GlobalSystemMediaTransportControlsSessionManager const& mgr, SongInfoBox* box, HANDLE stopEvent)
{
    if (StopRequested(stopEvent))
        return;
    try {
        // synchronous, but a cached in-process getter on the manager: no call into the player, no timeout needed
        GlobalSystemMediaTransportControlsSession session = mgr.GetCurrentSession();
        if (!session) {
            if (!StopRequested(stopEvent))
                box->Set(SongInfo{});
            return;
        }
        auto op = session.TryGetMediaPropertiesAsync();
        if (WaitDone(op) != AsyncStatus::Completed)
            return;
        GlobalSystemMediaTransportControlsSessionMediaProperties props = op.GetResults();
        if (!props)
            return;
        SongInfo s;
        s.title = ToWide(props.Title());
        s.artist = ToWide(props.Artist());
        if (StopRequested(stopEvent))
            return;
        box->Set(s);
    }
    catch (hresult_error const&) {}
    catch (...) {}
}

void RunPoller(SongInfoBox* box, HANDLE stopEvent)
{
    GlobalSystemMediaTransportControlsSessionManager mgr{ nullptr };
    HRESULT hr = S_OK;
    try {
        auto op = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        AsyncStatus st = WaitDone(op);
        if (st == AsyncStatus::Completed)
            mgr = op.GetResults();
        else if (st == AsyncStatus::Error)
            hr = op.ErrorCode();
        else
            hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    catch (hresult_error const& e) { hr = e.code(); }
    catch (...) { hr = E_FAIL; }

    if (StopRequested(stopEvent))   // Stop() during RequestAsync: leave the state alone
        return;
    if (!mgr) {
        if (hr == S_OK) hr = E_POINTER;
        box->SetState(NowPlayingState::Unavailable);
        LOG(L"nowplaying: unavailable (0x%08X)", (unsigned)hr);
        return;
    }
    box->SetState(NowPlayingState::Available);
    LOG(L"nowplaying: available");

    for (;;) {
        PollOnce(mgr, box, stopEvent);
        if (WaitForSingleObject(stopEvent, kNowPlayingPollMs) != WAIT_TIMEOUT)
            break;
    }
}

unsigned __stdcall NowPlayingThread(void* p)
{
    ThreadArgs args = *(ThreadArgs*)p;
    delete (ThreadArgs*)p;

    bool apartment = false;
    try {
        init_apartment(apartment_type::multi_threaded);
        apartment = true;
    }
    catch (hresult_error const& e) {
        args.box->SetState(NowPlayingState::Unavailable);
        LOG(L"nowplaying: unavailable (0x%08X)", (unsigned)e.code());
    }
    if (apartment) {
        RunPoller(args.box, args.stopEvent);   // all WinRT objects are released when it returns
        uninit_apartment();
    }
    return 0;
}

} // namespace

CNowPlaying::CNowPlaying() : m_thread(NULL), m_stopEvent(NULL) {}

CNowPlaying::~CNowPlaying()
{
    Stop();
}

void CNowPlaying::Start(SongInfoBox* box)
{
    if (m_thread || !box)
        return;
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);   // manual reset
    if (!ev) {
        LOG(L"nowplaying: CreateEvent failed (%u)", (unsigned)GetLastError());
        return;
    }
    ThreadArgs* args = new ThreadArgs{ box, ev };
    unsigned id = 0;
    uintptr_t h = _beginthreadex(NULL, 0, NowPlayingThread, args, 0, &id);
    if (!h) {
        LOG(L"nowplaying: _beginthreadex failed (%d)", errno);
        delete args;
        CloseHandle(ev);
        return;
    }
    m_thread = (HANDLE)h;
    m_stopEvent = ev;
}

void CNowPlaying::Stop()
{
    if (!m_thread)
        return;
    SetEvent(m_stopEvent);
    if (WaitForSingleObject(m_thread, kNowPlayingJoinMs) == WAIT_OBJECT_0) {
        CloseHandle(m_thread);
        CloseHandle(m_stopEvent);
    } else {
        // no TerminateThread (the process is exiting or the user turned it off): the thread
        // still waits on its event, so both handles are left open on purpose.
        LOG(L"nowplaying: thread did not stop within %u ms", (unsigned)kNowPlayingJoinMs);
    }
    m_thread = NULL;
    m_stopEvent = NULL;
}
