// test_playlist_screen.cpp
//
// SURI: tests for features/playlist_screen.* (playlist screen + import prompt state).

#include "mini_test.h"
#include <string>
#include <vector>
#include "..\vis_milk2\features\playlist.h"
#include "..\vis_milk2\features\drop_import.h"
#include "..\vis_milk2\features\playlist_screen.h"

static const std::vector<std::wstring> lib = { L"Aurora.milk", L"Cosmic Dust.milk", L"Galaxy.milk", L"Pulse.milk" };

TEST(TestPlaylistScreen_DefaultAllPlaying) {
    PlaylistScreen s; Playlist p; s.Open(lib, p);
    CHECK(s.Count(PlCol::Playing) == 4 && s.Count(PlCol::NotPlaying) == 0 && s.AllPlaying());
}
TEST(TestPlaylistScreen_MoveAndFilter) {
    PlaylistScreen s; Playlist p; p.SetNames({ L"Galaxy.milk" }); s.Open(lib, p);
    CHECK(s.Active() == PlCol::NotPlaying);
    s.Char(L'c'); s.Char(L'o'); s.Char(L's'); s.Char(L'm'); s.Char(L'i'); s.Char(L'c'); s.Char(L' ');   // space is search text
    CHECK(s.Visible(PlCol::NotPlaying) == std::vector<size_t>({ 1 }));
    CHECK(s.Key(UiKey::CtrlA, 10) == PlAction::None);
    CHECK(s.Key(UiKey::Right, 10) == PlAction::Changed);
    CHECK(s.PlayingNames() == std::vector<std::wstring>({ L"Cosmic Dust.milk", L"Galaxy.milk" }));
    CHECK(s.Key(UiKey::Esc, 10) == PlAction::None);                    // clears the filter first
    CHECK(s.Filter(PlCol::NotPlaying).empty());
    CHECK(s.Key(UiKey::Esc, 10) == PlAction::Close);
}
TEST(TestPlaylistScreen_CtrlAOnlyVisible) {
    PlaylistScreen s; Playlist p; p.SetNames({ L"Galaxy.milk" }); s.Open(lib, p);
    s.Char(L'p');                                                      // 'p' is search, not "close"
    s.Key(UiKey::CtrlA, 10); s.Key(UiKey::Right, 10);
    CHECK(s.Count(PlCol::Playing) == 2);                               // Pulse + Galaxy, not Aurora
}
TEST(TestPlaylistScreen_MoveAllBack) {
    PlaylistScreen s; Playlist p; s.Open(lib, p);
    s.Key(UiKey::Tab, 10); CHECK(s.Active() == PlCol::Playing);
    s.Key(UiKey::CtrlA, 10); CHECK(s.Key(UiKey::Left, 10) == PlAction::Changed);
    CHECK(s.Count(PlCol::Playing) == 0 && s.PlayingNames().empty());
}
TEST(TestImportPrompt_PickOneByOne) {
    ImportPrompt q; q.Open({ { L"D:\\a.milk", L"a.milk", L"a.milk", true }, { L"D:\\b.milk", L"b.milk", L"b (2).milk", true } });
    CHECK(q.CheckedCount() == 2);
    q.Key(UiKey::Down, 5); q.Key(UiKey::Space, 5);
    CHECK(q.CheckedCount() == 1 && q.Checked()[0].name == L"a.milk");
    q.Key(UiKey::CtrlA, 5); CHECK(q.CheckedCount() == 2);
    q.Key(UiKey::CtrlA, 5); CHECK(q.CheckedCount() == 0);
    CHECK(q.Key(UiKey::Enter, 5) == PromptResult::Accept);
    CHECK(q.Key(UiKey::Esc, 5) == PromptResult::Skip);
}
