// playlist_screen.cpp
//
// SURI: see playlist_screen.h.

#include "playlist_screen.h"
#include <windows.h>

static std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    if (!r.empty()) CharLowerBuffW(&r[0], (DWORD)r.size());
    return r;
}

// Shared cursor movement; returns the new cursor in [0, n-1] (0 when n == 0).
static size_t MoveCursor(UiKey k, size_t cur, size_t n, int pageRows) {
    if (n == 0) return 0;
    size_t page = pageRows > 1 ? (size_t)pageRows : 1;
    switch (k) {
    case UiKey::Up: case UiKey::ShiftUp:     return cur > 0 ? cur - 1 : 0;
    case UiKey::Down: case UiKey::ShiftDown: return cur + 1 < n ? cur + 1 : n - 1;
    case UiKey::PageUp:   return cur > page ? cur - page : 0;
    case UiKey::PageDown: return cur + page < n ? cur + page : n - 1;
    case UiKey::Home:     return 0;
    case UiKey::End:      return n - 1;
    default:              return cur;
    }
}

void PlaylistScreen::Open(const std::vector<std::wstring>& library, const Playlist& pl) {
    m_lib = library;
    m_libLower.resize(library.size());
    for (size_t i = 0; i < library.size(); i++) m_libLower[i] = Lower(library[i]);
    m_playing.assign(library.size(), false);
    for (size_t i = 0; i < library.size(); i++) m_playing[i] = pl.Contains(library[i]);
    m_sel.assign(library.size(), false);
    m_active = PlCol::NotPlaying;
    m_filter[0].clear(); m_filter[1].clear();
    m_cursor[0] = m_cursor[1] = 0;
}

std::vector<size_t> PlaylistScreen::Visible(PlCol c) const {
    std::vector<size_t> r;
    const bool want = (c == PlCol::Playing);
    const std::wstring f = Lower(m_filter[(int)c]);
    for (size_t i = 0; i < m_lib.size(); i++) {
        if (m_playing[i] != want) continue;
        if (!f.empty() && m_libLower[i].find(f) == std::wstring::npos) continue;
        r.push_back(i);
    }
    return r;
}

size_t PlaylistScreen::Count(PlCol c) const {
    size_t n = 0;
    const bool want = (c == PlCol::Playing);
    for (size_t i = 0; i < m_playing.size(); i++) if (m_playing[i] == want) n++;
    return n;
}

std::vector<std::wstring> PlaylistScreen::PlayingNames() const {
    std::vector<std::wstring> r;
    for (size_t i = 0; i < m_lib.size(); i++) if (m_playing[i]) r.push_back(m_lib[i]);
    return r;
}

void PlaylistScreen::Clamp(PlCol c) {
    size_t n = Visible(c).size();
    size_t& cur = m_cursor[(int)c];
    if (n == 0) cur = 0; else if (cur >= n) cur = n - 1;
}

void PlaylistScreen::ClearSelection() { m_sel.assign(m_lib.size(), false); }

bool PlaylistScreen::AnySelected() const {
    for (size_t i = 0; i < m_sel.size(); i++) if (m_sel[i]) return true;
    return false;
}

void PlaylistScreen::SelectCursorRow() {
    std::vector<size_t> v = Visible(m_active);
    size_t cur = m_cursor[(int)m_active];
    if (cur < v.size()) m_sel[v[cur]] = true;
}

// Only acts when the active column is the source of the move.
PlAction PlaylistScreen::MoveSelected(bool toPlaying) {
    if ((m_active == PlCol::Playing) == toPlaying) return PlAction::None;
    bool moved = false;
    if (AnySelected()) {
        for (size_t i = 0; i < m_sel.size(); i++)
            if (m_sel[i] && m_playing[i] != toPlaying) { m_playing[i] = toPlaying; moved = true; }
    } else {
        std::vector<size_t> v = Visible(m_active);
        size_t cur = m_cursor[(int)m_active];
        if (cur < v.size()) { m_playing[v[cur]] = toPlaying; moved = true; }
    }
    ClearSelection();
    Clamp(PlCol::NotPlaying); Clamp(PlCol::Playing);
    return moved ? PlAction::Changed : PlAction::None;
}

