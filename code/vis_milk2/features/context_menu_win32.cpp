// context_menu_win32.cpp
//
// SURI: builds the right-click HMENU from MenuLayout() and the current AppSnapshot.

#include "context_menu.h"
#include "..\resstrings.h"

static std::wstring AutoChangeLabel(float seconds)
{
    if (seconds == 0.0f)
        return L"off";
    if (seconds == 16.0f)
        return L"15 sec";   // what F3 shows for its first step
    return std::to_wstring((int)seconds) + L" sec";
}

static std::wstring ItemText(Cmd c, UINT labelId, const AppSnapshot& s)
{
    std::wstring text = LoadResString(labelId);
    if (c == Cmd::FpsLimitCycle)
        text += L": " + FpsLimitMessage(s.fpsLimit);
    else if (c == Cmd::AutoChangeCycle)
        text += L": " + AutoChangeLabel(s.autoChangeSeconds);

    std::wstring key = PrimaryKeyLabel(c);
    if (!key.empty())
        text += L"\t" + key;
    return text;
}

static void AppendNodes(HMENU menu, const std::vector<MenuNode>& nodes, const AppSnapshot& s)
{
    for (const MenuNode& n : nodes) {
        switch (n.kind) {
        case MenuNodeKind::Separator:
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            break;
        case MenuNodeKind::Submenu: {
            HMENU sub = CreatePopupMenu();
            AppendNodes(sub, n.children, s);
            AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, LoadResString(n.labelId));
            break;
        }
        case MenuNodeKind::Item: {
            CmdState st = QueryState(n.cmd, s);
            UINT flags = MF_STRING | (st.enabled ? 0 : MF_GRAYED) | (st.checked ? MF_CHECKED : 0);
            AppendMenuW(menu, flags, CmdToMenuId(n.cmd), ItemText(n.cmd, n.labelId, s).c_str());
            break;
        }
        }
    }
}

// SURI: any menu layout (the main menu, the tray menu)
HMENU BuildMenu(const std::vector<MenuNode>& nodes, const AppSnapshot& s)
{
    HMENU menu = CreatePopupMenu();
    AppendNodes(menu, nodes, s);
    return menu;
}

HMENU BuildContextMenu(const AppSnapshot& s)
{
    return BuildMenu(MenuLayout(), s);
}
