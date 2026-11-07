// test_preset_filter.cpp
//
// SURI: tests for features/preset_filter.* (type-to-filter in the preset list).

#include "mini_test.h"
#include <wchar.h>
#include "..\vis_milk2\features\preset_filter.h"

TEST(TestPresetFilter_EmptyMatchesAll) {
    CHECK(PresetNameMatches(L"Aurora", L""));
    CHECK(PresetNameMatches(L"Aurora", nullptr));
    CHECK(PresetNameMatches(L"", L""));
}
TEST(TestPresetFilter_SubstringAnywhereIgnoringCase) {
    CHECK(PresetNameMatches(L"Geiss - Cosmic Dust", L"cosmic"));
    CHECK(PresetNameMatches(L"Geiss - Cosmic Dust", L"GEISS"));
    CHECK(PresetNameMatches(L"Geiss - Cosmic Dust", L"dust"));
    CHECK(PresetNameMatches(L"Geiss - Cosmic Dust", L"s - c"));
    CHECK(!PresetNameMatches(L"Geiss - Cosmic Dust", L"galaxy"));
    CHECK(!PresetNameMatches(L"Dust", L"dusty"));
}
TEST(TestPresetFilter_Wildcards) {
    CHECK(PresetNameMatches(L"Geiss - Cosmic Dust", L"geiss*dust"));
    CHECK(!PresetNameMatches(L"Geiss - Cosmic Dust", L"dust*geiss"));
    CHECK(PresetNameMatches(L"Rovastar - Pulse 2", L"pulse ?"));
    CHECK(!PresetNameMatches(L"Rovastar - Pulse", L"pulse ?"));
    CHECK(PresetNameMatches(L"anything", L"*"));
    CHECK(PresetNameMatches(L"aaab", L"a*ab"));   // needs backtracking
}
TEST(TestPresetFilter_PasteKeepsFirstLine) {
    wchar_t out[64];
    PresetSearchFromPaste(L"catch 333\r\nsecond line", out, 64);
    CHECK(wcscmp(out, L"catch 333") == 0);
    PresetSearchFromPaste(L"\r\n\r\n  royal\n", out, 64);   // leading blank lines are skipped
    CHECK(wcscmp(out, L"royal") == 0);
}
TEST(TestPresetFilter_PasteDropsControlsAndTrims) {
    wchar_t out[64];
    PresetSearchFromPaste(L"\tgeiss\x01 dust  ", out, 64);
    CHECK(wcscmp(out, L"geiss dust") == 0);
    PresetSearchFromPaste(L"x.milk", out, 64);               // the extension is not part of the shown name
    CHECK(wcscmp(out, L"x") == 0);
    PresetSearchFromPaste(nullptr, out, 64);
    CHECK(out[0] == 0);
}
TEST(TestPresetFilter_PasteCutsToLength) {
    wchar_t in[200];
    for (int i = 0; i < 199; i++) in[i] = L'a';
    in[199] = 0;
    wchar_t out[64];
    PresetSearchFromPaste(in, out, 64);
    CHECK(wcslen(out) == 63);
}
// list layout for the highlight tests: 0 "[ .. ]", 1-2 folders, 3-5 presets (nDirs = 3)
TEST(TestPresetFilter_HighlightPrefersPreset) {
    const bool vis[] = { true, true, false, false, true, true };
    CHECK(PresetSearchHighlight(2, vis, 6, 3, 0) == 4);   // hidden -> first matching preset
    CHECK(PresetSearchHighlight(0, vis, 6, 3, 0) == 4);   // on "[ .. ]" while searching -> leaves it
    CHECK(PresetSearchHighlight(5, vis, 6, 3, 0) == 5);   // a shown match stays
}
TEST(TestPresetFilter_HighlightFallsBackToFolder) {
    const bool vis[] = { true, false, true, false, false, false };
    CHECK(PresetSearchHighlight(4, vis, 6, 3, 0) == 2);   // only a folder matches -> that folder
    CHECK(PresetSearchHighlight(0, vis, 6, 3, 0) == 2);
    CHECK(PresetSearchHighlight(2, vis, 6, 3, 0) == 2);
}
TEST(TestPresetFilter_HighlightNothingMatches) {
    const bool vis[] = { true, false, false, false, false, false };
    CHECK(PresetSearchHighlight(4, vis, 6, 3, 0) == 0);   // only "[ .. ]" is left
    const bool none[] = { false, false, false };
    CHECK(PresetSearchHighlight(1, none, 3, 0, -1) == -1); // empty list (no "[ .. ]" at the root)
}
