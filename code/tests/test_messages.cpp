// test_messages.cpp
//
// SURI: tests for features/messages.* (random message pick, sample ini templates, UTF-16 writer).

#include "mini_test.h"
#include <windows.h>
#include "..\vis_milk2\features\messages.h"

TEST(TestMessages_PickRandomNone) {
    bool none[4] = { false, false, false, false };
    CHECK(PickRandomMessage(none, 4, 12345) == -1);
    CHECK(PickRandomMessage(none, 0, 0) == -1);
}
TEST(TestMessages_PickRandomSkipsEmpty) {
    bool some[5] = { false, true, false, true, false };
    CHECK(PickRandomMessage(some, 5, 0) == 1);
    CHECK(PickRandomMessage(some, 5, 1) == 3);
    CHECK(PickRandomMessage(some, 5, 2) == 1);
}
TEST(TestMessages_TemplatesHaveSections) {
    CHECK(MessagesIniTemplate().find(L"[message00]") != std::wstring::npos);
    CHECK(MessagesIniTemplate().find(L"[font00]") != std::wstring::npos);
    CHECK(SpritesIniTemplate().find(L"[img00]") != std::wstring::npos);
    CHECK(SpritesIniTemplate().find(L"textures\\") != std::wstring::npos);
}
TEST(TestMessages_WriteIfMissing) {
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(path, L"%smd3_test_messages_%lu.ini", dir, GetCurrentProcessId());
    DeleteFileW(path);
    std::wstring text = L"[a]\r\nx=\u05E9\r\n";
    CHECK(WriteUtf16IniIfMissing(path, text));
    CHECK(!WriteUtf16IniIfMissing(path, text));      // exists now: not overwritten

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    unsigned char buf[256] = { 0 };
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf), &got, NULL);
    CloseHandle(h);
    CHECK(got == 2 + text.size() * 2);
    CHECK(buf[0] == 0xFF && buf[1] == 0xFE);
    CHECK(memcmp(buf + 2, text.data(), text.size() * 2) == 0);
    DeleteFileW(path);
}
