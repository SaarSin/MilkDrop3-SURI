// ui_scale.h
//
// SURI: infrastructure for the on-screen text: DPI font scaling, paged help splitting and a
// toast (short notification) queue. Pure code except WindowDpi (one dynamic user32 lookup);
// unit-tested in code/tests/test_ui_scale.cpp.

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>

// MulDiv(basePixels, dpi, 96); never smaller than basePixels (dpi < 96 is treated as 96).
int  ScaleFontPixels(int basePixels, UINT dpi);

// DPI of a window. GetDpiForWindow needs Windows 10 1607, so it is looked up in user32 at run
// time (no import, no SDK requirement); returns 96 when it is not available.
UINT WindowDpi(HWND hwnd);

// Pages are separated by a line that contains only a form feed (L"\f"). Empty pages (for example
// after a trailing separator) are dropped; text without a separator is one page; never returns
// an empty vector.
std::vector<std::wstring> SplitHelpPages(const std::wstring& text);

// One help page, parsed for the help card. The first non-blank line is the title; a
// "key<TAB>description" line is a key row; a line starting with a TAB continues the previous
// row's description; blank lines are dropped; any other line is a paragraph (empty key).
struct HelpRow { std::wstring key; std::wstring text; };
std::vector<HelpRow> ParseHelpPage(const std::wstring& page, std::wstring* title);

// isError picks the status dot colour; tag is the caller's category (CPlugin: ERR_*), so a
// group of toasts can be cleared together.
struct Toast { std::wstring text; float startTime; float duration; bool isError; int tag; };

// Newest on top, at most kMaxVisible shown. Fading is up to the caller.
// SURI: thread-safe (the preset-scan thread pushes and clears while the render thread reads).
class ToastQueue {
public:
    static const size_t kMaxVisible = 3;
    void Push(const std::wstring& text, float now, float duration, bool isError = false, int tag = 0);
    // Removes the toasts with this tag; tag 0 removes all of them.
    void Clear(int tag = 0);
    // Newest first; expired toasts (now >= startTime + duration) are skipped.
    // Returns copies, so another thread may Push / Clear while the caller draws them.
    std::vector<Toast> Visible(float now) const;
private:
    mutable std::mutex m_lock;     // guards m_toasts
    std::vector<Toast> m_toasts;   // oldest first
};
