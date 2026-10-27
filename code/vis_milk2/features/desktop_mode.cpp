// desktop_mode.cpp
//
// SURI: wallpaper host selection and reattach rules - see desktop_mode.h.

#include "desktop_mode.h"
#include "..\resource.h"

HWND PickWallpaperHost(const std::vector<TopWindowInfo>& top, HWND progmanWorkerChild)
{
    // Windows 11 24H2: the wallpaper host is a WorkerW child of Progman
    if (progmanWorkerChild != NULL)
        return progmanWorkerChild;

    // Windows 10 / older 11: the WorkerW that comes after the window holding the icons (DefView)
    for (size_t i = 0; i < top.size(); i++) {
        if (!top[i].hasDefView)
            continue;
        for (size_t j = i + 1; j < top.size(); j++)
            if (top[j].className == L"WorkerW")
                return top[j].hwnd;
        break;
    }
    return NULL;
}

DesktopAction DecideDesktopAction(bool active, bool hostValid, DesktopEvent e)
{
    if (!active)
        return DesktopAction::None;
    switch (e) {
    case DesktopEvent::HostLost:
        return hostValid ? DesktopAction::None : DesktopAction::Detach;
    case DesktopEvent::TaskbarCreated:
    case DesktopEvent::SessionUnlocked:
        return hostValid ? DesktopAction::None : DesktopAction::Reattach;
    case DesktopEvent::DisplayChanged:
        return hostValid ? DesktopAction::Resize : DesktopAction::Reattach;
    default:
        return DesktopAction::None;
    }
}

// SURI: relaunch after the shell restarts (see desktop_mode.h)
static const wchar_t kResumeSwitch[] = L"/desktop-resume";
static const DWORD kRelaunchMinIntervalMs = 30000;

bool ParseResumeArgs(int argc, const wchar_t* const* argv, std::wstring* presetPath)
{
    if (presetPath)
        presetPath->clear();
    if (!argv)
        return false;
    for (int i = 1; i < argc; i++) {
        if (!argv[i] || _wcsicmp(argv[i], kResumeSwitch) != 0)
            continue;
        if (presetPath && i + 1 < argc && argv[i + 1] && argv[i + 1][0] && argv[i + 1][0] != L'/')
            *presetPath = argv[i + 1];
        return true;
    }
    return false;
}

bool ShouldRelaunch(DWORD nowTick, DWORD startedAtTick, bool startedByResume)
{
    if (!startedByResume)
        return true;
    return (DWORD)(nowTick - startedAtTick) >= kRelaunchMinIntervalMs;
}

std::wstring BuildResumeCommandLine(const std::wstring& exePath, const std::wstring& presetPath)
{
    std::wstring cmd = L"\"" + exePath + L"\" " + kResumeSwitch;
    if (!presetPath.empty())
        cmd += L" \"" + presetPath + L"\"";
    return cmd;
}

static MenuNode TrayItem(Cmd c)
{
    return MenuNode{ MenuNodeKind::Item, c, CmdLabelId(c), {} };
}

static std::vector<MenuNode> BuildTrayLayout()
{
    std::vector<MenuNode> v;
    v.push_back(TrayItem(Cmd::ViewDesktopMode));
    v.push_back(MenuNode{ MenuNodeKind::Separator, Cmd::None, 0, {} });
    v.push_back(TrayItem(Cmd::PresetNext));
    v.push_back(TrayItem(Cmd::PresetRandom));
    v.push_back(TrayItem(Cmd::PresetLockToggle));
    v.push_back(MenuNode{ MenuNodeKind::Separator, Cmd::None, 0, {} });
    // the Effects submenu is the main menu's own node, not a second hand-written list
    for (const MenuNode& n : MenuLayout())
        if (n.kind == MenuNodeKind::Submenu && n.labelId == IDS_MENU_EFFECTS)
            v.push_back(n);
    v.push_back(TrayItem(Cmd::FxClearAll));
    v.push_back(MenuNode{ MenuNodeKind::Separator, Cmd::None, 0, {} });
    v.push_back(TrayItem(Cmd::AppQuit));
    return v;
}

const std::vector<MenuNode>& TrayMenuLayout()
{
    static const std::vector<MenuNode> layout = BuildTrayLayout();
    return layout;
}
