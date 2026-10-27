# Phases 3–4: Command layer and right-click menu – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** one command table that the keyboard and the right-click menu (and later the tray icon, phase 7) go through, and a Win32 menu built from that table.

**Architecture:** the pure part, with no D3D and no window, lives in `code/vis_milk2/features/`: the command enum, the key table, the menu ID mapping, the state of each item (checked and enabled) and the menu layout. It is tested in `unit_tests`. Execution itself is `CPlugin::DispatchCommand`, which lives in `plugin.cpp` because it needs that file's statics. Window-level commands (fullscreen, Stretch, exit, help and About) are executed in `HandleWindowCommand` in `Milkdrop2PcmVisualizer.cpp`. The old switch stays for every key that did not move to the table, and for state-dependent behavior (UI_LOAD, UI_MASHUP). When `DispatchCommand` returns `false`, the code continues down the old path.

**Tech Stack:** C++ (the `plugin` project compiles with the v143 default standard, i.e. C++14, and `CharacterSet=NotSet`; the `unit_tests` project compiles with C++17 and Unicode), Win32 (`TrackPopupMenu`, `AppendMenuW`, `ShellExecuteW`), `mini_test.h`.

**Spec:** [2026-10-24-milkdrop-suri-roadmap.md](2026-10-24-milkdrop-suri-roadmap.md), phases 3 and 4. Description of the current state: [PROJECT_STATUS.md](../../PROJECT_STATUS.md).

## Global Constraints

