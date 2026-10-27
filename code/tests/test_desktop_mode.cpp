// test_desktop_mode.cpp
//
// SURI: tests for wallpaper host selection, reattach rules and the tray menu layout.

#include "mini_test.h"
#include "..\vis_milk2\features\desktop_mode.h"
#include "..\vis_milk2\resource.h"

TEST(TestDesktop_PickHost_Classic) {   // Win10: WorkerW sibling after the window holding DefView
    HWND a = (HWND)1, b = (HWND)2, c = (HWND)3;
    std::vector<TopWindowInfo> top = { {a, L"WorkerW", true}, {b, L"WorkerW", false}, {c, L"Progman", false} };
    CHECK(PickWallpaperHost(top, NULL) == b);
}
TEST(TestDesktop_PickHost_ProgmanChild) {   // Win11 24H2
    std::vector<TopWindowInfo> top = { {(HWND)3, L"Progman", true} };
    CHECK(PickWallpaperHost(top, (HWND)9) == (HWND)9);
}
TEST(TestDesktop_PickHost_None) {
    std::vector<TopWindowInfo> top = { {(HWND)3, L"Progman", true} };
    CHECK(PickWallpaperHost(top, NULL) == NULL);
}
TEST(TestDesktop_ReattachDecision) {   // Review Focus 4
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::HostLost)       == DesktopAction::Detach);
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::TaskbarCreated) == DesktopAction::Reattach);
    CHECK(DecideDesktopAction(true,  true,  DesktopEvent::DisplayChanged) == DesktopAction::Resize);
    CHECK(DecideDesktopAction(true,  true,  DesktopEvent::SessionUnlocked)== DesktopAction::None);
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::SessionUnlocked)== DesktopAction::Reattach);
    CHECK(DecideDesktopAction(false, false, DesktopEvent::TaskbarCreated) == DesktopAction::None);
}

static void CheckTrayLabels(const std::vector<MenuNode>& nodes) {
    for (const MenuNode& n : nodes) {
        if (n.kind == MenuNodeKind::Item) {
            CHECK(CmdLabelId(n.cmd) != 0);
            CHECK(n.labelId == CmdLabelId(n.cmd));
        }
        if (n.kind == MenuNodeKind::Submenu) {
            CHECK(n.labelId != 0);
            CheckTrayLabels(n.children);
        }
    }
}

TEST(TestDesktop_TrayMenuCommandsHaveLabels) {
    const std::vector<MenuNode>& tray = TrayMenuLayout();
    CHECK(!tray.empty());
    CHECK(tray.front().kind == MenuNodeKind::Item);
    CHECK(tray.front().cmd == Cmd::ViewDesktopMode);
    CheckTrayLabels(tray);
}
TEST(TestDesktop_TrayMenuHasEffectsSubmenu) {   // SURI: final fix T8
    bool found = false;
    for (const MenuNode& n : TrayMenuLayout())
        if (n.kind == MenuNodeKind::Submenu && n.labelId == IDS_MENU_EFFECTS && !n.children.empty())
            found = true;
    CHECK(found);
}

// SURI: fix round 2 - relaunch after the shell restarts
TEST(TestDesktop_ParseResumeArgs) {
    std::wstring preset;
    const wchar_t* none[] = { L"MilkDrop3.exe" };
    CHECK(!ParseResumeArgs(1, none, &preset));
    CHECK(preset.empty());

    const wchar_t* bare[] = { L"MilkDrop3.exe", L"/desktop-resume" };
    CHECK(ParseResumeArgs(2, bare, &preset));
    CHECK(preset.empty());

    const wchar_t* withPreset[] = { L"C:\\a b\\MilkDrop3.exe", L"/Desktop-Resume", L"C:\\p q\\pre.milk" };
    CHECK(ParseResumeArgs(3, withPreset, &preset));
    CHECK(preset == L"C:\\p q\\pre.milk");

    const wchar_t* otherSwitch[] = { L"MilkDrop3.exe", L"/desktop-resume", L"/other" };
    CHECK(ParseResumeArgs(3, otherSwitch, &preset));
    CHECK(preset.empty());

    const wchar_t* unrelated[] = { L"MilkDrop3.exe", L"pre.milk" };
    CHECK(!ParseResumeArgs(2, unrelated, &preset));
    CHECK(!ParseResumeArgs(0, nullptr, &preset));
    CHECK(ParseResumeArgs(2, bare, nullptr));
}
TEST(TestDesktop_ShouldRelaunch) {
    CHECK(ShouldRelaunch(1000, 0, false));          // a normal start may always relaunch
    CHECK(ShouldRelaunch(5, 0, false));
    CHECK(!ShouldRelaunch(10000, 1000, true));      // resumed 9 s ago: a loop, stop
    CHECK(!ShouldRelaunch(30999, 1000, true));      // 29.999 s
    CHECK(ShouldRelaunch(31000, 1000, true));       // 30 s exactly
    CHECK(ShouldRelaunch(100000, 1000, true));
    CHECK(!ShouldRelaunch(0x00001000u, 0xFFFFF000u, true));   // tick wrapped: 8 s
    CHECK(ShouldRelaunch(0x00010000u, 0xFFFF0000u, true));    // tick wrapped: 131 s
}
TEST(TestDesktop_ResumeCommandLine) {
    CHECK(BuildResumeCommandLine(L"C:\\a b\\MilkDrop3.exe", L"") ==
          L"\"C:\\a b\\MilkDrop3.exe\" /desktop-resume");
    CHECK(BuildResumeCommandLine(L"C:\\a b\\MilkDrop3.exe", L"C:\\p q\\pre.milk") ==
          L"\"C:\\a b\\MilkDrop3.exe\" /desktop-resume \"C:\\p q\\pre.milk\"");
    // round trip through the parser
    std::wstring preset;
    const wchar_t* argv[] = { L"C:\\a b\\MilkDrop3.exe", L"/desktop-resume", L"C:\\p q\\pre.milk" };
    CHECK(ParseResumeArgs(3, argv, &preset) && preset == L"C:\\p q\\pre.milk");
}
