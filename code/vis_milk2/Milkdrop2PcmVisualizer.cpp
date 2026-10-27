#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#endif

#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>

#include <windows.h>
#include <process.h>
#include <d3d9.h>
#include <math.h>
#include <dwmapi.h>

// SPOUT
#include <ShellScalingApi.h> // for dpi awareness
#pragma comment(lib, "shcore.lib") // for dpi awareness

#include "plugin.h"
#include "resource.h"
#include "resstrings.h"
#include "features/context_menu.h"
#include "features/desktop_mode.h"
#include <shellapi.h>
#include <wtsapi32.h>

#include <mutex>
#include <atomic>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "..\audio\common.h"

#define SAMPLE_SIZE 576
#define DEFAULT_WIDTH 800;
#define DEFAULT_HEIGHT 800;

CPlugin g_plugin;
HINSTANCE api_orig_hinstance = nullptr;
_locale_t g_use_C_locale;
char keyMappings[8];

static IDirect3D9* pD3D9 = nullptr;
static IDirect3DDevice9* pD3DDevice = nullptr;
static D3DPRESENT_PARAMETERS d3dPp;

static LONG lastWindowStyle = 0;
static LONG lastWindowStyleEx = 0;

static bool fullscreen = false;
static bool stretch = false;
static RECT lastRect = { 0 };

static std::atomic<HANDLE> thread = nullptr;
static unsigned threadId = 0;
static std::mutex pcmMutex;
static unsigned char pcmLeftIn[SAMPLE_SIZE];
static unsigned char pcmRightIn[SAMPLE_SIZE];
static unsigned char pcmLeftOut[SAMPLE_SIZE];
static unsigned char pcmRightOut[SAMPLE_SIZE];

static HICON icon = nullptr;

// SURI: desktop background mode (features/desktop_mode.h)
static CDesktopMode desktop;
static CTrayIcon tray;
static HICON trayIcon = nullptr;
static bool exitRequested = false;          // we asked for the window to go away
static bool hotkeyRegistered = false;       // SURI: Ctrl+Alt+Shift+D is ours
static const int kDesktopHotkeyId = 1;      // Ctrl+Alt+Shift+D
static const UINT kMsgDesktopOnStart = WM_APP + 0x52;
static const wchar_t kDesktopOnStartKey[] = L"DesktopModeOnStart";

// SURI: one copy at a time; a second start brings the running one forward. After the shell
// restarts (the render window dies with its WorkerW parent) the app starts a new copy of
// itself with /desktop-resume, which waits for the shell and goes back to the desktop.
static const wchar_t kWindowClass[] = L"MilkDropSURI.Window";
static const wchar_t kInstanceClass[] = L"MilkDropSURI.Instance";   // message-only, findable in desktop mode too
static const wchar_t kInstanceMutex[] = L"Local\\MilkDropSURI.SingleInstance";
static const DWORD kResumeMutexWaitMs = 15000;
static const UINT_PTR kResumeTimerId = 0x5D03;
static const UINT kResumePollMs = 500;
static const unsigned kResumeMaxPolls = 120;   // 60 s
static DWORD startTick = 0;
static bool startedByResume = false;
static std::wstring resumePreset;
static bool resumePending = false;             // hidden window waiting for the shell
static unsigned resumePolls = 0;
static HWND instanceWnd = NULL;

static UINT ActivateMessage() {
    static const UINT msg = RegisterWindowMessageW(L"MilkDropSURI.Activate");
    return msg;
}

void InitD3d(HWND hwnd, int width, int height) {
    pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);

    D3DDISPLAYMODE mode;
    pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

    UINT adapterId = g_plugin.m_adapterId;

    if (adapterId > pD3D9->GetAdapterCount()) {
        adapterId = D3DADAPTER_DEFAULT;
    }

    memset(&d3dPp, 0, sizeof(d3dPp));

    d3dPp.BackBufferCount = 1;
    d3dPp.BackBufferFormat = mode.Format;
    d3dPp.BackBufferWidth = width;
    d3dPp.BackBufferHeight = height;
    d3dPp.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dPp.Flags = 0;
    d3dPp.EnableAutoDepthStencil = TRUE;
    d3dPp.AutoDepthStencilFormat = D3DFMT_D24X8;
    d3dPp.Windowed = TRUE;
    d3dPp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dPp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dPp.hDeviceWindow = (HWND) hwnd;

    pD3D9->CreateDevice(
        adapterId,
        D3DDEVTYPE_HAL,
        (HWND) hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dPp,
        &pD3DDevice);
}

