// test_playlist.cpp
//
// SURI: tests for features/playlist.* (playlist model, picking, m3u).

#include "mini_test.h"
#include <string>
#include <vector>
#include "..\vis_milk2\features\playlist.h"

static const std::vector<std::wstring> lib = { L"*sub", L"a.milk", L"B.milk", L"c.milk", L"d.milk" };

TEST(TestPlaylist_DefaultIsAll) {
    Playlist p; CHECK(p.IsAll()); CHECK(p.Contains(L"anything"));
    CHECK(AllowedIndices(lib, 1, p) == std::vector<int>({ 1, 2, 3, 4 }));
}
TEST(TestPlaylist_ResolveByName) {                    // Review Focus 1
    Playlist p; p.SetNames({ L"b.milk", L"D.MILK" });
    unsigned g = p.Generation();
    CHECK(AllowedIndices(lib, 1, p) == std::vector<int>({ 2, 4 }));
    std::vector<std::wstring> grown = { L"*sub", L"0.milk", L"a.milk", L"B.milk", L"c.milk", L"d.milk" };
    CHECK(AllowedIndices(grown, 1, p) == std::vector<int>({ 3, 5 }));
    p.SetAll(); CHECK(p.Generation() != g);
}
TEST(TestPlaylist_PickSkipsMissing) {                 // Review Focus 5
    Playlist p; p.SetNames({ L"gone.milk", L"c.milk" });
    CHECK(AllowedIndices(lib, 1, p) == std::vector<int>({ 3 }));
    p.SetNames({ L"gone.milk" });
    CHECK(AllowedIndices(lib, 1, p) == std::vector<int>({ 1, 2, 3, 4 }));   // nothing left -> all
}
TEST(TestPlaylist_SequentialFromOutside) {            // Review Focus 5
    std::vector<int> a = { 2, 4 };
    CHECK(PickSequential(a, 2, +1) == 4);
    CHECK(PickSequential(a, 4, +1) == 2);   // wrap
    CHECK(PickSequential(a, 3, +1) == 4);   // current not in playlist
    CHECK(PickSequential(a, 3, -1) == 2);
    CHECK(PickSequential(a, 2, -1) == 4);   // wrap back
    CHECK(PickSequential(a, -1, +1) == 2);  // invalid current
}
TEST(TestPlaylist_PickRestricted) {
    std::vector<float> rating = { 0, 1, 0, 3, 0 };
    std::vector<int> a = { 1, 3 };
    CHECK(PickWeighted(a, rating, 0.0f) == 1);
    CHECK(PickWeighted(a, rating, 0.24f) == 1);
    CHECK(PickWeighted(a, rating, 0.26f) == 3);
    std::vector<float> zero = { 0, 0, 0, 0, 0 };
    CHECK(PickWeighted(a, zero, 0.4f) == 1);
    CHECK(PickWeighted(a, zero, 0.6f) == 3);
}
TEST(TestPlaylist_M3uRoundTrip) {
    std::vector<std::wstring> n = { L"Galaxy.milk", L"\u05D2\u05DC\u05E7\u05E1\u05D9.milk" };
    std::string s = SerializeM3u(n);
    CHECK(s.compare(0, 3, "\xEF\xBB\xBF") == 0);
    CHECK(s.find("#EXTM3U\r\n") == 3);
    CHECK(ParseM3u(s) == n);
}
TEST(TestPlaylist_M3uForeignLines) {                  // Review Focus 3
    std::string s = "#EXTM3U\n\nC:\\x\\Galaxy.milk  \n../Neon.milk\r\n#EXTINF:-1,foo\n  \xD7\x90.milk\n";
    CHECK(ParseM3u(s) == std::vector<std::wstring>({ L"Galaxy.milk", L"Neon.milk", L"\u05D0.milk" }));
}
TEST(TestPlaylist_LoadMissing) {
    std::vector<std::wstring> wanted, libNames, out;
    for (int i = 0; i < 34; i++) wanted.push_back(L"p" + std::to_wstring(i) + L".milk");
    for (int i = 3; i < 40; i++) libNames.push_back(L"P" + std::to_wstring(i) + L".milk");
    LoadCount c = MatchToLibrary(wanted, libNames, &out);
    CHECK(c.loaded == 31 && c.total == 34 && out.size() == 31);
    CHECK(out[0] == L"P3.milk");                     // library spelling
    CHECK(LoadedMessage(c) == L"Loaded 31 of 34 presets");
}
