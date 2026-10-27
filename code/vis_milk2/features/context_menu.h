// context_menu.h
//
// SURI: the right-click menu. MenuLayout() is the menu as data (pure, unit-tested in
// code/tests/test_context_menu.cpp); BuildContextMenu() turns it into an HMENU.

#pragma once

#include "commands.h"
#include <vector>

enum class MenuNodeKind { Item, Separator, Submenu };

struct MenuNode {
    MenuNodeKind kind;
    Cmd cmd;                        // Item only
    UINT labelId;                   // string table id (Item, Submenu)
    std::vector<MenuNode> children; // Submenu only
};

const std::vector<MenuNode>& MenuLayout();
UINT  CmdLabelId(Cmd c);                                   // 0 if none
POINT MenuAnchor(LPARAM lParam, const RECT& windowRect);   // lParam == -1 (keyboard) -> window centre

// context_menu_win32.cpp (MilkDrop3.exe only); the caller destroys the menu
HMENU BuildContextMenu(const AppSnapshot& s);
HMENU BuildMenu(const std::vector<MenuNode>& nodes, const AppSnapshot& s);   // SURI: any layout (tray menu)
