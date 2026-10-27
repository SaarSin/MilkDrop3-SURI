// messages.cpp
//
// SURI: see messages.h. C++14, no dependencies beyond the Win32 file API.

#include "messages.h"
#include <windows.h>

int PickRandomMessage(const bool* hasText, int count, int randomValue)
{
    if (!hasText || count <= 0)
        return -1;
    int n = 0;
    for (int i = 0; i < count; i++)
        if (hasText[i])
            n++;
    if (n == 0)
        return -1;
    int want = (randomValue < 0 ? -(randomValue + 1) : randomValue) % n;   // safe for INT_MIN
    for (int i = 0; i < count; i++)
        if (hasText[i] && want-- == 0)
            return i;
    return -1;
}

std::wstring MessagesIniTemplate()
{
    return
        L"; milk2_msg.ini - custom messages.\r\n"
        L"; IMPORTANT: save this file as UTF-16 (Unicode) so that non-English text such as\r\n"
        L"; the Hebrew message below is shown correctly. An ANSI or UTF-8 file will garble it.\r\n"
        L"; Press Y and then a two digit number (00-99) to show a message.\r\n"
        L"\r\n"
        L"[font00]\r\n"
        L"face=Segoe UI\r\n"
        L"bold=1\r\n"
        L"ital=0\r\n"
        L"r=255\r\n"
        L"g=255\r\n"
        L"b=255\r\n"
        L"\r\n"
        L"[message00]\r\n"
        L"font=0\r\n"
        L"size=50\r\n"
        L"x=0.5\r\n"
        L"y=0.5\r\n"
        L"time=4\r\n"
        L"fade=1\r\n"
        L"text=Hello from MilkDrop\r\n"
        L"\r\n"
        L"[message01]\r\n"
        L"font=0\r\n"
        L"size=50\r\n"
        L"x=0.5\r\n"
        L"y=0.7\r\n"
        L"time=4\r\n"
        L"fade=1\r\n"
        L"text=\u05E9\u05DC\u05D5\u05DD \u05DE\u05DE\u05D9\u05DC\u05E7\u05D3\u05E8\u05D5\u05E4\r\n";
}

std::wstring SpritesIniTemplate()
{
    return
        L"; milk2_img.ini - sprites.\r\n"
        L"; Press K and then a two digit number (00-99) to show a sprite; Shift+K and a number removes it.\r\n"
        L"; img is relative to the plugins\\Milkdrop2 folder. code_N lines run every frame.\r\n"
        L"\r\n"
        L"[img00]\r\n"
        L"; a static sprite\r\n"
        L"img=textures\\OIchess1.jpg\r\n"
        L"code_1=x = 0.25;\r\n"
        L"code_2=y = 0.5;\r\n"
        L"code_3=sx = 0.3;\r\n"
        L"code_4=sy = 0.3;\r\n"
        L"\r\n"
        L"[img01]\r\n"
        L"; a sprite that moves with the bass\r\n"
        L"img=textures\\OIbeans1.jpg\r\n"
        L"code_1=x = 0.7 + 0.1*bass;\r\n"
        L"code_2=y = 0.5;\r\n"
        L"code_3=sx = 0.3;\r\n"
        L"code_4=sy = 0.3;\r\n";
}

bool WriteUtf16IniIfMissing(const wchar_t* path, const std::wstring& text)
{
    if (!path || !*path)
        return false;
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    const unsigned char bom[2] = { 0xFF, 0xFE };
    DWORD wrote = 0;
    bool ok = WriteFile(h, bom, 2, &wrote, NULL) && wrote == 2;
    if (ok && !text.empty()) {
        DWORD bytes = (DWORD)(text.size() * sizeof(wchar_t));
        ok = WriteFile(h, text.data(), bytes, &wrote, NULL) && wrote == bytes;
    }
    CloseHandle(h);
    if (!ok)
        DeleteFileW(path);
    return ok;
}
