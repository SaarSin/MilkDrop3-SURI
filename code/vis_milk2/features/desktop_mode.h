// desktop_mode.h
//
// SURI: pure decisions for running the visualizer as the desktop wallpaper:
// which window to parent into, what to do when the shell restarts, the tray menu.
// No window is touched here; unit-tested in code/tests/test_desktop_mode.cpp.

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include "context_menu.h"

struct TopWindowInfo { HWND hwnd; std::wstring className; bool hasDefView; };

// Given the top-level windows in EnumWindows order and Progman's WorkerW child (or NULL),
// returns the window to parent into, or NULL.
HWND PickWallpaperHost(const std::vector<TopWindowInfo>& top, HWND progmanWorkerChild);

enum class DesktopEvent { None, HostLost, TaskbarCreated, SessionLocked, SessionUnlocked, DisplayChanged };
enum class DesktopAction { None, Detach, Reattach, Resize };
DesktopAction DecideDesktopAction(bool active, bool hostValid, DesktopEvent e);

// SURI: relaunch after the shell restarts. The render window is a child of the shell's
// WorkerW, so it dies with Explorer; the app then starts a new copy of itself with
//   MilkDrop3.exe /desktop-resume ["<preset path>"]
// which waits for the wallpaper host and goes back to the desktop.
// True if argv holds /desktop-resume (any case); *presetPath = the argument after it,
// or empty when there is none or it is another /switch.
bool ParseResumeArgs(int argc, const wchar_t* const* argv, std::wstring* presetPath);
// Loop guard: a copy started by a resume less than 30 s ago does not relaunch again.
// Ticks are GetTickCount() values (wraparound safe).
bool ShouldRelaunch(DWORD nowTick, DWORD startedAtTick, bool startedByResume);
std::wstring BuildResumeCommandLine(const std::wstring& exePath, const std::wstring& presetPath);

// ViewDesktopMode, PresetNext, PresetRandom, PresetLockToggle, Effects submenu (same nodes
// as the main menu), FxClearAll, AppQuit
const std::vector<MenuNode>& TrayMenuLayout();

// ---------------------------------------------------------------------------
// SURI: the Win32 side (desktop_mode_win32.cpp, MilkDrop3.exe only)

typedef void (*ResizeBackBufferFn)(int width, int height);

// Parents the render window into the wallpaper host (behind the desktop icons) and back.
class CDesktopMode {
public:
    static const UINT_PTR kTimerId = 0x5D02;   // WM_TIMER id of the 250 ms host check

    bool Enter(HWND hwnd, ResizeBackBufferFn resize);   // false (and logged) if no host
    // full restore, safe to call twice. show=false (quitting): the window stays hidden and
    // is not brought to the front, so nothing flashes on the way out.
    void Leave(bool show = true);
    bool IsActive() const { return m_active; }
    void OnEvent(DesktopEvent e);                       // runs DecideDesktopAction
    void OnTimer();                                     // host check; retries while detached
    void OnWindowDestroyed();                           // WM_DESTROY: no window calls after this
    HWND Host() const { return m_host; }
    // SURI: the shell is up far enough to host us (Progman and a wallpaper host found); quiet
    static bool HostAvailable();

private:
    bool HostValid() const;
    bool Attach(HWND host);
    void Detach();
    void FitToVirtualScreen();

    bool m_active = false;
    HWND m_hwnd = NULL;
    HWND m_host = NULL;                // NULL while detached (host lost)
    ResizeBackBufferFn m_resize = nullptr;
    LONG m_savedStyle = 0, m_savedExStyle = 0;
    RECT m_savedRect = { 0 };
    unsigned m_ticksDetached = 0;
};

// The notification-area icon, present only while in desktop mode. It owns a hidden
// top-level window: the render window is a child of the shell there, so it gets neither
// the TaskbarCreated / WM_DISPLAYCHANGE broadcasts nor a usable foreground for menus.
// That window forwards kMsg, TaskbarCreated, WM_DISPLAYCHANGE and the menu-loop messages
// to the render window.
class CTrayIcon {
public:
    static const UINT kMsg = WM_APP + 0x51;

    bool Add(HWND hwnd, HICON icon, const wchar_t* tip);
    void Remove();
    void ReAdd();                       // after TaskbarCreated
    HWND MenuOwner() const { return m_owner; }   // owner window for TrackPopupMenu

private:
    bool Notify(DWORD message);
    HWND m_target = NULL;
    HWND m_owner = NULL;
    HICON m_icon = NULL;
    std::wstring m_tip;
};

UINT TaskbarCreatedMessage();   // RegisterWindowMessageW(L"TaskbarCreated"), cached
