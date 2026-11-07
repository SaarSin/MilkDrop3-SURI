// test_preset_filter.cpp
//
// SURI: tests for features/preset_filter.* (type-to-filter in the preset list).

#include "mini_test.h"
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