- **Approval gate:** execution starts only after the user approves this plan. After Task 4 (end of phase 3), stop for a manual test and the user's approval before Task 5.
- **Base:** phases 0–2 are already committed (tests and audio). Phases 3–4 are built on a `suri/phase-3-4` branch that starts there.
- **Don't touch the other sessions' files:** `code/audio/*` (phase 2), `code/resources/Milkdrop2/docs/*` and `defines.h`. (The earlier cleanup removed `DOCFILE` and `LONGNAMEW`, so the guide path is built from `m_szMilkdrop2Path`, which equals `<exe>\plugins\Milkdrop2\`.)
- The shared code (the pure `features/*.cpp`) must compile as C++14, as Unicode and as MBCS: no `std::optional`, structured bindings or `inline` variables, and explicit calls to the `W` versions of the API.
- No new dependencies. Win32 (x86) and v143.
- Each Task ends when `.\build.ps1` passes with no errors and no new warnings (there are 2 existing C4477 warnings), and `.\build.ps1 -Test` prints `0 failed`.
- Each Task has its own commit. The commit message ends with the line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A key or menu item while editing text** (Save As or Rename open, or a Y/N question on screen): a key is expected not to trigger a command, because routing to the table happens only after the waitstring handling. In the menu, all preset commands are disabled. Tested in `TestCommands_TextEditDisablesPresetCommands` (Task 2).
2. **A silent change in an existing key's behavior** (Space inside the preset browser, h/H in mashup, F-keys while editing): each key is expected to do exactly what it did before, in every UI state. Tested in `TestCommands_LegacyKeysPreserved` (Task 1), and in the manual test following the table in Task 3.
3. **Menu in fullscreen:** fullscreen here is already fake fullscreen (a TOPMOST popup window, and `d3dPp.Windowed=TRUE` always), so there is no D3D exclusive problem. The real problems are that the cursor is hidden (`ShowCursor(FALSE)`) and that the visualization freezes, because rendering happens only when the message queue is empty, and `TrackPopupMenu` is modal. The cursor is expected to be shown while the menu is open and hidden again after it closes (even if the command left fullscreen), and the image to keep moving behind the menu. Tested manually in Task 6.
4. **Opening the menu from the keyboard** (Shift+F10 or the Apps key, then `lParam == -1`) and a secondary monitor with negative coordinates: the menu is expected to open in the center of the window in the first case, and at the click point in the second. Tested in `TestMenu_Anchor` (Task 5).
5. **`WM_COMMAND` falls through to `WM_CHAR`:** `CPlugin::MyWindowProc` (`plugin.cpp:4467`) has `case WM_COMMAND:` without a `break`, so a menu ID arriving there would be interpreted as a character. This is expected never to happen: the menu uses `TPM_RETURNCMD`, and `StaticWndProc` swallows every `WM_COMMAND` in our range. Tested in `TestCommands_MenuIdRoundTrip` (Task 1), and manually in Task 6.

---

## File structure

| File | Responsibility | Built in |
|---|---|---|
| `code/vis_milk2/features/commands.h/.cpp` | `Cmd`, the key table, `LookupKey`, menu ID mapping, key labels, `AppSnapshot`/`QueryState`, F2/F3 cycles | plugin + tests |
| `code/vis_milk2/features/context_menu.h/.cpp` | Menu layout (`MenuLayout`), `CmdLabelId`, `MenuAnchor` | plugin + tests |
| `code/vis_milk2/features/context_menu_win32.cpp` | `BuildContextMenu` (HMENU from the layout and the snapshot) | plugin only |
| `code/tests/test_commands.cpp`, `code/tests/test_context_menu.cpp` | Tests | tests |
| `code/vis_milk2/plugin.cpp` | `DispatchCommand`, `GetAppSnapshot`, and the hookup in `HandleRegularKey`/`WM_KEYDOWN` | – |
| `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` | `HandleWindowCommand`, `WM_CONTEXTMENU`, the render timer, the `WM_COMMAND` guard | – |
| `code/vis_milk2/plugin.rc`, `resource.h` | Menu labels (`IDS_MENU_*`) | – |
| `code/vis_milk2/plugin.vcxproj(.filters)`, `code/tests/unit_tests.vcxproj` | Adding the files | – |

**Collision point with phase 2:** `Milkdrop2PcmVisualizer.cpp` (phase 2 changes `StartThreads`, and we change `StaticWndProc` and the loop), and both vcxproj files. Whoever merges second must rebase and resolve the conflict by hand. Usually these are adjacent lines in an `<ItemGroup>`.

---

# Phase 3: Command layer

### Task 1: Command core (enum, key table, menu IDs, labels)

**Files:**
- Create: `code/vis_milk2/features/commands.h`, `code/vis_milk2/features/commands.cpp`
- Create: `code/tests/test_commands.cpp`
- Modify: `code/tests/unit_tests.vcxproj` (add `test_commands.cpp` and `..\vis_milk2\features\commands.cpp`), `code/vis_milk2/plugin.vcxproj` and `.filters` (add `features\commands.cpp/.h` under a filter named `features`)

**Interfaces:**
- Produces (`commands.h`, includes `<windows.h>` and `<string>`):

```cpp
enum class Cmd : unsigned short {
    None = 0,
    // preset
    PresetNext, PresetPrev, PresetRandom, PresetLockToggle, PresetOrderToggle,
    PresetLoadMenu, PresetSave, PresetDelete, PresetEditMenu,
    PresetRateUp, PresetRateDown, ShaderLockCycle,
    // view
    ViewFullscreen, ViewStretch, ViewAlwaysOnTop, ViewFps, ViewPresetInfo,
    ViewRating, ViewShaderHelp, ViewSongTitle, FpsLimitCycle, AutoChangeCycle,
    // help / app
    HelpOverlay, HelpUserGuide, HelpAuthoringGuide, HelpAbout, AppQuit,
    Count
};

// Where a key is routed from; each scope is looked up at one place in the window procs.
enum class KeyScope : unsigned char {
    Char,        // WM_CHAR, in CPlugin::HandleRegularKey
    VKeyGlobal,  // WM_KEYDOWN, top of CPlugin::MyWindowProc (works even while editing text, like today's F2-F9)
    VKeyNormal,  // WM_KEYDOWN, CPlugin::MyWindowProc "case 3" (after waitstring and on-screen menu)
    VKeyAlt      // WM_SYSKEYDOWN, StaticWndProc
};

struct KeyBinding { KeyScope scope; unsigned short key; Cmd cmd; };

const KeyBinding* KeyBindings(size_t* count);
Cmd  LookupKey(KeyScope scope, unsigned key);          // Cmd::None if unbound
std::wstring KeyLabel(const KeyBinding& b);            // "F7", "Alt+Enter", "Space", "Backspace", "`"
std::wstring PrimaryKeyLabel(Cmd c);                   // label of c's first binding in table order, L"" if none

const UINT kCmdMenuIdBase = 41000;
UINT CmdToMenuId(Cmd c);                               // kCmdMenuIdBase + (UINT)c
Cmd  MenuIdToCmd(UINT id);                             // Cmd::None outside (None, Count)
```

- [x] **Step 1: Write failing tests** in `code/tests/test_commands.cpp`. This is the full key table, and it preserves today's behavior:

```cpp
TEST(TestCommands_LegacyKeysPreserved) {
    CHECK(LookupKey(KeyScope::Char, 'h') == Cmd::PresetNext);
    CHECK(LookupKey(KeyScope::Char, 'H') == Cmd::PresetNext);
    CHECK(LookupKey(KeyScope::Char, '`') == Cmd::PresetLockToggle);
    CHECK(LookupKey(KeyScope::Char, '~') == Cmd::PresetLockToggle);
    CHECK(LookupKey(KeyScope::Char, 'r') == Cmd::PresetOrderToggle);
    CHECK(LookupKey(KeyScope::Char, 'l') == Cmd::PresetLoadMenu);
    CHECK(LookupKey(KeyScope::Char, 'L') == Cmd::PresetLoadMenu);
    CHECK(LookupKey(KeyScope::Char, 's') == Cmd::PresetSave);
    CHECK(LookupKey(KeyScope::Char, 'S') == Cmd::PresetSave);
    CHECK(LookupKey(KeyScope::Char, 'm') == Cmd::PresetEditMenu);
    CHECK(LookupKey(KeyScope::Char, 'M') == Cmd::PresetEditMenu);
    CHECK(LookupKey(KeyScope::Char, '+') == Cmd::PresetRateUp);
    CHECK(LookupKey(KeyScope::Char, '-') == Cmd::PresetRateDown);
    CHECK(LookupKey(KeyScope::Char, 'd') == Cmd::ShaderLockCycle);
    CHECK(LookupKey(KeyScope::Char, 'D') == Cmd::ShaderLockCycle);
    CHECK(LookupKey(KeyScope::Char, 't') == Cmd::ViewSongTitle);
    CHECK(LookupKey(KeyScope::Char, 'T') == Cmd::ViewSongTitle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F2) == Cmd::FpsLimitCycle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F3) == Cmd::AutoChangeCycle);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F4) == Cmd::ViewPresetInfo);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F5) == Cmd::ViewFps);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F6) == Cmd::ViewRating);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F7) == Cmd::ViewAlwaysOnTop);
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F9) == Cmd::ViewShaderHelp);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_SPACE) == Cmd::PresetRandom);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_BACK) == Cmd::PresetPrev);
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_F1) == Cmd::HelpOverlay);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_RETURN) == Cmd::ViewFullscreen);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_SHIFT) == Cmd::ViewStretch);
    CHECK(LookupKey(KeyScope::VKeyAlt, VK_F4) == Cmd::AppQuit);
}

TEST(TestCommands_UnboundKeysReturnNone) {
    CHECK(LookupKey(KeyScope::Char, 'R') == Cmd::None);        // only lowercase r toggles order today
    CHECK(LookupKey(KeyScope::Char, 'q') == Cmd::None);        // preset-tuning keys stay in the legacy switch
    CHECK(LookupKey(KeyScope::VKeyNormal, VK_F7) == Cmd::None); // scope matters
    CHECK(LookupKey(KeyScope::VKeyGlobal, VK_F1) == Cmd::None);
}

TEST(TestCommands_NoKeyBoundTwice) {
    size_t n = 0; const KeyBinding* b = KeyBindings(&n);
    CHECK(n > 0);
    for (size_t i = 0; i < n; i++) {
        CHECK(b[i].cmd != Cmd::None);
        for (size_t j = i + 1; j < n; j++)
            CHECK(!(b[i].scope == b[j].scope && b[i].key == b[j].key));
    }
}

TEST(TestCommands_MenuIdRoundTrip) {
    for (unsigned c = 1; c < (unsigned)Cmd::Count; c++)
        CHECK(MenuIdToCmd(CmdToMenuId((Cmd)c)) == (Cmd)c);
    CHECK(MenuIdToCmd(0) == Cmd::None);
    CHECK(MenuIdToCmd(kCmdMenuIdBase) == Cmd::None);
    CHECK(MenuIdToCmd(kCmdMenuIdBase + (UINT)Cmd::Count) == Cmd::None);
    CHECK(MenuIdToCmd('s') == Cmd::None);
}

TEST(TestCommands_KeyLabels) {
    CHECK(PrimaryKeyLabel(Cmd::ViewAlwaysOnTop) == L"F7");
    CHECK(PrimaryKeyLabel(Cmd::ViewFullscreen) == L"Alt+Enter");
    CHECK(PrimaryKeyLabel(Cmd::ViewStretch) == L"Alt+Shift");
    CHECK(PrimaryKeyLabel(Cmd::AppQuit) == L"Alt+F4");
    CHECK(PrimaryKeyLabel(Cmd::PresetPrev) == L"Backspace");
    CHECK(PrimaryKeyLabel(Cmd::PresetRandom) == L"Space");
    CHECK(PrimaryKeyLabel(Cmd::PresetLockToggle) == L"`");
    CHECK(PrimaryKeyLabel(Cmd::PresetNext) == L"h");
    CHECK(PrimaryKeyLabel(Cmd::PresetDelete) == L"");
}
```

- [x] **Step 2:** run `.\build.ps1 -Test`. Expected: a compile failure, because `commands.h` doesn't exist yet.
- [x] **Step 3:** implement `commands.h/.cpp`. The table is a `static const KeyBinding` array in the order of the test above (one row per combination). `LookupKey` searches it linearly. `KeyLabel` handles VK_F1–VK_F12, VK_SPACE, VK_BACK, VK_RETURN and VK_SHIFT. A `Char` key is shown as the character itself, and `VKeyAlt` gets an `Alt+` prefix.
- [x] **Step 4:** run `.\build.ps1 -Test`. Expected: all 5 tests pass (`PASS TestCommands_…`) and `0 failed` is printed. Also run `.\build.ps1`; the build is expected to pass with the new file in the `plugin` project.
- [x] **Step 5:** Commit `feat(commands): command enum, key table and menu ids`.

### Task 2: Command state (checked and enabled) and F2/F3 cycles

**Files:**
- Modify: `code/vis_milk2/features/commands.h/.cpp`
- Modify: `code/tests/test_commands.cpp`

**Interfaces:**
- Consumes: `Cmd` from Task 1.
- Produces:

```cpp
struct AppSnapshot {
    bool presetLockedByUser = false, presetLockedByCode = false, sequentialOrder = false;
    bool showFps = false, showPresetInfo = false, showRating = false, showShaderHelp = false, showHelp = false;
    bool alwaysOnTop = false, fullscreen = false, stretch = false;
    bool loadMenuOpen = false, editMenuOpen = false;
    bool textEditActive = false;   // waitstring active, or a Y/N prompt (UI_LOAD_DEL, UI_SAVE_OVERWRITE, UI_UPGRADE_PIXEL_SHADER)
    bool canDeleteCurrent = false; // m_nDirs <= m_nCurrentPreset < m_nPresets
    int  fpsLimit = 0;             // m_max_fps_w, 0 = unlimited
    float autoChangeSeconds = 0;   // m_fTimeBetweenPresets
};
struct CmdState { bool enabled; bool checked; };
CmdState QueryState(Cmd c, const AppSnapshot& s);

int          NextFpsLimit(int current);
std::wstring FpsLimitMessage(int limit);        // L"60 fps", L"unlimited fps"
struct AutoChangeStep { float seconds; float randSeconds; bool lockPreset; const wchar_t* message; };
AutoChangeStep NextAutoChange(float currentSeconds);
```

- [x] **Step 1: Write failing tests.** The values are copied from the existing F2/F3 (`plugin.cpp:4706-4725`). The only change: an unknown value (for example from the ini file) returns 30. Today F2 with an unknown value does nothing.

```cpp
TEST(TestCommands_FpsCycle) {
    CHECK(NextFpsLimit(0) == 30);   CHECK(NextFpsLimit(30) == 60);  CHECK(NextFpsLimit(60) == 90);
    CHECK(NextFpsLimit(90) == 120); CHECK(NextFpsLimit(120) == 144); CHECK(NextFpsLimit(144) == 240);
    CHECK(NextFpsLimit(240) == 360); CHECK(NextFpsLimit(360) == 0); CHECK(NextFpsLimit(75) == 30);
    CHECK(FpsLimitMessage(0) == L"unlimited fps");
    CHECK(FpsLimitMessage(144) == L"144 fps");
}

TEST(TestCommands_AutoChangeCycle) {
    AutoChangeStep a = NextAutoChange(0.0f);
    CHECK(a.seconds == 16.0f && a.randSeconds == 5.0f && !a.lockPreset && std::wstring(a.message) == L"15 sec");
    CHECK(NextAutoChange(16.0f).seconds == 30.0f);
    CHECK(NextAutoChange(30.0f).seconds == 60.0f);
    CHECK(NextAutoChange(60.0f).seconds == 120.0f);
    CHECK(NextAutoChange(120.0f).seconds == 180.0f);
    AutoChangeStep off = NextAutoChange(180.0f);
    CHECK(off.seconds == 0.0f && off.randSeconds == 0.0f && off.lockPreset && std::wstring(off.message) == L"unlimited sec");
    AutoChangeStep odd = NextAutoChange(45.0f);
    CHECK(odd.seconds == 30.0f && !odd.lockPreset && std::wstring(odd.message) == L"30 sec");
}

TEST(TestCommands_StateChecks) {
    AppSnapshot s;
    CHECK(!QueryState(Cmd::PresetLockToggle, s).checked);
    s.presetLockedByUser = true; s.fullscreen = true; s.alwaysOnTop = true; s.showFps = true;
    s.sequentialOrder = true; s.loadMenuOpen = true;
    CHECK(QueryState(Cmd::PresetLockToggle, s).checked);
    CHECK(QueryState(Cmd::ViewFullscreen, s).checked);
    CHECK(QueryState(Cmd::ViewAlwaysOnTop, s).checked);
    CHECK(QueryState(Cmd::ViewFps, s).checked);
    CHECK(QueryState(Cmd::PresetOrderToggle, s).checked);
    CHECK(QueryState(Cmd::PresetLoadMenu, s).checked);
    CHECK(!QueryState(Cmd::PresetNext, s).checked);
    CHECK(QueryState(Cmd::PresetNext, s).enabled);
}

TEST(TestCommands_TextEditDisablesPresetCommands) {   // Review Focus 1
    AppSnapshot s; s.textEditActive = true; s.canDeleteCurrent = true;
    for (unsigned c = (unsigned)Cmd::PresetNext; c <= (unsigned)Cmd::ShaderLockCycle; c++)
        CHECK(!QueryState((Cmd)c, s).enabled);
    CHECK(!QueryState(Cmd::ViewSongTitle, s).enabled);
    CHECK(QueryState(Cmd::ViewFullscreen, s).enabled);
    CHECK(QueryState(Cmd::HelpOverlay, s).enabled);
    CHECK(QueryState(Cmd::AppQuit, s).enabled);
}

TEST(TestCommands_RandomDisabledWhenLockedByCode) {
    AppSnapshot s; s.presetLockedByCode = true;
    CHECK(!QueryState(Cmd::PresetRandom, s).enabled);
    CHECK(QueryState(Cmd::PresetNext, s).enabled);
}

TEST(TestCommands_DeleteNeedsPresetFile) {
    AppSnapshot s;
    CHECK(!QueryState(Cmd::PresetDelete, s).enabled);
    s.canDeleteCurrent = true;
    CHECK(QueryState(Cmd::PresetDelete, s).enabled);
}
```

- [x] **Step 2:** run `.\build.ps1 -Test`. Expected: a compile failure, because the symbols are missing.
- [x] **Step 3:** implement. In `QueryState`, `checked` is set from the matching snapshot field (Lock↔presetLockedByUser, Order↔sequentialOrder, LoadMenu↔loadMenuOpen, EditMenu↔editMenuOpen, HelpOverlay↔showHelp, and each View↔the field with the matching name). `enabled` is false in the cases that appear in the tests, and true in all others.
- [x] **Step 4:** run `.\build.ps1 -Test`. Expected: 11 tests pass and `0 failed` is printed.
- [x] **Step 5:** Commit `feat(commands): command state and F2/F3 cycles`.

### Task 3: `CPlugin::DispatchCommand` and keyboard hookup

**Files:**
- Modify: `code/vis_milk2/plugin.h` (two declarations and `#include "features/commands.h"`)
- Modify: `code/vis_milk2/plugin.cpp`: `HandleRegularKey` (5489–5827), `MyWindowProc` `WM_KEYDOWN` (4695–4755, and "case 3" at 5142–5473)
- Modify: `code/vis_milk2/pluginshell.cpp:1831-1834` (delete `case VK_F1`, because it moves to the table)

**Interfaces:**
- Consumes: `LookupKey`, `NextFpsLimit`, `FpsLimitMessage`, `NextAutoChange`, `AppSnapshot` (Tasks 1–2).
- Produces:
  - `bool CPlugin::DispatchCommand(Cmd c, HWND hWnd);`: returns true if the command was handled. When false, the caller continues down the old path.
  - `AppSnapshot CPlugin::GetAppSnapshot() const;`: fills every field except `fullscreen` and `stretch` (those are filled in Task 4).

- [x] **Step 1:** write `DispatchCommand` in `plugin.cpp`, right before `HandleRegularKey`. The body of each case that moves here is moved as is, and every `break` that returned 1 in the old code becomes `return false`. The exceptions:

| Cmd | Behavior |
|---|---|
| `PresetNext` | In `UI_MASHUP` returns false, and the old `'h'`/`'H'` case stays in the switch with only the mashup branch. Otherwise `NextPreset(0)` and `m_fHardCutThresh *= 2.0f`. |
| `PresetRandom` | In `UI_LOAD`, or if `m_bPresetLockedByCode`, returns false. The old `case VK_SPACE` stays for `goto HitEnterFromLoadMenu`. Otherwise `LoadRandomPreset(m_fBlendTimeUser)`. |
| `PresetDelete` | Returns false if `!GetAppSnapshot().canDeleteCurrent` or text is being edited. Otherwise `UpdatePresetList(); m_nPresetListCurPos = m_nCurrentPreset; m_UI_mode = UI_LOAD_DEL;`, i.e. reuse the existing on-screen Y/N question, without a MessageBox. |
| `FpsLimitCycle` | `m_max_fps_w = m_max_fps_fs = NextFpsLimit(m_max_fps_w)`, then put `FpsLimitMessage` into `m_szSongTitle` and call `LaunchSongTitleAnim()`, as today. |
| `AutoChangeCycle` | From `NextAutoChange`: `m_fTimeBetweenPresets`, `m_fTimeBetweenPresetsRand`, `m_bPresetLockedByUser = step.lockPreset`, and a message as for F2. |
| `HelpOverlay` | `m_show_press_f1_msg = 0; ToggleHelp();` |
| `ViewFullscreen`, `ViewStretch`, `AppQuit`, `Help{UserGuide,AuthoringGuide,About}` | Return false, because these are window commands (Task 4 and Task 6). |

- [x] **Step 2:** hook up three routing points, each with `if (c != Cmd::None && DispatchCommand(c, hWnd)) return 0;`:
  - In `HandleRegularKey`, after the `UI_LOAD`/`UI_MASHUP` checks at the top of the function and before the `switch`: `LookupKey(KeyScope::Char, wParam)`. The function has no `hWnd` parameter, so pass `GetPluginWindow()`. Delete the cases that moved from the switch (except the `'h'` remainder for mashup).
  - In `WM_KEYDOWN`, before the first `switch(wParam)` (4703): `LookupKey(KeyScope::VKeyGlobal, wParam)`. Delete `case VK_F2`–`VK_F7` and `VK_F9`. Keep `VK_SCROLL`.
  - In "case 3" (before the `switch(wParam)` at 5146): `LookupKey(KeyScope::VKeyNormal, wParam)`. Delete `case VK_BACK`, and keep only the `UI_LOAD` branch of `case VK_SPACE`.
- [x] **Step 3:** write `GetAppSnapshot()`. `alwaysOnTop` comes from the static `m_bAlwaysOnTop` at `plugin.cpp:76`. `textEditActive = m_waitstring.bActive ||` `m_UI_mode` is one of `UI_LOAD_DEL`, `UI_SAVE_OVERWRITE` or `UI_UPGRADE_PIXEL_SHADER`.
- [x] **Step 4:** run `.\build.ps1` and `.\build.ps1 -Test`. Expected: the build passes with no new warnings and `0 failed` is printed.
- [x] **Step 5: Manual test (Review Focus 2).** Run `.\build.ps1 -Run` with music and verify that each row in the table behaves as before the change:

| Key | State | Expected |
|---|---|---|
| h, Backspace, Space | Normal | Next (hard cut), previous, random with blend |
| Space | Preset browser (L) | Loads the highlighted preset |
| h / H | Mashup (A twice, if present) or without | Mashup branch as before |
| `, r, d, +, - | Normal | Lock, order (a message is shown), cycle shader lock, rating |
| l, m, s | Normal | Browser, edit menu, Save As |
| F2, F3 | Normal, and inside Save As | FPS cycle and change-time cycle with a message, also while editing (as today) |
| F1 | Normal, and inside Save As | Help appears and disappears. Nothing happens while editing (as today) |
| s, h | Inside Save As | Typed as text, no command triggered |

- [x] **Step 6:** Commit `refactor(commands): route hotkeys through command table`.

### Task 4: Window commands and the full snapshot (end of phase 3)

**Files:**
- Modify: `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` (`StaticWndProc`, 201–248)

**Interfaces:**
- Consumes: `LookupKey(KeyScope::VKeyAlt, …)`, `CPlugin::DispatchCommand`, `CPlugin::GetAppSnapshot`.
- Produces (static in the file; Task 6 uses them):
  - `static bool HandleWindowCommand(HWND hwnd, Cmd c);`: handles `ViewFullscreen` (`ToggleFullScreen`), `ViewStretch` (`ToggleStretch`) and `AppQuit` (`PostQuitMessage(0)`, as today). Returns false for any other command.
  - `static bool RunCommand(HWND hwnd, Cmd c);`: `HandleWindowCommand(hwnd, c) || g_plugin.DispatchCommand(c, hwnd)`.
  - `static AppSnapshot BuildAppSnapshot();`: `g_plugin.GetAppSnapshot()` together with `fullscreen` and `stretch` from the file's statics.

- [x] **Step 1:** in `WM_SYSKEYDOWN`, replace the if chain (F4, SHIFT, RETURN) with `Cmd c = LookupKey(KeyScope::VKeyAlt, wParam); if (c != Cmd::None && RunCommand(hWnd, c)) break;`. Everything else continues to `PluginShellWindowProc` as today. `WM_LBUTTONDBLCLK` calls `RunCommand(hWnd, Cmd::ViewFullscreen)`.
- [x] **Step 2:** run `.\build.ps1` and `.\build.ps1 -Test`. Expected: they pass and `0 failed` is printed.
- [x] **Step 3: Manual test:** Alt+Enter and a double-click enter and leave fullscreen. Alt+Shift turns on Stretch. Alt+F4 exits, with no error in the log.
- [x] **Step 4:** Commit `feat(commands): window-level commands`.
- [x] **Step 5: Approval gate.** Report to the user that the keyboard has moved entirely to the table (Tasks 3–4), and wait for their approval after a manual test before Task 5.

---

# Phase 4: Right-click menu

### Task 5: Menu layout and labels

**Files:**
- Create: `code/vis_milk2/features/context_menu.h`, `code/vis_milk2/features/context_menu.cpp`
- Create: `code/tests/test_context_menu.cpp`
- Modify: `code/vis_milk2/resource.h` and `code/vis_milk2/plugin.rc` (add a new STRINGTABLE block in the range 700–739; first verify with grep that the IDs are free)
- Modify: `unit_tests.vcxproj`, `plugin.vcxproj(.filters)`

**Interfaces:**
- Consumes: `Cmd` (Task 1).
- Produces:

```cpp
enum class MenuNodeKind { Item, Separator, Submenu };
struct MenuNode { MenuNodeKind kind; Cmd cmd; UINT labelId; std::vector<MenuNode> children; };
const std::vector<MenuNode>& MenuLayout();
UINT  CmdLabelId(Cmd c);                                   // 0 if none
POINT MenuAnchor(LPARAM lParam, const RECT& windowRect);   // lParam == -1 (keyboard) -> window centre
```

**Layout and labels** (`IDS_MENU_*`, in English like the rest of the UI; our own wording):

```
Next preset            h            | Previous preset   Backspace | Random preset   Space
---
Preset ▸  Lock current preset `  · Sequential order r · Browse presets... l · Save preset as... s
          Delete current preset... · --- · Rate up + · Rate down - · Cycle shader lock d · Edit preset... m
View ▸    Full screen Alt+Enter · Stretch across monitors Alt+Shift · Always on top F7 · ---
          Show FPS F5 · Show preset name F4 · Show rating F6 · Shader help F9 · Show song title t · ---
          Max FPS: <value> F2 · Auto-change: <value> F3
Help ▸    Keyboard help F1 · User guide · Preset authoring guide · --- · About
---
Exit                   Alt+F4
```
(Submenu labels: `IDS_MENU_PRESET`, `IDS_MENU_VIEW`, `IDS_MENU_HELP`. `<value>` is built in Task 6.)

- [x] **Step 1: Write failing tests:**

```cpp
TEST(TestMenu_EveryCommandExactlyOnce) {
    int seen[(int)Cmd::Count] = {};
    // walk MenuLayout() recursively, ++seen[(int)node.cmd] for every Item
    for (int c = 1; c < (int)Cmd::Count; c++) CHECK(seen[c] == 1);
    CHECK(seen[0] == 0);
}
TEST(TestMenu_LabelsExist) {
    // every Item: node.labelId != 0 && node.labelId == CmdLabelId(node.cmd); every Submenu: labelId != 0
}
TEST(TestMenu_NoEmptySubmenuNoEdgeSeparators) {
    // every Submenu has children; no list starts or ends with a Separator; no two Separators in a row
}
TEST(TestMenu_Anchor) {   // Review Focus 4
    RECT r = { 100, 100, 900, 700 };
    POINT k = MenuAnchor((LPARAM)-1, r);              CHECK(k.x == 500 && k.y == 400);
    POINT m = MenuAnchor(MAKELPARAM(250, 300), r);    CHECK(m.x == 250 && m.y == 300);
    POINT neg = MenuAnchor(MAKELPARAM((WORD)-500, 20), r); CHECK(neg.x == -500 && neg.y == 20);
}
```

- [x] **Step 2:** run `.\build.ps1 -Test`. Expected: FAIL, or a compile failure.
- [x] **Step 3:** implement `context_menu.cpp`. The layout is a `static const std::vector<MenuNode>` built once. `MenuAnchor` uses `GET_X_LPARAM`/`GET_Y_LPARAM` (`<windowsx.h>`) rather than `LOWORD`, because of negative coordinates. Add the strings to `plugin.rc`.
- [x] **Step 4:** run `.\build.ps1 -Test` and `.\build.ps1`. Expected: `0 failed` is printed and the build passes.
- [x] **Step 5:** Commit `feat(menu): context menu layout and labels`.

### Task 6: The menu itself (WM_CONTEXTMENU, cursor, rendering, help and About)

**Files:**
- Create: `code/vis_milk2/features/context_menu_win32.cpp` (`plugin` project only)
- Modify: `code/vis_milk2/features/context_menu.h` (declare `BuildContextMenu`)
- Modify: `code/vis_milk2/Milkdrop2PcmVisualizer.cpp`

**Interfaces:**
- Consumes: `MenuLayout`, `CmdLabelId`, `MenuAnchor`, `QueryState`, `PrimaryKeyLabel`, `CmdToMenuId`/`MenuIdToCmd`, `FpsLimitMessage`, `RunCommand`, `BuildAppSnapshot`, `RenderFrame`.
- Produces: `HMENU BuildContextMenu(const AppSnapshot& s);`. The caller is responsible for `DestroyMenu`.

- [x] **Step 1: `BuildContextMenu`:** build the tree recursively with `CreatePopupMenu` and `AppendMenuW`. The label is `LoadResString(labelId)`. For `FpsLimitCycle` append `L": " + FpsLimitMessage(s.fpsLimit)`, and for `AutoChangeCycle` append `L": "` followed by `L"off"` when 0, or `<n> sec` from `s.autoChangeSeconds`. Then append `L"\t" + PrimaryKeyLabel(cmd)` if it isn't empty. Flags: `MF_GRAYED` when `!enabled`, and `MF_CHECKED` when `checked`.
- [x] **Step 2: `WM_CONTEXTMENU` in `StaticWndProc`**, in this order, returning 0:
  1. `bool wasFullscreen = fullscreen; if (wasFullscreen) ShowCursor(TRUE);`
  2. `HMENU m = BuildContextMenu(BuildAppSnapshot()); RECT wr; GetWindowRect(hWnd, &wr); POINT p = MenuAnchor(lParam, wr);`
  3. `SetForegroundWindow(hWnd); UINT id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL); PostMessageW(hWnd, WM_NULL, 0, 0); DestroyMenu(m);`
  4. `Cmd c = MenuIdToCmd(id); if (c != Cmd::None) RunCommand(hWnd, c);`
  5. `if (wasFullscreen) ShowCursor(FALSE);`. Call this once in every case, even if the command left fullscreen, because `ToggleFullScreen` already added its own `ShowCursor(TRUE)`, so the counter ends up balanced.
- [x] **Step 3: Rendering while the menu is open:** `WM_ENTERMENULOOP` calls `SetTimer(hWnd, kMenuRenderTimerId /*0x5D01*/, 16, NULL)`, `WM_TIMER` with that ID calls `GetAudioBuf(pcmLeftIn, pcmRightIn, SAMPLE_SIZE); RenderFrame();` and returns 0, and `WM_EXITMENULOOP` calls `KillTimer`. This also fixes the system menu (Alt+Space).
- [x] **Step 4: `WM_COMMAND` guard (Review Focus 5):** in `StaticWndProc`, `case WM_COMMAND:` with `MenuIdToCmd(LOWORD(wParam)) != Cmd::None` runs `RunCommand` and returns 0, without passing on to `PluginShellWindowProc`. Phase 7 (tray icon) will use this.
- [x] **Step 5: Help commands in `HandleWindowCommand`:**
  - `HelpUserGuide`: the path is `g_plugin.m_szMilkdrop2Path + L"docs\\user-guide.html"`. `HelpAuthoringGuide`: `g_plugin.m_szMilkdrop2Path + L"docs\\preset-authoring.html"`. If `GetFileAttributesW` returns `INVALID_FILE_ATTRIBUTES`, show `g_plugin.AddError(<IDS_MENU_GUIDE_MISSING>, 3.0f, ERR_NOTIFY, false)`. Otherwise `ShellExecuteW(hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL)`, and in fullscreen leave fullscreen first (`ToggleFullScreen`) so the browser doesn't open behind a TOPMOST window.
  - `HelpAbout`: `MessageBoxW(hwnd, text, L"About", MB_OK | MB_ICONINFORMATION)`. The text is `L"MilkDrop " MILKDROP_VERSION_STRW L"\n\nBased on MilkDrop 2 by Ryan Geiss (Nullsoft).\nBSD license. See LICENSE."`, and in fullscreen the cursor is already shown thanks to Step 2.
- [x] **Step 6:** run `.\build.ps1` and `.\build.ps1 -Test`. Expected: they pass and `0 failed` is printed.
- [x] **Step 7: Manual test (Review Focus 3–5):**
  - In a window: right-click opens the menu. The check marks match the state (F5 then opening the menu shows "Show FPS" checked). A chosen item does exactly what its key does. While the menu is open, the visualization keeps moving.
  - In fullscreen (Alt+Enter): the menu appears above the window and the cursor is visible. After closing with Esc the cursor disappears. After "Full screen" from the menu, you're back in a window and the cursor is visible. After entering fullscreen again, the cursor is hidden.
  - Shift+F10 and the Apps key open the menu in the center of the window.
  - While Save As is open (s): the Preset items and Next, Previous and Random are greyed out.
  - "Delete current preset..." shows the on-screen Y/N question. N or Esc does not delete.
  - "User guide" opens `user-guide.html` in the browser. If the file is temporarily renamed, a message is shown and there is no crash.
- [x] **Step 8:** Commit `feat(menu): right-click context menu`.

### Task 7: Audio submenu (depends on phase 2)

**Gate:** phase 2 is done, but it does not expose an API for listing or choosing devices. The capture thread always follows the default device, and `followDefault` is internal only. So **Task 7 is postponed** until the user approves adding the contract below to `code/audio/`. Until then the Audio menu does not appear, and there are no disabled items.

**Interfaces:**
- Consumes (contract, `code/audio/audio-control.h`):

```cpp
struct AudioDeviceEntry { std::wstring id; std::wstring name; bool isDefault; };
std::vector<AudioDeviceEntry> AudioListRenderDevices();   // DEVICE_STATE_ACTIVE render endpoints
bool         AudioIsFollowingDefault();
std::wstring AudioCurrentDeviceId();
void AudioRequestFollowDefault();                         // async: capture loop restarts on default
void AudioRequestDevice(const std::wstring& id);          // async: pin this device (followDefault=false)
```
- Produces: `Cmd::AudioFollowDefault` (added before `Count`), `MenuNodeKind::AudioDevices` (a node expanded at build time), `const UINT kAudioDeviceMenuIdBase = 41500; const int kMaxAudioMenuDevices = 32; int AudioDeviceIndexFromMenuId(UINT id); // -1 outside range`.

