# Phase 10: Dragging presets and a Playlist – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** drag a folder or `.milk` files onto the window, and the program detects what exists and what is new, asks about the new ones (including copying their textures) and restricts playback to the session playlist. In addition, an on-screen Playlist screen with save and load.

**Architecture:** as in phases 3–7: pure logic in three new files in `code/vis_milk2/features/` (`drop_import`, `playlist`, `playlist_screen`), with tests in `unit_tests`. The hookup is in `plugin.cpp` (preset choice, two new UI modes and drawing), in `Milkdrop2PcmVisualizer.cpp` (`WM_DROPFILES`) and in `commands`/`context_menu`. The playlist holds file **names**, not indices, so it survives a rescan of the library. When `IsAll()`, the old selection path runs unchanged.

**Tech Stack:** C++14 in the `plugin` project (`CharacterSet=NotSet`), C++17 + Unicode in `unit_tests`, Win32 (`DragAcceptFiles`, `DragQueryFileW`, `GetOpenFileNameW`/`GetSaveFileNameW`, `CopyFileW`, `FindFirstFileW`), D3D9, `mini_test.h`.

**Spec:** [2026-10-25-phase-10-drag-drop-playlist-design.md](../specs/2026-10-25-phase-10-drag-drop-playlist-design.md). The mockups: `.superpowers/brainstorm/1252-1792882475/content/playlist-layout.html` (B) and `import-prompt.html` (B).

## Global Constraints

