// ui_scale.cpp
//
// SURI: see ui_scale.h.

#include "ui_scale.h"

int ScaleFontPixels(int basePixels, UINT dpi)
{
    if (dpi <= 96)
        return basePixels;
    return MulDiv(basePixels, (int)dpi, 96);
}

UINT WindowDpi(HWND hwnd)
{
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn fn = nullptr;
    static bool looked = false;
    if (!looked) {
        looked = true;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32)
            fn = (GetDpiForWindowFn)GetProcAddress(user32, "GetDpiForWindow");
    }
    if (!fn || !hwnd)
        return 96;
    UINT dpi = fn(hwnd);
    return dpi ? dpi : 96;
}

std::vector<std::wstring> SplitHelpPages(const std::wstring& text)
{
    std::vector<std::wstring> pages;
    std::wstring cur;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find(L'\n', pos);
        bool last = (eol == std::wstring::npos);
        std::wstring line = text.substr(pos, last ? std::wstring::npos : eol - pos);
        if (!line.empty() && line.back() == L'\r')
            line.pop_back();
        if (line == L"\f") {
            pages.push_back(cur);
            cur.clear();
        } else {
            if (!cur.empty() || !line.empty() || !last)
                cur += line + L"\n";
        }
        if (last) break;
        pos = eol + 1;
    }
    pages.push_back(cur);

    // trim trailing blank lines, drop empty pages
    std::vector<std::wstring> out;
    for (std::wstring& p : pages) {
        while (!p.empty() && (p.back() == L'\n' || p.back() == L' '))
            p.pop_back();
        if (!p.empty())
            out.push_back(p);
    }
    if (out.empty())
        out.push_back(text);
    return out;
}

std::vector<HelpRow> ParseHelpPage(const std::wstring& page, std::wstring* title)
{
    std::vector<HelpRow> rows;
    bool haveTitle = false;
    size_t pos = 0;
    while (pos <= page.size()) {
        size_t eol = page.find(L'\n', pos);
        bool last = (eol == std::wstring::npos);
        std::wstring line = page.substr(pos, last ? std::wstring::npos : eol - pos);
        if (!line.empty() && line.back() == L'\r')
            line.pop_back();
        pos = last ? page.size() + 1 : eol + 1;

        size_t firstText = line.find_first_not_of(L" \t");
        if (firstText == std::wstring::npos)
            continue;                       // blank line
        if (!haveTitle) {
            haveTitle = true;
            if (title) *title = line;
            continue;
        }
        size_t tab = line.find(L'\t');
        HelpRow r;
        if (tab == 0) {
            if (!rows.empty()) {            // continuation of the previous description
                rows.back().text += L" " + line.substr(firstText);
                continue;
            }
            r.text = line.substr(firstText);
        } else if (tab != std::wstring::npos) {
            r.key = line.substr(0, tab);
            r.text = line.substr(tab + 1);
        } else {
            r.text = line;                  // paragraph
        }
        rows.push_back(r);
    }
    if (title && !haveTitle) title->clear();
    return rows;
}

void ToastQueue::Push(const std::wstring& text, float now, float duration, bool isError, int tag)
{
    std::lock_guard<std::mutex> lock(m_lock);   // SURI: see ui_scale.h
    // keep the list short: drop what has already expired
    std::vector<Toast> keep;
    for (const Toast& t : m_toasts)
        if (now < t.startTime + t.duration)
            keep.push_back(t);
    m_toasts.swap(keep);
    Toast t;
    t.text = text; t.startTime = now; t.duration = duration; t.isError = isError; t.tag = tag;
    m_toasts.push_back(t);
}

void ToastQueue::Clear(int tag)
{
    std::lock_guard<std::mutex> lock(m_lock);
    std::vector<Toast> keep;
    if (tag != 0)
        for (const Toast& t : m_toasts)
            if (t.tag != tag)
                keep.push_back(t);
    m_toasts.swap(keep);
}

std::vector<Toast> ToastQueue::Visible(float now) const
{
    std::lock_guard<std::mutex> lock(m_lock);
    std::vector<Toast> out;
    for (size_t i = m_toasts.size(); i-- > 0 && out.size() < kMaxVisible; )
        if (now < m_toasts[i].startTime + m_toasts[i].duration)
            out.push_back(m_toasts[i]);   // a copy: valid after the lock is released
    return out;
}