- [ ] **Step 1: Failing tests:** `TestMenu_AudioDeviceIdRange` (`AudioDeviceIndexFromMenuId(41500) == 0`, `(41531) == 31`, `(41532) == -1`, `(41499) == -1`). Update `TestMenu_EveryCommandExactlyOnce` to include `AudioFollowDefault`, and check that `MenuIdToCmd(41500) == Cmd::None`.
- [ ] **Step 2:** run `.\build.ps1 -Test`. Expected: FAIL.
- [ ] **Step 3:** implement. "Audio ▸" goes between View and Help, and contains: "Follow Windows default" (radio, checked when `AudioIsFollowingDefault()`), `---`, followed by the devices (radio, checked when `id == AudioCurrentDeviceId()` and `!following`). The vector is kept in a static during `BuildContextMenu`, and a choice in `WM_CONTEXTMENU` (whose value is outside `MenuIdToCmd`'s range) calls `AudioRequestDevice(list[idx].id)`. When there are no devices, a greyed-out "No playback devices" item is shown.
- [ ] **Step 4:** run `.\build.ps1 -Test` and `.\build.ps1`. Expected: `0 failed` is printed.
- [ ] **Step 5: Manual test:** choose a different device; the graphics react to its sound within less than 3 seconds (the phase 2 requirement). "Follow Windows default" restores following. Disconnecting the chosen device returns to the default (phase 2).
- [ ] **Step 6:** Commit `feat(menu): audio device submenu`.

### Task 8: Closing the phases

**Files:**
- Modify: `docs/PROJECT_STATUS.md` (a row in the "Work done" table), `docs/superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md` (the status of phases 3 and 4 and a link to this file)

- [x] **Step 1:** run `.\build.ps1 -Clean`, then `.\build.ps1 -Test`. Expected: a clean Rebuild with only 2 C4477 warnings, and `0 failed`.
- [x] **Step 2:** update the documents. For the **user guide** (it is built in a separate session and not edited here): pass the key table from Task 1 and the menu layout from Task 5 to the user, so they can hand them to the guide's session.
- [x] **Step 3:** Commit `docs: phase 3-4 status`. Then `superpowers:finishing-a-development-branch` (merge or PR, as the user decides).