void DeinitD3d() {
    if (pD3DDevice) {
        pD3DDevice->Release();
        pD3DDevice = nullptr;
    }

    if (pD3D9) {
        pD3D9->Release();
        pD3D9 = nullptr;
    }
}

// SURI: new back buffer size for the render window (the window's WM_SIZE then lets
// CPluginShell::OnUserResizeWindow rebuild its own resources at the new size)
static void ResizeBackBuffer(int width, int height) {
    d3dPp.BackBufferWidth = width;
    d3dPp.BackBufferHeight = height;
    pD3DDevice->Reset(&d3dPp);
}

// Code from milkdropper, thank you!
void ToggleStretch(HWND hwnd) {
    if (!stretch) {
        int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

        if (!fullscreen) {
            lastWindowStyle = GetWindowLong(hwnd, GWL_STYLE);
            lastWindowStyleEx = GetWindowLongW(hwnd, GWL_EXSTYLE);
            lastWindowStyleEx &= ~WS_EX_TOPMOST;
            GetWindowRect(hwnd, &lastRect);
        }

        ResizeBackBuffer(width, height);
        stretch = false;
        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongW(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
        SetWindowPos(hwnd, HWND_NOTOPMOST, left, top, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);

        stretch = true;
    } else {
        ShowCursor(TRUE);

        int width = lastRect.right - lastRect.left;
        int height = lastRect.bottom - lastRect.top;

        ResizeBackBuffer(width, height);
        stretch = false;

        SetWindowLongW(hwnd, GWL_STYLE, lastWindowStyle);
        SetWindowLongW(hwnd, GWL_EXSTYLE, lastWindowStyleEx);
        SetWindowPos(hwnd, HWND_NOTOPMOST, lastRect.left, lastRect.top, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);
        SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }
    fullscreen = false;
}

void ToggleFullScreen(HWND hwnd) {
    if (!fullscreen) {
        ShowCursor(FALSE);

        if (!stretch) {
            lastWindowStyle = GetWindowLong(hwnd, GWL_STYLE);
            lastWindowStyleEx = GetWindowLongW(hwnd, GWL_EXSTYLE);
            lastWindowStyleEx &= ~WS_EX_TOPMOST;
            GetWindowRect(hwnd, &lastRect);
        }

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        MONITORINFO info;
        info.cbSize = sizeof(MONITORINFO);

        GetMonitorInfoW(monitor, &info);

        int width = info.rcMonitor.right - info.rcMonitor.left;
        int height = info.rcMonitor.bottom - info.rcMonitor.top;

        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongW(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
        SetWindowPos(hwnd, HWND_TOPMOST, info.rcMonitor.left, info.rcMonitor.top, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);

        ResizeBackBuffer(width, height);
        fullscreen = true;
    }
    else {
        ShowCursor(TRUE);

        int width = lastRect.right - lastRect.left;
        int height = lastRect.bottom - lastRect.top;

        ResizeBackBuffer(width, height);
        fullscreen = false;

        SetWindowLongW(hwnd, GWL_STYLE, lastWindowStyle);
        SetWindowLongW(hwnd, GWL_EXSTYLE, lastWindowStyleEx);
        SetWindowPos(hwnd, HWND_NOTOPMOST, lastRect.left, lastRect.top, width, height, SWP_DRAWFRAME | SWP_FRAMECHANGED);
        SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }
     stretch = false;
}

void RenderFrame();

// SURI: open one of the offline HTML guides from plugins\Milkdrop2\docs\ in the browser
static void OpenGuide(HWND hwnd, const wchar_t* fileName) {
    std::wstring path = std::wstring(g_plugin.m_szMilkdrop2Path) + L"docs\\" + fileName;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        g_plugin.AddError(LoadResString(IDS_MENU_GUIDE_MISSING), 3.0f, ERR_NOTIFY, false);
        return;
    }
    // a TOPMOST full-screen window would cover the browser
    if (fullscreen)
        ToggleFullScreen(hwnd);
    ShellExecuteW(hwnd, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// SURI: wallpaper mode on/off. Entered from a normal window only; the tray icon is
// the way back while the window sits behind the desktop icons.
static void EnterDesktopMode(HWND hwnd) {
    if (desktop.IsActive())
        return;
    if (fullscreen)
        ToggleFullScreen(hwnd);   // also shows the cursor again
    if (stretch)
        ToggleStretch(hwnd);
    // SURI: keys and clicks no longer reach us on the desktop: no overlay may stay open there
    g_plugin.CloseOverlays();
    if (!desktop.Enter(hwnd, ResizeBackBuffer)) {
        g_plugin.AddError(LoadResString(IDS_DESKTOP_UNAVAILABLE), 4.0f, ERR_MISC, false);
        return;
    }
    if (!tray.Add(hwnd, trayIcon ? trayIcon : icon, LoadResString(IDS_DESKTOP_TRAY_TIP))) {
        // SURI: with the hotkey as the way back, keep the hidden owner window: right after an
        // Explorer restart the taskbar may not take icons yet, and its TaskbarCreated
        // broadcast (to that window) adds the icon then
        if (!hotkeyRegistered) {
            tray.Remove();
            // SURI: neither tray icon nor hotkey: no way back from the desktop, so do not stay
            ERR(L"desktop: no tray icon and no hotkey, leaving desktop mode");
            desktop.Leave();
            g_plugin.AddError(LoadResString(IDS_DESKTOP_NO_WAY_BACK), 6.0f, ERR_MISC, false);
        }
    }
}

// SURI: quitting=true on the way out: the window is not shown again (no flash before exit)
static void LeaveDesktopMode(bool quitting = false) {
    if (!desktop.IsActive())
        return;
    tray.Remove();
    desktop.Leave(!quitting);
}

// SURI: the struct CreateProcessW fills in, taken from its signature (its Win32 name is
// kept out of our sources on purpose)
template <typename F> struct LastParamOf;
template <typename R, typename... A> struct LastParamOf<R (WINAPI*)(A...)> {
    typedef typename std::remove_pointer<
        typename std::tuple_element<sizeof...(A) - 1, std::tuple<A...>>::type>::type type;
};
typedef LastParamOf<decltype(&CreateProcessW)>::type ChildProcessInfo;

// SURI: start a new copy of ourselves that returns to the desktop once the shell is back.
// It waits on the single-instance mutex until this process has finished shutting down.
static bool RelaunchForResume() {
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        ERR(L"desktop: relaunch failed, no module path (error %u)", GetLastError());
        return false;
    }
    wchar_t cwd[MAX_PATH];
    DWORD c = GetCurrentDirectoryW(MAX_PATH, cwd);
    bool haveCwd = c > 0 && c < MAX_PATH;

    std::wstring cmd = BuildResumeCommandLine(exe, g_plugin.m_szCurrentPresetFile);
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);   // CreateProcessW may write to the command line

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    ChildProcessInfo pi = {};
    if (!CreateProcessW(exe, buf.data(), NULL, NULL, FALSE, 0, NULL, haveCwd ? cwd : NULL, &si, &pi)) {
        ERR(L"desktop: relaunch failed (error %u)", GetLastError());
        return false;
    }
    LOG(L"desktop: relaunched as pid %u", pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// SURI: the render window died with the shell while on the desktop. Two ways in: WM_DESTROY
// (if it reaches us) or the render loop finding the window gone - on Windows 10 19045 a
// killed Explorer destroys our child window without any WM_DESTROY reaching our thread, and
// the process then ran on with no window at all. Start a new copy that goes back to the
// desktop once the shell is up; the caller then shuts this one down as usual.
static void OnWindowLostWithShell() {
    static bool handled = false;
    if (handled)
        return;
    handled = true;
    desktop.OnWindowDestroyed();
    bool relaunched = false;
    if (ShouldRelaunch(GetTickCount(), startTick, startedByResume)) {
        LOG(L"desktop: window destroyed with the shell, relaunching");
        relaunched = RelaunchForResume();
    } else {
        ERR(L"desktop: window destroyed with the shell again within 30 s of a relaunch, not relaunching");
    }
    // fallback: come back as wallpaper on the next start
    if (!relaunched) {
        LOG(L"desktop: desktop mode resumes on next start");
        WritePrivateProfileStringW(L"settings", kDesktopOnStartKey, L"1", g_plugin.GetConfigIniFile());
    }
}

// SURI: a /desktop-resume start no longer waits: show the window as a normal one
static void EndResumeWait(HWND hwnd, bool show) {
    if (!resumePending)
        return;
    resumePending = false;
    KillTimer(hwnd, kResumeTimerId);
    if (show && !desktop.IsActive()) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

// SURI: polled every 500 ms after a /desktop-resume start, up to 60 s
static void ResumePoll(HWND hwnd) {
    if (!resumePending) {
        KillTimer(hwnd, kResumeTimerId);
        return;
    }
    if (CDesktopMode::HostAvailable()) {
        EnterDesktopMode(hwnd);   // exactly as the hotkey does
        if (desktop.IsActive()) {
            LOG(L"desktop: back on the desktop after the shell restart (%u ms)", resumePolls * kResumePollMs);
            EndResumeWait(hwnd, false);
            return;
        }
        ERR(L"desktop: shell is back but desktop mode failed, showing the window");
    } else if (++resumePolls < kResumeMaxPolls) {
        return;
    } else {
        ERR(L"desktop: no wallpaper host after %u s, showing the window", kResumeMaxPolls * kResumePollMs / 1000);
    }
    EndResumeWait(hwnd, true);
    g_plugin.AddError(LoadResString(IDS_DESKTOP_RESUME_FAILED), 5.0f, ERR_NOTIFY, false);
}

// SURI: a second start asked us to come forward
static void OnActivateRequest(HWND hwnd) {
    if (desktop.IsActive()) {
        g_plugin.AddError(LoadResString(IDS_DESKTOP_ALREADY_RUNNING), 6.0f, ERR_NOTIFY, false);
        return;
    }
    if (resumePending) {
        LOG(L"desktop: started again while waiting for the shell, showing the window");
        EndResumeWait(hwnd, true);
        return;
    }
    if (IsIconic(hwnd))
        ShowWindow(hwnd, SW_RESTORE);
    else if (!IsWindowVisible(hwnd))
        ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

// SURI: the message-only window a second start finds (the render window is not top-level,
// so not findable, while it sits on the desktop); it passes the request on
static LRESULT CALLBACK InstanceWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == ActivateMessage()) {
        HWND target = (HWND)GetWindowLongPtrW(h, GWLP_USERDATA);
        if (target)
            PostMessageW(target, msg, 0, 0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND CreateInstanceWindow(HINSTANCE instance, HWND target) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = InstanceWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kInstanceClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return NULL;
    HWND h = CreateWindowExW(0, kInstanceClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, instance, NULL);
    if (h)
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)target);
    else
        ERR(L"single instance: window failed (error %u)", GetLastError());
    return h;
}

// SURI: second start - bring the running copy forward (it decides how)
static void ActivateRunningInstance() {
    HWND target = FindWindowExW(HWND_MESSAGE, NULL, kInstanceClass, NULL);
    if (!target)
        target = FindWindowW(kWindowClass, NULL);
    if (!target)
        return;   // still starting up (or shutting down): nothing to bring forward
    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    if (pid)
        AllowSetForegroundWindow(pid);
    PostMessageW(target, ActivateMessage(), 0, 0);
}

// SURI: commands that act on the window itself (see features/commands.h)
static bool HandleWindowCommand(HWND hwnd, Cmd c) {
    switch (c) {
        // in desktop mode the window is not ours to resize
        case Cmd::ViewFullscreen: if (!desktop.IsActive()) ToggleFullScreen(hwnd); return true;
        case Cmd::ViewStretch:    if (!desktop.IsActive()) ToggleStretch(hwnd);    return true;
        case Cmd::ViewDesktopMode:
            if (desktop.IsActive())
                LeaveDesktopMode();
            else
                EnterDesktopMode(hwnd);
            return true;
        case Cmd::AppQuit:
            exitRequested = true;
            LeaveDesktopMode(true);
            PostQuitMessage(0);
            return true;
        case Cmd::HelpUserGuide:      OpenGuide(hwnd, L"user-guide.html");      return true;
        case Cmd::HelpAuthoringGuide: OpenGuide(hwnd, L"preset-authoring.html"); return true;
        case Cmd::HelpAbout:
            MessageBoxW(hwnd,
                L"MilkDrop " MILKDROP_VERSION_STRW L"\n\n"
                L"Based on MilkDrop 2 by Ryan Geiss (Nullsoft).\n"
                L"BSD license. See LICENSE.",
                L"About", MB_OK | MB_ICONINFORMATION);
            return true;
        default:                  return false;
    }
}

static bool RunCommand(HWND hwnd, Cmd c) {
    return HandleWindowCommand(hwnd, c) || g_plugin.DispatchCommand(c, hwnd);
}

// SURI: lets the plugin leave full screen around its file dialogs (playlist save / load)
static bool IsFullscreenNow() {
    return fullscreen;
}

static AppSnapshot BuildAppSnapshot() {
    AppSnapshot s = g_plugin.GetAppSnapshot();
    s.fullscreen = fullscreen;
    s.stretch = stretch;
    s.desktopAvailable = FindWindowW(L"Progman", NULL) != NULL;   // SURI
    s.desktopMode = desktop.IsActive();
    return s;
}

static const UINT_PTR kMenuRenderTimerId = 0x5D01;

// SURI: right-click (or Shift+F10 / Apps key) menu
static void ShowContextMenu(HWND hWnd, LPARAM lParam) {
    // full screen hides the cursor; show it while the menu is up
    bool wasFullscreen = fullscreen;
    if (wasFullscreen)
        ShowCursor(TRUE);

    HMENU menu = BuildContextMenu(BuildAppSnapshot());
    RECT wr;
    GetWindowRect(hWnd, &wr);
    POINT p = MenuAnchor(lParam, wr);

    SetForegroundWindow(hWnd);
    // TPM_RETURNCMD: no WM_COMMAND is posted (CPlugin::MyWindowProc would read it as a key)
    UINT id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
    PostMessageW(hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    Cmd c = MenuIdToCmd(id);
    if (c != Cmd::None)
        RunCommand(hWnd, c);

    // once, whatever the command did: leaving full screen adds its own ShowCursor(TRUE)
    if (wasFullscreen)
        ShowCursor(FALSE);
}

// SURI: right-click on the tray icon; the menu belongs to the tray's own top-level window
// (the render window is a child of the shell's window and cannot take the foreground)
static void ShowTrayMenu(HWND hWnd) {
    HWND owner = tray.MenuOwner();
    if (!owner)
        return;

    HMENU menu = BuildMenu(TrayMenuLayout(), BuildAppSnapshot());
    POINT p;
    GetCursorPos(&p);

    SetForegroundWindow(owner);
    UINT id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, owner, NULL);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    Cmd c = MenuIdToCmd(id);
    if (c != Cmd::None)
        RunCommand(hWnd, c);
}

LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {

        // double-click: full screen on/off
	    case WM_LBUTTONDBLCLK:
		RunCommand(hWnd, Cmd::ViewFullscreen);
		break;
            
            
        // SURI: moved to another monitor / DPI changed: follow the suggested rectangle, rebuild fonts
        case WM_DPICHANGED: {
            if (!fullscreen && !stretch && !desktop.IsActive()) {
                const RECT* r = (const RECT*)lParam;
                SetWindowPos(hWnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            g_plugin.ReallocateFontsForDpi();
            return 0;
        }

        case WM_CLOSE: {
            exitRequested = true;   // SURI
            LeaveDesktopMode(true);
            DestroyWindow( hWnd );
            UnregisterClassW(kWindowClass, NULL);
            return 0;
        }

        case WM_DESTROY: {
            // SURI: destroyed while on the desktop without our asking: the shell went down
            // and took its windows (ours included) with it. Start a new copy that goes back
            // to the desktop once the shell is up, and shut this one down as usual.
            if (desktop.IsActive() && !exitRequested)
                OnWindowLostWithShell();
            if (resumePending) {   // SURI: no timer calls on a dying window
                resumePending = false;
                KillTimer(hWnd, kResumeTimerId);
            }
            LeaveDesktopMode(true);
            UnregisterHotKey(hWnd, kDesktopHotkeyId);
            WTSUnRegisterSessionNotification(hWnd);
            PostQuitMessage(0);
            break;
        }

        // SURI: desktop mode - hotkey, tray icon, shell and session events
        case WM_HOTKEY:
            if (wParam == kDesktopHotkeyId) {
                RunCommand(hWnd, Cmd::ViewDesktopMode);
                return 0;
            }
            break;

        case CTrayIcon::kMsg:
            if (LOWORD(lParam) == WM_RBUTTONUP)
                ShowTrayMenu(hWnd);
            else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
                LeaveDesktopMode();
            return 0;

        case kMsgDesktopOnStart:
            EnterDesktopMode(hWnd);
            return 0;

        case WM_WTSSESSION_CHANGE:
            if (wParam == WTS_SESSION_LOCK)
                desktop.OnEvent(DesktopEvent::SessionLocked);
            else if (wParam == WTS_SESSION_UNLOCK)
                desktop.OnEvent(DesktopEvent::SessionUnlocked);
            return 0;

        case WM_DISPLAYCHANGE:
            desktop.OnEvent(DesktopEvent::DisplayChanged);
            return g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_KEYDOWN: {
            g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);
        }
        break;

        case WM_SYSKEYDOWN: {
            // Alt+Enter, Alt+Shift, Alt+F4
            Cmd cmd = LookupKey(KeyScope::VKeyAlt, (unsigned)wParam);
            if (cmd == Cmd::None || !RunCommand(hWnd, cmd)) {
                g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);
            }
            break;
        }

        case WM_CONTEXTMENU:
            ShowContextMenu(hWnd, lParam);
            return 0;

        // SURI: folders / .milk files dropped on the window (DragFinish is called there)
        case WM_DROPFILES:
            g_plugin.OnDropFiles((HDROP)wParam);
            return 0;

        // SURI: sprite drag / wheel (CPlugin::SpriteMouseMessage); not on the desktop, where the
        // window sits behind the icons
        case WM_LBUTTONDOWN:
        case WM_MOUSEWHEEL:
            if (desktop.IsActive())
                break;
            return g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);

        case WM_COMMAND:
            // our command ids must never reach CPlugin::MyWindowProc (WM_COMMAND falls into WM_CHAR there)
            if (MenuIdToCmd(LOWORD(wParam)) != Cmd::None) {
                RunCommand(hWnd, MenuIdToCmd(LOWORD(wParam)));
                return 0;
            }
            return g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);

        // keep rendering while a menu is open (TrackPopupMenu runs its own modal loop)
        case WM_ENTERMENULOOP:
            SetTimer(hWnd, kMenuRenderTimerId, 16, NULL);
            break;

        case WM_EXITMENULOOP:
            KillTimer(hWnd, kMenuRenderTimerId);
            break;

        case WM_TIMER:
            if (wParam == CDesktopMode::kTimerId) {   // SURI
                desktop.OnTimer();
                return 0;
            }
            if (wParam == kResumeTimerId) {      // SURI
                ResumePoll(hWnd);
                return 0;
            }
            if (wParam == kMenuRenderTimerId) {
                GetAudioBuf(pcmLeftIn, pcmRightIn, SAMPLE_SIZE);
                RenderFrame();
                return 0;
            }
            return g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);

        default:
            // SURI: the shell (taskbar) came back - find the wallpaper window again, re-add the icon
            if (uMsg == TaskbarCreatedMessage()) {
                desktop.OnEvent(DesktopEvent::TaskbarCreated);
                tray.ReAdd();
                return 0;
            }
            if (uMsg == ActivateMessage()) {     // SURI: a second start
                OnActivateRequest(hWnd);
                return 0;
            }
            return g_plugin.PluginShellWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void RenderFrame() {
    {
        std::unique_lock<std::mutex> lock(pcmMutex);
        memcpy(pcmLeftOut, pcmLeftIn, SAMPLE_SIZE);
        memcpy(pcmRightOut, pcmRightIn, SAMPLE_SIZE);
        memset(pcmLeftIn, 0, SAMPLE_SIZE);
        memset(pcmRightIn, 0, SAMPLE_SIZE);
    }

    g_plugin.PluginRender(
        (unsigned char*) pcmLeftOut,
        (unsigned char*) pcmRightOut);
}

unsigned __stdcall CreateWindowAndRun(void* data) {
    HINSTANCE instance = (HINSTANCE) data;

#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetBreakAlloc(60);
#endif

    // Register the windows class
    WNDCLASSW wndClass;
    wndClass.style = CS_DBLCLKS;
    wndClass.lpfnWndProc = StaticWndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = instance;
    wndClass.hIcon = NULL;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = kWindowClass;   // SURI: our own name, found by a second start

    // SPOUT
	// Set Per Monitor awareness
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    
    if (!RegisterClassW(&wndClass)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_CLASS_ALREADY_EXISTS) {
            return 0;
        }
    }

    // SPOUT
	// make the window a fixed size to and avoid
	// resolution change for move instead of size
    int windowWidth = 800;
    int windowHeight = 800;

    RECT rc;
    SetRect(&rc, 0, 0, windowWidth, windowHeight);
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);
    
    // SPOUT
	// Centre on the desktop work area
	int WindowPosLeft = 0;
	int WindowPosTop = 0;
	RECT WorkArea;
	SystemParametersInfo(SPI_GETWORKAREA, 0, (LPVOID)&WorkArea, 0);
	WindowPosLeft += ((WorkArea.right - WorkArea.left) - windowWidth) / 2;
	WindowPosTop += ((WorkArea.bottom - WorkArea.top) - windowHeight) / 2;

	// SPOUT
	// Remove minimize and maximize
	DWORD dwStyle = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);

    // Create the render window
    HWND hwnd = CreateWindowW(
        kWindowClass,
        L"MilkDrop " MILKDROP_VERSION_STRW L" - Press F1 for help",
        // WS_OVERLAPPEDWINDOW, // SPOUT
		dwStyle,
		WindowPosLeft, // SPOUT
		WindowPosTop,
		// CW_USEDEFAULT,
		// CW_USEDEFAULT,
        (rc.right - rc.left),
        (rc.bottom - rc.top),
        0,
        NULL,
        instance,
        0);

    if (!hwnd) {
        DWORD dwError = GetLastError();
        return 0;
    }
    DragAcceptFiles(hwnd, TRUE);   // SURI: drop folders / presets on the window (WM_DROPFILES)

    if (!icon) {
        icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_PLUGIN_ICON));
    }

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon);

    // SURI: a /desktop-resume start stays hidden until it is back on the desktop (ResumePoll)
    if (!startedByResume)
        ShowWindow(hwnd, SW_SHOW);
    instanceWnd = CreateInstanceWindow(instance, hwnd);

    // SURI: desktop mode - global hotkey, lock/unlock notices, the tray's small icon
    hotkeyRegistered = RegisterHotKey(hwnd, kDesktopHotkeyId, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'D') != FALSE;
    if (!hotkeyRegistered)
        ERR(L"desktop: hotkey Ctrl+Alt+Shift+D not registered (error %u); use the menu", GetLastError());
    if (!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION))
        ERR(L"desktop: session notifications not registered (error %u)", GetLastError());
    trayIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_PLUGIN_ICON), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    TaskbarCreatedMessage();

   	// SPOUT
	// Make output resolution independent of the window size
	// The user can adjust this subsequently by resizing the BeatBox window
	// int lastWidth = windowWidth;
	// int lastHeight = windowHeight;
	int resolutionWidth = 800;
	int resolutionHeight = 800;

    g_plugin.PluginPreInitialize(0, 0);
    g_plugin.m_pfnIsFullscreen = IsFullscreenNow;   // SURI
    g_plugin.m_pfnRunCommand = RunCommand;
    // InitD3d(hwnd, windowWidth, windowHeight);
	InitD3d(hwnd, resolutionWidth, resolutionHeight);

    g_plugin.PluginInitialize(
        pD3DDevice,
        &d3dPp,
        hwnd,
        // windowWidth,
        // windowHeight);
		resolutionWidth,
		resolutionHeight);

    // SURI: tell the user once that the hotkey belongs to another program (the menu still works)
    if (!hotkeyRegistered)
        g_plugin.AddError(LoadResString(IDS_DESKTOP_HOTKEY_TAKEN), 5.0f, ERR_NOTIFY, false);

    // SURI: the last run lost its window together with the shell while on the desktop
    if (GetPrivateProfileIntW(L"settings", kDesktopOnStartKey, 0, g_plugin.GetConfigIniFile()) == 1) {
        WritePrivateProfileStringW(L"settings", kDesktopOnStartKey, L"0", g_plugin.GetConfigIniFile());
        if (!startedByResume) {   // SURI: a /desktop-resume start waits for the shell below
            LOG(L"desktop: resuming desktop mode after the shell restart");
            PostMessageW(hwnd, kMsgDesktopOnStart, 0, 0);
        }
    }

    // SURI: started by the copy that lost its window with the shell: same preset, then wait
    // (hidden) for the wallpaper host and go back to the desktop
    if (startedByResume) {
        if (!resumePreset.empty() && GetFileAttributesW(resumePreset.c_str()) != INVALID_FILE_ATTRIBUTES)
            g_plugin.LoadPreset(resumePreset.c_str(), 0.0f);
        LOG(L"desktop: /desktop-resume start, waiting for the shell (preset %s)",
            resumePreset.empty() ? L"none" : L"kept");
        resumePending = true;
        resumePolls = 0;
        SetTimer(hwnd, kResumeTimerId, kResumePollMs, NULL);
    }

    MSG msg;
    msg.message = WM_NULL;

    PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);
    while (WM_QUIT != msg.message) {
        // SURI: the shell took our window without a WM_DESTROY (see OnWindowLostWithShell)
        if (desktop.IsActive() && !IsWindow(hwnd)) {
            OnWindowLostWithShell();
            break;
        }
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            GetAudioBuf(pcmLeftIn, pcmRightIn, SAMPLE_SIZE);
            RenderFrame();
        }
    }

    LeaveDesktopMode(true);   // SURI: safety net (every exit path above already left it)
    if (instanceWnd) {        // SURI: a second start finds nothing from here on
        DestroyWindow(instanceWnd);
        instanceWnd = NULL;
    }

    g_plugin.MyWriteConfig();
    g_plugin.PluginQuit();

    DeinitD3d();

    thread = nullptr;
    threadId = 0;

    return 1;
}