PlAction PlaylistScreen::Key(UiKey k, int pageRows) {
    const int a = (int)m_active;
    switch (k) {
    case UiKey::Up: case UiKey::Down: case UiKey::PageUp: case UiKey::PageDown:
    case UiKey::Home: case UiKey::End:
        m_cursor[a] = MoveCursor(k, m_cursor[a], Visible(m_active).size(), pageRows);
        return PlAction::None;
    case UiKey::ShiftUp: case UiKey::ShiftDown:
        if (!AnySelected()) SelectCursorRow();
        m_cursor[a] = MoveCursor(k, m_cursor[a], Visible(m_active).size(), pageRows);
        SelectCursorRow();
        return PlAction::None;
    case UiKey::CtrlSpace: {
        std::vector<size_t> v = Visible(m_active);
        if (m_cursor[a] < v.size()) m_sel[v[m_cursor[a]]] = !m_sel[v[m_cursor[a]]];
        return PlAction::None;
    }
    case UiKey::CtrlA: {
        std::vector<size_t> v = Visible(m_active);
        for (size_t i = 0; i < v.size(); i++) m_sel[v[i]] = true;
        return PlAction::None;
    }
    case UiKey::Tab:
        m_active = (m_active == PlCol::Playing) ? PlCol::NotPlaying : PlCol::Playing;
        ClearSelection();
        return PlAction::None;
    case UiKey::Right: return MoveSelected(true);
    case UiKey::Left:  return MoveSelected(false);
    case UiKey::Backspace:
        if (!m_filter[a].empty()) { m_filter[a].erase(m_filter[a].size() - 1); ClearSelection(); Clamp(m_active); }
        return PlAction::None;
    case UiKey::Esc:
        if (!m_filter[a].empty()) { m_filter[a].clear(); ClearSelection(); Clamp(m_active); return PlAction::None; }
        return PlAction::Close;
    case UiKey::CtrlS: return PlAction::Save;
    case UiKey::CtrlO: return PlAction::Load;
    default: return PlAction::None;   // Space / Enter: a plain space arrives through Char()
    }
}

PlAction PlaylistScreen::Char(wchar_t c) {
    if (c < 0x20) return PlAction::None;
    m_filter[(int)m_active] += c;
    ClearSelection();
    m_cursor[(int)m_active] = 0;
    return PlAction::None;
}

void ImportPrompt::Open(const std::vector<ImportItem>& newItems) {
    m_items = newItems;
    m_checked.assign(newItems.size(), true);
    m_cursor = 0;
}

PromptResult ImportPrompt::Key(UiKey k, int pageRows) {
    switch (k) {
    case UiKey::Up: case UiKey::Down: case UiKey::PageUp: case UiKey::PageDown:
    case UiKey::Home: case UiKey::End:
        m_cursor = MoveCursor(k, m_cursor, m_items.size(), pageRows);
        return PromptResult::None;
    case UiKey::Space:
        if (m_cursor < m_checked.size()) m_checked[m_cursor] = !m_checked[m_cursor];
        return PromptResult::None;
    case UiKey::CtrlA: {
        bool all = !m_checked.empty() && CheckedCount() == m_checked.size();
        m_checked.assign(m_items.size(), !all);
        return PromptResult::None;
    }
    case UiKey::Enter: return PromptResult::Accept;
    case UiKey::Esc:   return PromptResult::Skip;
    default: return PromptResult::None;
    }
}

size_t ImportPrompt::CheckedCount() const {
    size_t n = 0;
    for (size_t i = 0; i < m_checked.size(); i++) if (m_checked[i]) n++;
    return n;
}

std::vector<ImportItem> ImportPrompt::Checked() const {
    std::vector<ImportItem> r;
    for (size_t i = 0; i < m_items.size(); i++) if (m_checked[i]) r.push_back(m_items[i]);
    return r;
}