- **Approval gates:** execute only after the user approves the plan. Stop for the user's manual test and approval **after Task 6** (end of the phase).
- **Base:** a new branch `suri/phase-10` in a separate worktree (`superpowers:using-git-worktrees`), starting from the commit that closes phase 6 in `suri/phase-5-7` (`feat(ui): friendlier on-screen text`). If that commit doesn't exist yet, **stop and wait**. Don't build on uncommitted files. Before any whole-tree operation (stash/reset/checkout), run `ListAgents` and `git status`.
- **Library:** `m_szPresetDir` (always ends with `\`), top level only. Textures: `m_szMilkdrop2Path` + `textures\`. Playlists: `m_szMilkdrop2Path` + `playlists\`.
- **Never overwrite a file:** every copy is `CopyFileW(src, dst, TRUE)`. If it fails, skip the file and add it to the error count in the toast.
- **No change for anyone who doesn't touch the feature:** when `m_playlist.IsAll()`, the `LoadRandomPreset`/`PrevPreset` code behaves exactly as today.
- The pure `features/*.cpp` code compiles as C++14, as Unicode and as MBCS: no `std::optional`, no structured bindings, and explicit calls to the `W` versions.
- No new dependencies. `shell32.lib` and `comdlg32.lib` are added to `plugin.vcxproj`'s `AdditionalDependencies` if they're not there. Win32 (x86), v143.
- UI strings in English, exactly as in the design document: `Images: load from settings`, `No presets found`, `Playlist: N presets`, `Playlist empty: playing all`, `Loaded X of Y presets`, `K textures kept (name already used)`, `N new presets`, `Not in your library yet. Choose which to add.`, `Enter add K`, `Not playing`, `Playing`, `Tab side · Ctrl+A all · Ctrl+S save · Ctrl+O load · Esc close`.
- New `IDS_*` IDs: the first free number after the highest in `resource.h` at execution time.
- Every new Cmd appears in the menu exactly once (`TestMenu_EveryCommandExactlyOnce`) and gets a label (`TestMenu_LabelsExist`).
- Each Task ends when `.\build.ps1` passes with no errors and no new warnings, and `.\build.ps1 -Test` prints `0 failed`.
- One commit per Task. The commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **The library scan is still running in the background** (`UpdatePresetList(true)` on start, `m_nPresets` growing) when a playlist is dragged or loaded: the playlist is expected to hold names, and the indices to be recomputed on every change of `m_nPresets` or of `Playlist::Generation()`. Tested in `TestPlaylist_ResolveByName` (Task 2). After copying, the wiring runs `UpdatePresetList(false, true, true)` (blocking) before building the playlist (Task 6).
2. **Two new files with the same name in one drag** (from two folders, and also a name already in the library): the target names are expected to differ from each other and from everything in the library (`X.milk`, `X (2).milk`). Tested in `TestDrop_BatchCollision` (Task 1).
3. **An `.m3u` file created by another program:** no BOM, LF or CRLF, lines with a full path (`C:\x\Galaxy.milk` or `../Galaxy.milk`), Hebrew names, and trailing spaces. The name is expected to be taken after the last `\` or `/`, with no spaces at the ends. Tested in `TestPlaylist_M3uForeignLines` (Task 2).
4. **Dragging while another screen is open** (`M`, `L`, typing in a waitstring, the "new files" window or the Playlist screen): the drop is expected to be rejected with a `Close the open screen first` toast and no state change. `CanAcceptDrop(int uiMode)` is pure (Task 1) plus a manual test (Task 6).
5. **A preset in the playlist that was deleted or renamed in `L`, or the current preset removed from the playlist:** expected no crash, the dead name skipped when picking, and the current preset continuing until the next switch. Tested in `TestPlaylist_PickSkipsMissing` and `TestPlaylist_SequentialFromOutside` (Task 2).

---

## File structure

| File | Responsibility | Built in |
|---|---|---|
| `code/vis_milk2/features/drop_import.h/.cpp` | Path classification, existing or new, numbering, texture names, `CanAcceptDrop` | plugin + tests |
| `code/vis_milk2/features/playlist.h/.cpp` | `Playlist`, allowed indices, sequential and weighted picking, M3U | plugin + tests |
| `code/vis_milk2/features/playlist_screen.h/.cpp` | Playlist screen state and "new files" window state (no drawing) | plugin + tests |
| `code/tests/test_drop_import.cpp`, `test_playlist.cpp`, `test_playlist_screen.cpp` | Tests | tests |
| `code/vis_milk2/features/commands.h/.cpp`, `context_menu.cpp`, `help_text.cpp` | `Cmd::PlaylistScreen`, the `P` key, menu, help | plugin + tests |
| `code/vis_milk2/plugin.h/.cpp` | `m_playlist`, preset choice, `UI_PLAYLIST`, `UI_IMPORT_NEW`, drawing, input, `OnDropFiles` | – |
| `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` | `DragAcceptFiles`, `WM_DROPFILES` | – |
| `plugin.rc`, `resource.h`, the vcxproj and filters | Strings, files, link libraries | – |

---

### Task 1: Drop core (pure)

**Files:**
- Create: `code/vis_milk2/features/drop_import.h`, `drop_import.cpp`, `code/tests/test_drop_import.cpp`
- Modify: `code/tests/unit_tests.vcxproj`, `code/vis_milk2/plugin.vcxproj` and `.filters`

**Interfaces:**
- Produces (`drop_import.h`, includes `<string>`, `<vector>`):

```cpp
enum class DropKind { Folder, Preset, Image, Unsupported };
DropKind ClassifyDropPath(const std::wstring& path, bool isDirectory);  // ext case-insensitive; images: png jpg jpeg gif bmp tga dds

struct LibraryEntry { std::wstring name; unsigned long long size; };     // library root, name without path
struct DroppedFile  { std::wstring sourcePath; unsigned long long size; };
struct ImportItem {
    std::wstring sourcePath;
    std::wstring name;        // file name of sourcePath
    std::wstring targetName;  // == name when existing or no clash; numbered otherwise
    bool isNew;
};
// One item per file, in input order. Existing = a library name equal (case-insensitive) and same size.
// New names are unique against the library AND against earlier new items in the same batch.
std::vector<ImportItem> PlanImport(const std::vector<DroppedFile>& files, const std::vector<LibraryEntry>& library);

// "sampler_<x>" roots in a preset's text: 2-letter prefix + '_' (fw_, fc_, pw_, pc_ ...) stripped,
// main / blur1..3 / noise* / noisevol* / rand* dropped, unique case-insensitive, first-seen order.
std::vector<std::wstring> TextureRoots(const std::string& presetText);

bool CanAcceptDrop(bool regularUiMode);   // false while any screen/prompt/waitstring is open
```

- [x] **Step 1: Write failing tests** in `test_drop_import.cpp`:

```cpp
TEST(TestDrop_Classify) {
    CHECK(ClassifyDropPath(L"C:\\p", true) == DropKind::Folder);
    CHECK(ClassifyDropPath(L"C:\\p\\a.milk", false) == DropKind::Preset);
    CHECK(ClassifyDropPath(L"C:\\p\\A.MILK", false) == DropKind::Preset);
    CHECK(ClassifyDropPath(L"C:\\p\\a.PNG", false) == DropKind::Image);
    CHECK(ClassifyDropPath(L"C:\\p\\a.jpeg", false) == DropKind::Image);
    CHECK(ClassifyDropPath(L"C:\\p\\a.txt", false) == DropKind::Unsupported);
    CHECK(ClassifyDropPath(L"C:\\p\\milk", false) == DropKind::Unsupported);
}
TEST(TestDrop_ExistsOrNew) {
    std::vector<LibraryEntry> lib = { { L"Galaxy.milk", 100 }, { L"Pulse.milk", 50 } };
    auto r = PlanImport({ { L"D:\\x\\galaxy.milk", 100 }, { L"D:\\x\\Pulse.milk", 51 }, { L"D:\\x\\Neon.milk", 9 } }, lib);
    CHECK(r.size() == 3);
    CHECK(!r[0].isNew);                                   // same name (case-insensitive) + same size
    CHECK(r[1].isNew && r[1].targetName == L"Pulse (2).milk");
    CHECK(r[2].isNew && r[2].targetName == L"Neon.milk");
}
TEST(TestDrop_NumberingSkipsTaken) {
    std::vector<LibraryEntry> lib = { { L"A.milk", 1 }, { L"A (2).milk", 2 } };
    auto r = PlanImport({ { L"D:\\A.milk", 3 } }, lib);
    CHECK(r[0].targetName == L"A (3).milk");
}
TEST(TestDrop_BatchCollision) {                      // Review Focus 2
    std::vector<LibraryEntry> lib = { { L"X.milk", 1 } };
    auto r = PlanImport({ { L"D:\\a\\X.milk", 2 }, { L"D:\\b\\X.milk", 3 }, { L"D:\\a\\Y.milk", 1 }, { L"D:\\b\\Y.milk", 2 } }, lib);
    CHECK(r[0].targetName == L"X (2).milk");
    CHECK(r[1].targetName == L"X (3).milk");
    CHECK(r[2].targetName == L"Y.milk");
    CHECK(r[3].targetName == L"Y (2).milk");
}
TEST(TestDrop_TextureRefs) {
    std::string t = "comp_1=`sampler sampler_rand00;\ncomp_2=`sampler sampler_fw_clouds2;\n"
                    "comp_3=`ret = tex2D(sampler_main, uv) + tex2D(sampler_blur1, uv) + tex2D(sampler_noise_lq, uv);\n"
                    "warp_1=`sampler sampler_Worms; sampler sampler_worms; tex3D(sampler_noisevol_hq, x);\n";
    auto r = TextureRoots(t);
    CHECK(r.size() == 2);
    CHECK(r[0] == L"clouds2");
    CHECK(r[1] == L"Worms");
}
TEST(TestDrop_CanAccept) { CHECK(CanAcceptDrop(true)); CHECK(!CanAcceptDrop(false)); }
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure (`drop_import.h` doesn't exist).
- [x] **Step 3:** implement `drop_import.cpp`. Case-insensitive name comparison through `CompareStringOrdinal(..., TRUE)`. Numbering is `<stem> (<n>)<ext>` with `n` from 2, where the stem is what precedes the last dot. `TextureRoots` scans the text for `sampler_` followed by identifier characters (`[A-Za-z0-9_]`), and strips an `xx_` prefix only when the third character is `_`, with the same rule as `plugin.cpp` around line 2392.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(drop): classify dropped files against the library`.

### Task 2: The playlist (pure)

**Files:**
- Create: `code/vis_milk2/features/playlist.h`, `playlist.cpp`, `code/tests/test_playlist.cpp`
- Modify: the vcxproj and filters

**Interfaces:**
- Produces (`playlist.h`):

```cpp
class Playlist {
public:
    bool IsAll() const;                                   // default true: whole library
    void SetAll();
    void SetNames(const std::vector<std::wstring>& names); // stored sorted + unique, case-insensitive; IsAll() becomes false
    bool Contains(const std::wstring& name) const;        // IsAll() -> true
    const std::vector<std::wstring>& Names() const;       // empty when IsAll()
    unsigned Generation() const;                          // +1 on every SetAll/SetNames
};

// presetNames = m_presets filenames in list order; the first `dirs` entries are directories ("*name").
// Indices of files the playlist allows. Never empty while there is at least one file:
// an empty match (empty playlist, or every name gone) means "all files".
std::vector<int> AllowedIndices(const std::vector<std::wstring>& presetNames, int dirs, const Playlist& pl);
// Next (direction +1) / previous (-1) allowed index after `current` in list order, wrapping.
// `current` need not be allowed (or valid). allowed must be sorted ascending and non-empty.
int PickSequential(const std::vector<int>& allowed, int current, int direction);
// Rating-weighted pick over `allowed` using ratingThis[i]; r01 in [0,1).
// If the allowed ratings sum to < 0.1 -> uniform: allowed[(size_t)(r01 * allowed.size())].
int PickWeighted(const std::vector<int>& allowed, const std::vector<float>& ratingThis, float r01);

std::string SerializeM3u(const std::vector<std::wstring>& names);  // UTF-8 BOM, "#EXTM3U\r\n", one name per line, CRLF
std::vector<std::wstring> ParseM3u(const std::string& bytes);      // BOM optional, LF/CRLF, skips '#' and blank, trims, keeps text after last '\' or '/'
struct LoadCount { size_t loaded; size_t total; };
// out = wanted names that exist in libraryNames (case-insensitive), with the library's spelling.
LoadCount MatchToLibrary(const std::vector<std::wstring>& wanted, const std::vector<std::wstring>& libraryNames, std::vector<std::wstring>* out);
std::wstring LoadedMessage(LoadCount c);                           // L"Loaded 31 of 34 presets"
```

- [x] **Step 1: Write failing tests** in `test_playlist.cpp`, with `std::vector<std::wstring> lib = { L"*sub", L"a.milk", L"B.milk", L"c.milk", L"d.milk" };` (`dirs = 1`):

```cpp
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
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement `playlist.cpp`. UTF-8/UTF-16 conversion with `MultiByteToWideChar`/`WideCharToMultiByte` using `CP_UTF8`. `AllowedIndices` builds a lookup from the playlist's names (already sorted, so a binary search with `CompareStringOrdinal`), giving O(n log m) over ~10,000 presets. `PickWeighted` computes a cumulative sum over `allowed` only.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(playlist): playlist model, picking and m3u`.

### Task 3: Screen state (pure)

**Files:**
- Create: `code/vis_milk2/features/playlist_screen.h`, `playlist_screen.cpp`, `code/tests/test_playlist_screen.cpp`
- Modify: the vcxproj and filters

**Interfaces:**
- Consumes: `Playlist` (Task 2), `ImportItem` (Task 1).
- Produces (`playlist_screen.h`):

```cpp
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
    PlCol Active() const;
    const std::wstring& Filter(PlCol c) const;
    std::vector<size_t> Visible(PlCol c) const;   // library indices in that column matching its filter, list order
    size_t Count(PlCol c) const;                  // unfiltered count
    size_t Cursor(PlCol c) const;                 // position in Visible(c)
    bool IsSelected(size_t libIndex) const;
    bool AllPlaying() const;                      // Count(NotPlaying) == 0 -> caller uses Playlist::SetAll
    std::vector<std::wstring> PlayingNames() const;
};

enum class PromptResult { None, Accept, Skip };
class ImportPrompt {
public:
    void Open(const std::vector<ImportItem>& newItems);  // all checked, cursor 0
    PromptResult Key(UiKey k, int pageRows);  // arrows/Page/Home/End move; Space toggles; CtrlA: all checked -> none, else all; Enter Accept; Esc Skip
    size_t Size() const; size_t Cursor() const; bool IsChecked(size_t i) const; const ImportItem& Item(size_t i) const;
    size_t CheckedCount() const;
    std::vector<ImportItem> Checked() const;
};
```

`PlaylistScreen` behavior per the design document, section 3: `Right` moves the selected rows from NotPlaying to Playing and `Left` the reverse, each only when the active column is the source. With no selection, the row under the cursor is moved. After a move the selection is cleared and `Changed` is returned. `ShiftUp`/`ShiftDown` move the cursor and add the row the cursor reaches to the selection, plus the row it left if the selection was empty. `CtrlSpace` adds or removes the row under the cursor from the selection. `CtrlA` selects all of `Visible(Active())`. `Tab` switches columns and clears the selection. `Esc` clears the active column's search if it has something, otherwise returns `Close`. `Backspace` deletes a search character. `CtrlS`/`CtrlO` return `Save`/`Load`. `Char(L' ')` and `Char(L'p')` are search.

- [x] **Step 1: Write failing tests** in `test_playlist_screen.cpp`, with `lib = { L"Aurora.milk", L"Cosmic Dust.milk", L"Galaxy.milk", L"Pulse.milk" }`:

```cpp
TEST(TestPlaylistScreen_DefaultAllPlaying) {
    PlaylistScreen s; Playlist p; s.Open(lib, p);
    CHECK(s.Count(PlCol::Playing) == 4 && s.Count(PlCol::NotPlaying) == 0 && s.AllPlaying());
}
TEST(TestPlaylistScreen_MoveAndFilter) {
    PlaylistScreen s; Playlist p; p.SetNames({ L"Galaxy.milk" }); s.Open(lib, p);
    CHECK(s.Active() == PlCol::NotPlaying);
    s.Char(L'c'); s.Char(L'o'); s.Char(L's'); s.Char(L' ');            // space is search text
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
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement. The selection is stored as a `std::vector<bool>` by library index, and search is a case-insensitive "contains" (`CharLowerBuffW` on a copy). `Visible` is recomputed on every call, which is enough for 10,000 rows.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(playlist): playlist screen and import prompt state`.

### Task 4: The playlist in preset choice, and `Cmd::PlaylistScreen`

**Files:**
- Modify: `code/vis_milk2/features/commands.h/.cpp`, `context_menu.cpp`, `help_text.cpp`, `code/vis_milk2/plugin.h/.cpp`, `plugin.rc`, `resource.h`, `code/tests/test_commands.cpp`

**Interfaces:**
- Consumes: Task 2.
- Produces: `Cmd::PlaylistScreen` (after `ShaderLockCycle` in the preset group), `AppSnapshot::playlistOpen`; in `CPlugin`:
  - `Playlist m_playlist;`
  - `void SetPlaylist(const std::vector<std::wstring>& names);` (empty or "all" → `SetAll`; always `ResetPresetHistory()`)
  - `void ResetPresetHistory();`
  - `const std::vector<int>& PlaylistIndices();` (a cache recomputed when `m_nPresets`, `m_nDirs` or `m_playlist.Generation()` change; `EnterCriticalSection(&g_cs)` around reading `m_presets`)

- [x] **Step 1: Failing tests** in `test_commands.cpp`:

```cpp
TEST(TestCommands_PlaylistKey) {
    CHECK(LookupKey(KeyScope::Char, 'p') == Cmd::PlaylistScreen);
    CHECK(LookupKey(KeyScope::Char, 'P') == Cmd::PlaylistScreen);
}
TEST(TestCommands_PlaylistOpenDisablesPresetCommands) {
    AppSnapshot s; s.playlistOpen = true;
    CHECK(!QueryState(Cmd::PresetRandom, s).enabled);
    CHECK(!QueryState(Cmd::PresetNext, s).enabled);
    CHECK(QueryState(Cmd::PlaylistScreen, s).checked);
}
```

  (The existing `TestMenu_EveryCommandExactlyOnce` and `TestMenu_LabelsExist` fail until the item and the label are added.)
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a failure.
- [x] **Step 3:** add the Cmd: keys `p`/`P` (`KeyScope::Char`), an item in the Preset menu after `PresetLoadMenu`, the label `IDS_MENU_PRESET_PLAYLIST` = `Playlist...`, and a line in the help (Presets topic). `QueryState`: `checked = s.playlistOpen`, and preset keys are disabled when `playlistOpen`, with the same condition as `textEditActive`. `GetAppSnapshot` fills `playlistOpen = (m_UI_mode == UI_PLAYLIST)`. The Dispatch calls `OpenPlaylistScreen()` (Task 5). Until Task 5 the function exists only as a stub that switches to `UI_PLAYLIST` and immediately back to `UI_REGULAR`.
- [x] **Step 4:** hook into the choice in `plugin.cpp`:
  - `LoadRandomPreset`: after the history path, and only when `!m_playlist.IsAll()`: in sequential mode `m_nCurrentPreset = PickSequential(PlaylistIndices(), m_nCurrentPreset, +1)`, and in random mode `PickWeighted(PlaylistIndices(), ratingThis, (rand() % 14345) / 14345.0f)`, where `ratingThis` comes from `m_presets[i].fRatingThis`, and 1 for all files if `!m_bEnableRating`. When `IsAll()` the existing code runs unchanged.
  - `PrevPreset` in sequential mode, when `!IsAll()`: `PickSequential(..., -1)`. The history path does not change.
  - `ResetPresetHistory()`: `m_presetHistoryBackFence = m_presetHistoryFwdFence = m_presetHistoryPos`.
  - When the library folder changes (the assignments to `m_szPresetDir` around lines 5020 and 5376): `m_playlist.SetAll()`.
- [x] **Step 5:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 6:** commit `feat(playlist): restrict preset picking to the session playlist`.

### Task 5: The Playlist screen, save and load

**Files:**
- Modify: `code/vis_milk2/plugin.h/.cpp` (`ui_mode` + `UI_PLAYLIST`, drawing and input), `plugin.vcxproj` (`comdlg32.lib`)

**Interfaces:**
- Consumes: `PlaylistScreen`, `UiKey` (Task 3), `SetPlaylist` (Task 4), `SerializeM3u`/`ParseM3u`/`MatchToLibrary`/`LoadedMessage` (Task 2), `DrawRoundedPanel`, `UiPx`, `CARD_*`, `m_toasts` (phase 6).
- Produces: `PlaylistScreen m_plScreen;`, `void OpenPlaylistScreen();`, `void SavePlaylistDialog();`, `void LoadPlaylistDialog();`, `UiKey` from `WM_KEYDOWN`: `bool TranslateUiKey(WPARAM vk, UiKey* out)` (calls `GetKeyState` for Ctrl and Shift; in `plugin.cpp`, because Task 6 uses it too).

- [x] **Step 1:** `OpenPlaylistScreen()`: `UpdatePresetList()` (which makes sure the list is ready), then `m_plScreen.Open(file names of m_presets[m_nDirs..m_nPresets), m_playlist)` → `UI_PLAYLIST`. Input: when `m_UI_mode == UI_PLAYLIST`, `WM_KEYDOWN` in `MyWindowProc` (before "case 3") goes through `TranslateUiKey` to `m_plScreen.Key`, and `WM_CHAR` with a character `>= 0x20` (and without Ctrl) goes to `m_plScreen.Char`. `PlAction::Changed` → `SetPlaylist(AllPlaying() ? {} : PlayingNames())`, where an empty list means "all". `Close` → `UI_REGULAR`, and if `Count(Playing) == 0` a `Playlist empty: playing all` toast appears. `pageRows` is the number of rows actually drawn.
- [x] **Step 2:** drawing per mockup (B) and the phase 6 framework: a `CARD_FILL_PANEL` card, radius `UiPx(14)`, padding `UiPx(20)`, width min(980, 90% of the screen) and height 80% of the screen. Title `Playlist`. Two columns, each headed by its name (`Not playing` / `Playing`), the `Count`, and a search line in `CARD_TEXT_2` (when empty: `Type to search`). A selected row is drawn with `CARD_BAND`, and the cursor in the active column with a `CARD_ACCENT` border. Draw only the visible rows. Bottom line: `Tab side · Ctrl+A all · Ctrl+S save · Ctrl+O load · Esc close`.
- [x] **Step 3:** save and load. `GetSaveFileNameW`/`GetOpenFileNameW` with the filter `Playlists (*.m3u)`, starting folder `playlists\` (created with `CreateDirectoryW`), and `OFN_OVERWRITEPROMPT` when saving. If `fullscreen`, leave fullscreen through `Cmd::ViewFullscreen` before the dialog and return after it. Saving writes `SerializeM3u(m_plScreen.PlayingNames())`. Loading: `ParseM3u` → `MatchToLibrary` against the file names in `m_presets` → a `LoadedMessage` toast. If `loaded > 0`: `SetPlaylist`, `m_plScreen.Open(...)` again, and if the current preset is not in the playlist, load `PlaylistIndices()[0]` with `m_fBlendTimeUser`.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`. Self-check: `P` opens, moving both ways, search with a space, Save then Load (`Loaded N of N`), and opening the screen with all 9,795 presets with no visible slowdown.
- [x] **Step 5:** commit `feat(playlist): on-screen playlist with save and load`.

### Task 6: The drop itself

**Files:**
- Modify: `code/vis_milk2/Milkdrop2PcmVisualizer.cpp`, `code/vis_milk2/plugin.h/.cpp` (`UI_IMPORT_NEW`, `OnDropFiles`, drawing the window), `plugin.vcxproj` (`shell32.lib`)

**Interfaces:**
- Consumes: Tasks 1–5.
- Produces: `void CPlugin::OnDropFiles(HDROP h);`, `ImportPrompt m_importPrompt;`, `bool m_importSingle;` (a single-file drop).

- [x] **Step 1:** `DragAcceptFiles(hwnd, TRUE)` right after `CreateWindowW` in `CreateWindowAndRun`. In `StaticWndProc`: `case WM_DROPFILES: g_plugin.OnDropFiles((HDROP)wParam); return 0;` (`DragFinish` is called inside `OnDropFiles`).
- [x] **Step 2:** `OnDropFiles`:
  1. `!CanAcceptDrop(m_UI_mode == UI_REGULAR && !m_waitstring.bActive)` → a `Close the open screen first` toast, and return.
  2. Collect the paths: folder → `FindFirstFileW(dir\*.milk)` at the top level; `.milk` → as is; image → count it. If no `.milk` was found: an `Images: load from settings` toast if there was an image, otherwise `No presets found`.
  3. `m_importSingle = (one item was dragged, and it is a .milk file)`.
  4. Build `library` from `FindFirstFileW(m_szPresetDir*.milk)` with sizes, and run `PlanImport`.
  5. If there are new items: `m_importPrompt.Open(new items)` → `UI_IMPORT_NEW`, and remember the existing items. Otherwise go straight to step 7 without copying.
- [x] **Step 3:** `UI_IMPORT_NEW`: input through `TranslateUiKey` → `m_importPrompt.Key`. Drawing per mockup (B): the title `N new presets`, the subtitle `Not in your library yet. Choose which to add.`, rows with ☑/☐ (`CARD_DOT_NOTIFY` for checked and `CARD_TEXT_2` for unchecked), next to a changed name `→ <targetName>` in `CARD_STAR`, and the bottom line `Space toggle · Ctrl+A all · Enter add K · Esc skip`.
- [x] **Step 4:** at the end (`Accept` or `Skip`) return to `UI_REGULAR`, and after `Accept`, for each checked item:
  1. `CopyFileW(source, m_szPresetDir + targetName, TRUE)`.
  2. Textures: read the file, and for each root from `TextureRoots` look for `<dir>\<root>.<ext>` and then `<dir>\textures\<root>.<ext>` in `texture_exts` order. The target is `textures\<root>.<ext>`: if it doesn't exist, copy; if it exists with the same size, skip; if it exists with a different size, add one to the "kept" count. A texture not found on the dragged side is not counted.
  3. If anything was copied: `UpdatePresetList(false, true, true)`.
  4. If kept > 0: a `K textures kept (name already used)` toast.
- [x] **Step 5:** applying:
  - **`m_importSingle`:** if the file exists or was copied, `LoadPreset(m_szPresetDir + target name, m_fBlendTimeUser)`, and the playlist does not change. If it is new and `Esc` was pressed, nothing happens.
  - **Group:** `names` = the target names of the existing ones and of those copied. If the list is empty, nothing happens. Otherwise: `SetPlaylist(names)`, load `PlaylistIndices()[0]` with `m_fBlendTimeUser`, and a `Playlist: N presets` toast.
- [x] **Step 6:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 7:** commit `feat(drop): drag folders and presets onto the window`.
- [x] **Step 8: Manual test (user). Stop for phase 10 approval.** Prepare a test folder with 3 existing presets, 2 new ones, one preset with an existing name and a different size (expect `(2)`), and a preset with a `sampler_` pointing to a texture next to it, and hand it to the user:
  1. Drag the folder: the window, uncheck one, Enter, the toast, and Space/H staying inside the playlist.
  2. Drag a single new file and press `Esc`: nothing happens.
  3. Drag an image: the toast appears.
  4. Drag while `L` is open: the drop is rejected.
  5. `P`: move, search, Save, delete one of the files from the library, Load (`Loaded X of Y`).
  6. Drag in fullscreen and at 150% DPI.
  7. Close and reopen: everything plays.
