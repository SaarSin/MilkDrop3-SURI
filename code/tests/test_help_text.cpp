// test_help_text.cpp
//
// SURI: the help text lists every bound key, has 5 pages and none of the removed header.

#include "mini_test.h"
#include "..\vis_milk2\features\help_text.h"
#include "..\vis_milk2\features\ui_scale.h"
#include "..\vis_milk2\features\commands.h"
#include <wctype.h>

TEST(TestHelpText_FivePages) {
    std::vector<std::wstring> p = SplitHelpPages(HelpText());
    CHECK(p.size() == 5);
    CHECK(p[0].find(L"Presets") == 0); CHECK(p[1].find(L"View") == 0); CHECK(p[2].find(L"Effects") == 0);
    CHECK(p[3].find(L"Editing") == 0); CHECK(p[4].find(L"Desktop") == 0);
}
// SURI: true if a key row on that page has token as one whitespace-separated word of its
// key column (case-insensitive), so a one-letter key must be a key of its own, not a letter
// somewhere in the text.
static std::wstring LowerW(std::wstring s) { for (wchar_t& c : s) c = (wchar_t)towlower(c); return s; }
static bool HelpRowOnPage(const std::wstring& token, size_t page) {
    std::vector<std::wstring> pages = SplitHelpPages(HelpText());
    if (page >= pages.size()) return false;
    std::wstring title, want = L" " + LowerW(token) + L" ";
    for (const HelpRow& r : ParseHelpPage(pages[page], &title)) {
        std::wstring k = L" " + LowerW(r.key) + L" ";
        for (wchar_t& c : k) if (iswspace(c)) c = L' ';
        if (k.find(want) != std::wstring::npos) return true;
    }
    return false;
}
static bool HelpHasKeyToken(const std::wstring& token) {
    for (size_t p = 0; p < SplitHelpPages(HelpText()).size(); p++)
        if (HelpRowOnPage(token, p)) return true;
    return false;
}
TEST(TestHelpText_EveryKeyListed) {
    std::wstring t = HelpText();
    size_t n = 0; const KeyBinding* b = KeyBindings(&n);
    for (size_t i = 0; i < n; i++) {
        std::wstring label = KeyLabel(b[i]);
        if (!HelpHasKeyToken(label)) { printf("  missing key: %ls\n", label.c_str()); CHECK(!"key missing from help"); }
    }
    CHECK(HelpHasKeyToken(L"Right-click"));
    CHECK(HelpHasKeyToken(L"Double-click"));
    CHECK(HelpHasKeyToken(L"Ctrl+Alt+Shift+D"));
    CHECK(t.find(L"2077") == std::wstring::npos);
}
TEST(TestHelpText_KeyTokenNotSubstring) {
    CHECK(!HelpHasKeyToken(L"x"));    // "next" has an x, but no key column is just "x"
    CHECK(!HelpHasKeyToken(L"preset"));
    CHECK(!HelpHasKeyToken(L"Shift")); // only inside "Shift+Q" and similar
    CHECK(HelpHasKeyToken(L"h"));
}
TEST(TestHelpText_LegacyKeysOnTheirPage) {   // SURI: cleanup item 1 (pages: 0 Presets, 1 View, 3 Editing)
    CHECK(HelpRowOnPage(L"Shift+R", 0) || HelpRowOnPage(L"R", 0));
    CHECK(HelpRowOnPage(L"Scroll", 0));      // Scroll Lock
    CHECK(HelpRowOnPage(L"Esc", 1));
    CHECK(HelpRowOnPage(L"Ctrl+T", 1));
    CHECK(HelpRowOnPage(L"0-9", 1));
    CHECK(HelpRowOnPage(L"*", 1));
    CHECK(HelpRowOnPage(L"K", 1));
    CHECK(HelpRowOnPage(L"Shift+K", 1));
    CHECK(HelpRowOnPage(L"Ctrl+K", 1));
    CHECK(HelpRowOnPage(L"Enter", 0));       // L list keys
    CHECK(HelpRowOnPage(L"Del", 0));
    CHECK(HelpRowOnPage(L"Ins", 0));
    CHECK(HelpRowOnPage(L"Home", 0));
}
TEST(TestHelpText_PagesParse) {
    std::vector<std::wstring> p = SplitHelpPages(HelpText());
    const wchar_t* titles[] = { L"Presets", L"View", L"Effects", L"Editing", L"Desktop" };
    for (size_t i = 0; i < p.size() && i < 5; i++) {
        std::wstring title;
        std::vector<HelpRow> rows = ParseHelpPage(p[i], &title);
        CHECK(title == titles[i]);
        CHECK(!rows.empty());
        for (const HelpRow& r : rows) CHECK(!r.text.empty());
    }
}
// SURI: final fix item 1 - the Desktop page describes the shipped feature, the Effects page
// uses the menu's own labels
TEST(TestHelpText_DesktopAndEffectsPages) {
    std::vector<std::wstring> p = SplitHelpPages(HelpText());
    CHECK(p.size() == 5);
    if (p.size() != 5) return;
    const std::wstring& fx = p[2];
    const wchar_t* labels[] = { L"Mirror left/right", L"Mirror top/bottom", L"Kaleidoscope", L"Ripple",
        L"Zoom in", L"Swap axes", L"Tunnel", L"Invert colours", L"Black and white", L"Posterize",
        L"Colour spin", L"Brighter", L"Darker", L"More contrast", L"Less contrast", L"Clear effects" };
    for (const wchar_t* l : labels)
        if (fx.find(l) == std::wstring::npos) { printf("  missing effect label: %ls\n", l); CHECK(!"label missing"); }
    CHECK(fx.find(L"Clear all effects") == std::wstring::npos);
    const std::wstring& d = p[4];
    CHECK(d.find(L"upcoming") == std::wstring::npos);
    CHECK(d.find(L"Run as desktop background") != std::wstring::npos);
    CHECK(d.find(L"tray") != std::wstring::npos);
    CHECK(d.find(L"Explorer") != std::wstring::npos);
    CHECK(HelpRowOnPage(L"Ctrl+Alt+Shift+D", 4));
}