void StartRenderThread(HINSTANCE instance) {
    thread = (HANDLE) _beginthreadex(
        nullptr,
        0,
        &CreateWindowAndRun,
        (void *) instance,
        0,
        &threadId);
}

int StartThreads(HINSTANCE instance) {

    HRESULT hr = S_OK;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        ERR(L"CoInitialize failed: hr = 0x%08x", hr);
        return -__LINE__;
    }
    CoUninitializeOnExit cuoe;

    // create a "loopback capture has started" event
    HANDLE hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hStartedEvent) {
        ERR(L"CreateEvent failed: last error is %u", GetLastError());
        return -__LINE__;
    }
    CloseHandleOnExit closeStartedEvent(hStartedEvent);

    // create a "loopback capture could not start" event
    HANDLE hGaveUpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hGaveUpEvent) {
        ERR(L"CreateEvent failed: last error is %u", GetLastError());
        return -__LINE__;
    }
    CloseHandleOnExit closeGaveUpEvent(hGaveUpEvent);

    // create a "stop capturing now" event (manual-reset: the capture thread checks it more than once)
    HANDLE hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == hStopEvent) {
        ERR(L"CreateEvent failed: last error is %u", GetLastError());
        return -__LINE__;
    }
    CloseHandleOnExit closeStopEvent(hStopEvent);

    // create arguments for loopback capture thread
    LoopbackCaptureThreadFunctionArguments threadArgs;
    threadArgs.hr = E_UNEXPECTED; // thread will overwrite this
    threadArgs.hStartedEvent = hStartedEvent;
    threadArgs.hGaveUpEvent = hGaveUpEvent;
    threadArgs.hStopEvent = hStopEvent;
    threadArgs.nFrames = 0;

    HANDLE hThread = CreateThread(
        NULL, 0,
        LoopbackCaptureThreadFunction, &threadArgs,
        0, NULL
    );
    if (NULL == hThread) {
        ERR(L"CreateThread failed: last error is %u", GetLastError());
        return -__LINE__;
    }
    CloseHandleOnExit closeThread(hThread);

    // wait for capture to start, or to give up (SURI: the user has been told and the
    // visualizer runs without audio until a device shows up)
    HANDLE waitArray[3] = { hStartedEvent, hGaveUpEvent, hThread };
    DWORD dwWaitResult;
    dwWaitResult = WaitForMultipleObjects(
        ARRAYSIZE(waitArray), waitArray,
        FALSE, INFINITE
    );

    if (WAIT_OBJECT_0 != dwWaitResult) {
        ERR(L"Starting without audio capture: wait result %u, hr = 0x%08x", dwWaitResult, threadArgs.hr);
    }

    // run the visualizer until its window closes
    StartRenderThread(instance);
    WaitForSingleObject(thread, INFINITE);

    // stop capture and wait for the capture thread to finish
    SetEvent(hStopEvent);
    WaitForSingleObject(hThread, INFINITE);

    // at this point the thread is definitely finished

    DWORD exitCode;
    if (!GetExitCodeThread(hThread, &exitCode)) {
        ERR(L"GetExitCodeThread failed: last error is %u", GetLastError());
        return -__LINE__;
    }

    if (0 != exitCode) {
        ERR(L"Loopback capture thread exit code is %u; expected 0", exitCode);
        return -__LINE__;
    }

    if (S_OK != threadArgs.hr) {
        ERR(L"Thread HRESULT is 0x%08x", threadArgs.hr);
        return -__LINE__;
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
    api_orig_hinstance = hInstance;

    // SURI: /desktop-resume (relaunched after the shell restarted, see RelaunchForResume)
    startTick = GetTickCount();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    startedByResume = ParseResumeArgs(argc, argv, &resumePreset);
    if (argv)
        LocalFree(argv);

    // SURI: single instance. A second start brings the running copy forward and exits before
    // any window, audio or D3D exists; a resume start waits for the old copy to finish.
    HANDLE instanceMutex = CreateMutexW(NULL, TRUE, kInstanceMutex);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!startedByResume) {
            ActivateRunningInstance();
            CloseHandle(instanceMutex);
            return 0;
        }
        DWORD w = WaitForSingleObject(instanceMutex, kResumeMutexWaitMs);
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED)
            ERR(L"single instance: the previous copy did not exit within %u s (wait %u), starting anyway",
                kResumeMutexWaitMs / 1000, w);
    }

    int result = StartThreads(hInstance);

    if (instanceMutex) {
        ReleaseMutex(instanceMutex);   // fails harmlessly if the wait above timed out
        CloseHandle(instanceMutex);
    }
    return result;
}

#ifdef DEBUG
struct _DEBUG_STATE {
    _DEBUG_STATE() {
    }

    ~_DEBUG_STATE() {
        _CrtDumpMemoryLeaks();
    }
};

_DEBUG_STATE ds;
#endif
