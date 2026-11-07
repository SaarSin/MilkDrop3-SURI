// test_context_menu.cpp
//
// SURI: tests for the right-click menu layout (features/context_menu.*).

#include "mini_test.h"
#include "..\vis_milk2\features\context_menu.h"
#include "..\vis_milk2\resource.h"

static void CountItems(const std::vector<MenuNode>& nodes, int* seen) {
    for (const MenuNode& n : nodes) {
        if (n.kind == MenuNodeKind::Item) seen[(int)n.cmd]++;
        if (n.kind == MenuNodeKind::Submenu) CountItems(n.children, seen);
    }
}

static void CheckLabels(const std::vector<MenuNode>& nodes) {
    for (const MenuNode& n : nodes) {
        if (n.kind == MenuNodeKind::Item) {
            CHECK(n.labelId != 0);
            CHECK(n.labelId == CmdLabelId(n.cmd));
        }
        if (n.kind == MenuNodeKind::Submenu) {
            CHECK(n.labelId != 0);
            CheckLabels(n.children);
        }
    }
}

static void CheckShape(const std::vector<MenuNode>& nodes) {
    CHECK(!nodes.empty());
    CHECK(nodes.front().kind != MenuNodeKind::Separator);
    CHECK(nodes.back().kind != MenuNodeKind::Separator);
    for (size_t i = 1; i < nodes.size(); i++)
        CHECK(!(nodes[i].kind == MenuNodeKind::Separator && nodes[i - 1].kind == MenuNodeKind::Separator));
    for (const MenuNode& n : nodes)
        if (n.kind == MenuNodeKind::Submenu) CheckShape(n.children);
}

TEST(TestMenu_EveryCommandExactlyOnce) {
    int seen[(int)Cmd::Count] = {};
    CountItems(MenuLayout(), seen);
    for (int c = 1; c < (int)Cmd::Count; c++) CHECK(seen[c] == 1);
    CHECK(seen[0] == 0);
}

TEST(TestMenu_LabelsExist) {
    CheckLabels(MenuLayout());
}

TEST(TestMenu_NoEmptySubmenuNoEdgeSeparators) {
    CheckShape(MenuLayout());
}

TEST(TestMenu_Anchor) {   // Review Focus 4
    RECT r = { 100, 100, 900, 700 };
    POINT k = MenuAnchor((LPARAM)-1, r);                   CHECK(k.x == 500 && k.y == 400);
    POINT m = MenuAnchor(MAKELPARAM(250, 300), r);         CHECK(m.x == 250 && m.y == 300);
    POINT neg = MenuAnchor(MAKELPARAM((WORD)-500, 20), r); CHECK(neg.x == -500 && neg.y == 20);
}
TEST(TestMenu_MotionSubmenuInView) {   // SURI: calm motion
    const MenuNode* view = nullptr;
    for (const MenuNode& n : MenuLayout())
        if (n.kind == MenuNodeKind::Submenu && n.labelId == IDS_MENU_VIEW) view = &n;
    CHECK(view != nullptr);
    const MenuNode* motion = nullptr;
    for (const MenuNode& n : view->children)
        if (n.kind == MenuNodeKind::Submenu && n.labelId == IDS_MENU_MOTION) motion = &n;
    CHECK(motion != nullptr);
    const Cmd want[] = { Cmd::SpeedUp, Cmd::SpeedDown, Cmd::SensitivityUp, Cmd::SensitivityDown, Cmd::CalmReset,
                         Cmd::None, Cmd::AntiFlickerOff, Cmd::AntiFlickerSoft, Cmd::AntiFlickerStrong,
                         Cmd::None, Cmd::CalmMeterToggle, Cmd::CalmApplySuggestion };   // SURI: calm meter (phase 15)
    CHECK(motion->children.size() == 12);
    for (size_t i = 0; i < 12 && i < motion->children.size(); i++) {
        const MenuNode& n = motion->children[i];
        if (want[i] == Cmd::None) CHECK(n.kind == MenuNodeKind::Separator);
        else CHECK(n.kind == MenuNodeKind::Item && n.cmd == want[i]);
    }
}
