// test_now_playing.cpp
//
// SURI: tests for features/now_playing.* (song info formatting, truncation, change detection).

#include "mini_test.h"
#include "..\vis_milk2\features\now_playing.h"

TEST(TestSongInfo_FormatEmpty) {
    CHECK(FormatSongInfo(SongInfo{}) == L"");
    CHECK(FormatSongInfo(SongInfo{ L"", L"Artist" }) == L"");      // no title, nothing to show
}
TEST(TestSongInfo_FormatTitleOnly) { CHECK(FormatSongInfo(SongInfo{ L"Song", L"" }) == L"Song"); }
TEST(TestSongInfo_FormatBoth) { CHECK(FormatSongInfo(SongInfo{ L"Song", L"Band" }) == L"Band \u2013 Song"); }
TEST(TestSongInfo_HebrewTitle) {
    SongInfo s{ L"\u05E9\u05D9\u05E8 \u05D0\u05D4\u05D1\u05D4", L"\u05D0\u05DE\u05DF" };
    CHECK(FormatSongInfo(s) == L"\u05D0\u05DE\u05DF \u2013 \u05E9\u05D9\u05E8 \u05D0\u05D4\u05D1\u05D4");
}
TEST(TestSongInfo_TruncateKeepsSurrogatePair) {
    std::wstring s = L"ab\U0001F3B5cd";               // a, b, high, low, c, d
    CHECK(TruncateUtf16(s, 3) == L"ab");              // would cut between high and low
    CHECK(TruncateUtf16(s, 4) == L"ab\U0001F3B5");
    CHECK(TruncateUtf16(s, 100) == s);
    CHECK(TruncateUtf16(s, 0) == L"");
}
TEST(TestSongInfo_ChangeDetection) {
    CHECK(SongChanged(L"", L"A"));
    CHECK(!SongChanged(L"A", L"A"));
    CHECK(SongChanged(L"A", L"B"));
    CHECK(!SongChanged(L"A", L""));                   // player closed: no empty announcement
}
TEST(TestSongInfo_BoxRoundTrip) {
    SongInfoBox box;
    CHECK(box.State() == NowPlayingState::Unknown);
    CHECK(box.Text() == L"");
    box.Set(SongInfo{ L"Song", L"Band" });
    box.SetState(NowPlayingState::Available);
    CHECK(box.Text() == L"Band \u2013 Song");
    CHECK(box.State() == NowPlayingState::Available);
}
TEST(TestSongInfo_TruncateAtTitleAnimLimit) {   // td_supertext::szTextW is 256 units
    std::wstring s = std::wstring(254, L'a') + L"\U0001F3B5" + std::wstring(44, L'b');
    CHECK(s.size() == 300);
    std::wstring t = TruncateUtf16(s, 255);       // 255 would end on the high surrogate
    CHECK(t.size() == 254);
    CHECK(t[t.size() - 1] == L'a');
    CHECK(TruncateUtf16(s, 256).size() == 256);
}
