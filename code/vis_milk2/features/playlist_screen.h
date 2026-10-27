// playlist_screen.h
//
// SURI: pure state of the on-screen Playlist screen (two columns, search, move between them)
// and of the "new presets" import prompt. No drawing here.

#pragma once

#include <string>
#include <vector>
#include "playlist.h"
#include "drop_import.h"

enum class UiKey { Up, Down, PageUp, PageDown, Home, End, Tab, ShiftUp, ShiftDown,
                   Space, CtrlSpace, CtrlA, Left, Right, Enter, Backspace, Esc, CtrlS, CtrlO };
enum class PlCol { NotPlaying = 0, Playing = 1 };
enum class PlAction { None, Changed, Close, Save, Load };

class PlaylistScreen {
public:
    // library = file names (no dirs) in list order. Playing = pl.IsAll() ? all : names pl contains.
    void Open(const std::vector<std::wstring>& library, const Playlist& pl);
    PlAction Key(UiKey k, int pageRows);
    PlAction Char(wchar_t c);                     // printable (>= 0x20): appends to the active column's filter
    PlCol Active() const { return m_active; }
    const std::wstring& Filter(PlCol c) const { return m_filter[(int)c]; }
    std::vector<size_t> Visible(PlCol c) const;   // library indices in that column matching its filter, list order
    size_t Count(PlCol c) const;                  // unfiltered count
    size_t Cursor(PlCol c) const { return m_cursor[(int)c]; }   // position in Visible(c)
    bool IsSelected(size_t libIndex) const { return libIndex < m_sel.size() && m_sel[libIndex]; }
    bool AllPlaying() const { return Count(PlCol::NotPlaying) == 0; }   // caller uses Playlist::SetAll
    std::vector<std::wstring> PlayingNames() const;
private:
    void Clamp(PlCol c);
    void ClearSelection();
    bool AnySelected() const;
    void SelectCursorRow();
    PlAction MoveSelected(bool toPlaying);
    std::vector<std::wstring> m_lib;
    std::vector<std::wstring> m_libLower;   // m_lib lowercased once in Open(), for the filters
    std::vector<bool> m_playing;
    std::vector<bool> m_sel;
    PlCol m_active = PlCol::NotPlaying;
    std::wstring m_filter[2];
    size_t m_cursor[2] = { 0, 0 };
};

enum class PromptResult { None, Accept, Skip };
class ImportPrompt {
public:
    void Open(const std::vector<ImportItem>& newItems);  // all checked, cursor 0
    PromptResult Key(UiKey k, int pageRows);  // arrows/Page/Home/End move; Space toggles; CtrlA: all checked -> none, else all; Enter Accept; Esc Skip
    size_t Size() const { return m_items.size(); }
    size_t Cursor() const { return m_cursor; }
    bool IsChecked(size_t i) const { return i < m_checked.size() && m_checked[i]; }
    const ImportItem& Item(size_t i) const { return m_items[i]; }
    size_t CheckedCount() const;
    std::vector<ImportItem> Checked() const;
private:
    std::vector<ImportItem> m_items;
    std::vector<bool> m_checked;
    size_t m_cursor = 0;
};
