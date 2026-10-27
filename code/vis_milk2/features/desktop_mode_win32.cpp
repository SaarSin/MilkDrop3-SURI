// desktop_mode_win32.cpp
//
// SURI: runs the render window as the desktop background (behind the icons) and shows a
// notification-area icon meanwhile. The decisions (which window hosts us, what to do on
// a shell restart) are the pure functions in desktop_mode.cpp; this file does the calls.

#include "desktop_mode.h"
#include <shellapi.h>
#include "..\..\audio\log.h"

// Progman message that makes the shell create the WorkerW window behind the icons
static const UINT kSpawnWorkerW = 0x052C;

UINT TaskbarCreatedMessage()
{
    static const UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

static std::wstring ClassOf(HWND h)
{
    wchar_t name[64] = L"";
    GetClassNameW(h, name, ARRAYSIZE(name));
    return name;
}

static BOOL CALLBACK CollectTopWindow(HWND h, LPARAM lp)
{
    std::vector<TopWindowInfo>* top = (std::vector<TopWindowInfo>*)lp;
    bool defView = FindWindowExW(h, NULL, L"SHELLDLL_DefView", NULL) != NULL;
    top->push_back(TopWindowInfo{ h, ClassOf(h), defView });
    return TRUE;
}

// both layouts: a top-level WorkerW after the icons' window (Windows 10 / older 11),
// or a WorkerW child of Progman (Windows 11 24H2)
static HWND FindWallpaperHost(bool quiet)
{
    HWND progman = FindWindowW(L"Progman", NULL);
    if (!progman) {
        if (!quiet) ERR(L"desktop: no Progman window");
        return NULL;
    }
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(progman, kSpawnWorkerW, 0, 0, SMTO_NORMAL, 1000, &ignored);

    std::vector<TopWindowInfo> top;
    EnumWindows(CollectTopWindow, (LPARAM)&top);
    HWND progmanChild = FindWindowExW(progman, NULL, L"WorkerW", NULL);
    HWND host = PickWallpaperHost(top, progmanChild);
    if (!quiet)
        LOG(L"desktop: search progman=%p progmanWorkerW=%p windows=%u -> host=%p",
            progman, progmanChild, (unsigned)top.size(), host);
    return host;
}

static BOOL CALLBACK FindDefView(HWND h, LPARAM lp)
{
    HWND defView = FindWindowExW(h, NULL, L"SHELLDLL_DefView", NULL);
    if (!defView)
        return TRUE;
    *(HWND*)lp = defView;
    return FALSE;
}

// the icons do not repaint by themselves once a window appears below them
static void RedrawDesktopIcons()
{
    HWND defView = NULL;
    EnumWindows(FindDefView, (LPARAM)&defView);
    HWND icons = defView ? FindWindowExW(defView, NULL, L"SysListView32", NULL) : NULL;
    if (icons)
        RedrawWindow(icons, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

// SURI: our last frame stays on the wallpaper host after we leave, and redrawing the host does
// not clear it (tested on Windows 10 19045: the WorkerW does not paint the wallpaper itself).
// So the host is hidden - the shell's own wallpaper window (Progman) below it shows again - and
// Attach shows it again next time. Never SPI_SETDESKWALLPAPER: with a slideshow, Spotlight or a
// wallpaper per monitor, setting "the" wallpaper again would replace the user's setup.
static void RepaintWallpaper(HWND oldHost)
{
    const UINT flags = RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW;
    if (oldHost && IsWindow(oldHost)) {
        ShowWindow(oldHost, SW_HIDE);
        RedrawWindow(oldHost, NULL, NULL, flags);
    }
    HWND progman = FindWindowW(L"Progman", NULL);
    if (progman)
        RedrawWindow(progman, NULL, NULL, flags);
    RedrawDesktopIcons();
}

// --- CDesktopMode -----------------------------------------------------------

// SURI: polled by a /desktop-resume start until Explorer is back
bool CDesktopMode::HostAvailable()
{
    return FindWallpaperHost(true) != NULL;
}

bool CDesktopMode::Enter(HWND hwnd, ResizeBackBufferFn resize)
{
    if (m_active)
        return true;

    HWND host = FindWallpaperHost(false);
    if (!host) {
        ERR(L"desktop: no wallpaper host, staying in a window");
        return false;
    }

    m_hwnd = hwnd;
    m_resize = resize;
    m_savedStyle = GetWindowLongW(hwnd, GWL_STYLE);
    m_savedExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    GetWindowRect(hwnd, &m_savedRect);

    SetWindowLongW(hwnd, GWL_EXSTYLE, WS_EX_NOACTIVATE);
    if (!Attach(host)) {
        ERR(L"desktop: SetParent failed (error %u)", GetLastError());
        SetWindowLongW(hwnd, GWL_STYLE, m_savedStyle);
        SetWindowLongW(hwnd, GWL_EXSTYLE, m_savedExStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        return false;
    }

    m_active = true;
    m_ticksDetached = 0;
    SetTimer(hwnd, kTimerId, 250, NULL);
    LOG(L"desktop: host=%p", host);
    return true;
}

void CDesktopMode::Leave(bool show)
{
    if (!m_active)
        return;
    m_active = false;
    HWND oldHost = m_host;

    if (m_hwnd && IsWindow(m_hwnd)) {
        KillTimer(m_hwnd, kTimerId);
        if (!show)
            ShowWindow(m_hwnd, SW_HIDE);   // SURI: quitting - hidden before it becomes top-level
        SetParent(m_hwnd, NULL);
        SetWindowLongW(m_hwnd, GWL_STYLE, m_savedStyle);
        SetWindowLongW(m_hwnd, GWL_EXSTYLE, m_savedExStyle);

        int width = m_savedRect.right - m_savedRect.left;
        int height = m_savedRect.bottom - m_savedRect.top;
        m_resize(width, height);
        HWND band = (m_savedExStyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST;
        if (show) {
            SetWindowPos(m_hwnd, band, m_savedRect.left, m_savedRect.top, width, height,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            SetForegroundWindow(m_hwnd);
        } else {
            SetWindowPos(m_hwnd, band, m_savedRect.left, m_savedRect.top, width, height,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_HIDEWINDOW);
        }
    }
    m_host = NULL;
    m_hwnd = NULL;

    RepaintWallpaper(oldHost);
    LOG(L"desktop: leave");
}

void CDesktopMode::OnWindowDestroyed()
{
    // the window is going away (possibly with the shell): no window calls from here on
    m_hwnd = NULL;
    // SURI: nor on the host, which went down with the shell (its handle may be reused later)
    m_host = NULL;
}

bool CDesktopMode::HostValid() const
{
    // a handle alone is not enough: after the shell restarts it can be reused by another window
    if (!m_host || !IsWindow(m_host) || ClassOf(m_host) != L"WorkerW")
        return false;
    HWND parent = GetParent(m_host);
    return parent == NULL || parent == FindWindowW(L"Progman", NULL);
}

bool CDesktopMode::Attach(HWND host)
{
    // SURI: a host we hid when leaving last time (RepaintWallpaper) must be shown again
    if (!IsWindowVisible(host))
        ShowWindow(host, SW_SHOWNA);
    // WS_CHILD goes on before SetParent (and comes off after SetParent(NULL) in Detach)
    SetWindowLongW(m_hwnd, GWL_STYLE, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS);
    if (!SetParent(m_hwnd, host)) {
        SetWindowLongW(m_hwnd, GWL_STYLE, WS_POPUP);
        ShowWindow(m_hwnd, SW_HIDE);
        return false;
    }
    m_host = host;
    FitToVirtualScreen();
    RedrawDesktopIcons();
    return true;
}

void CDesktopMode::Detach()
{
    if (!m_host || !m_hwnd)
        return;   // already detached
    SetParent(m_hwnd, NULL);
    ShowWindow(m_hwnd, SW_HIDE);
    SetWindowLongW(m_hwnd, GWL_STYLE, WS_POPUP);
    m_host = NULL;
    m_ticksDetached = 0;
    LOG(L"desktop: host lost, detached");
}

void CDesktopMode::FitToVirtualScreen()
{
    RECT r;
    r.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.right = r.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.bottom = r.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    MapWindowPoints(HWND_DESKTOP, m_host, (POINT*)&r, 2);

    int width = r.right - r.left;
    int height = r.bottom - r.top;
    m_resize(width, height);
    SetWindowPos(m_hwnd, HWND_TOP, r.left, r.top, width, height,
                 SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

void CDesktopMode::OnEvent(DesktopEvent e)
{
    if (!m_active || !m_hwnd)
        return;
    if (e == DesktopEvent::SessionLocked || e == DesktopEvent::SessionUnlocked)
        LOG(L"desktop: session %s", e == DesktopEvent::SessionLocked ? L"locked" : L"unlocked");

    switch (DecideDesktopAction(m_active, HostValid(), e)) {
    case DesktopAction::Detach:
        Detach();
        break;
    case DesktopAction::Reattach: {
        Detach();   // a stale host (no-op when already detached)
        HWND host = FindWallpaperHost(false);
        if (host && Attach(host))
            LOG(L"desktop: reattach host=%p", host);
        else
            LOG(L"desktop: reattach failed, will retry");
        break;
    }
    case DesktopAction::Resize:
        FitToVirtualScreen();
        LOG(L"desktop: resize");
        break;
    default:
        break;
    }
}

void CDesktopMode::OnTimer()
{
    if (!m_active || !m_hwnd)
        return;
    if (m_host) {
        OnEvent(DesktopEvent::HostLost);
        return;
    }
    // detached: the shell may be back before (or without) a TaskbarCreated reaching us
    if (++m_ticksDetached % 8 == 0) {
        DesktopAction a = DecideDesktopAction(m_active, HostValid(), DesktopEvent::TaskbarCreated);
        if (a == DesktopAction::Reattach) {
            HWND host = FindWallpaperHost(true);
            if (host && Attach(host))
                LOG(L"desktop: reattach host=%p (retry)", host);
        }
    }
}

// --- CTrayIcon --------------------------------------------------------------

static const wchar_t kTrayOwnerClass[] = L"SuriDesktopTrayOwner";

static LRESULT CALLBACK TrayOwnerProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND target = (HWND)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (target) {
        if (msg == CTrayIcon::kMsg || msg == TaskbarCreatedMessage() || msg == WM_DISPLAYCHANGE) {
            PostMessageW(target, msg, wp, lp);
            return 0;
        }
        // the tray menu belongs to this window; the render window keeps drawing meanwhile
        if (msg == WM_ENTERMENULOOP || msg == WM_EXITMENULOOP) {
            SendMessageW(target, msg, wp, lp);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

bool CTrayIcon::Add(HWND hwnd, HICON icon, const wchar_t* tip)
{
    if (m_owner)
        return true;

    HINSTANCE inst = GetModuleHandleW(NULL);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = TrayOwnerProc;
        wc.hInstance = inst;
        wc.lpszClassName = kTrayOwnerClass;
        registered = RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    // hidden, top-level (it must get the TaskbarCreated broadcast), not on the taskbar
    m_owner = CreateWindowExW(WS_EX_TOOLWINDOW, kTrayOwnerClass, L"", WS_POPUP,
                              0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!m_owner) {
        ERR(L"desktop: tray window failed (error %u)", GetLastError());
        return false;
    }
    SetWindowLongPtrW(m_owner, GWLP_USERDATA, (LONG_PTR)hwnd);
    m_target = hwnd;
    m_icon = icon;
    m_tip = tip ? tip : L"";

    if (!Notify(NIM_ADD)) {
        ERR(L"desktop: tray icon not added");
        return false;
    }
    return true;
}

void CTrayIcon::Remove()
{
    if (!m_owner)
        return;
    Notify(NIM_DELETE);
    DestroyWindow(m_owner);
    m_owner = NULL;
    m_target = NULL;
}

void CTrayIcon::ReAdd()
{
    if (!m_owner)
        return;
    Notify(NIM_DELETE);   // normally gone with the old shell already
    if (Notify(NIM_ADD))
        LOG(L"desktop: tray icon added again");
    else
        ERR(L"desktop: tray icon not added again");
}

bool CTrayIcon::Notify(DWORD message)
{
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_owner;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kMsg;
    nid.hIcon = m_icon;
    wcsncpy_s(nid.szTip, m_tip.c_str(), _TRUNCATE);
    return Shell_NotifyIconW(message, &nid) != FALSE;
}
