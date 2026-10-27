# Phases 8–9: Now Playing song info, sprites and messages – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** (8) When a new song starts in any player that reports to Windows (Spotify, a browser, Media Player), the artist and song name are shown on screen. (9) Images (sprites) and text messages over the visualization: loading PNG/JPG/GIF with transparency, moving with the mouse, resizing with the wheel, blend modes, and saving inside the preset.

**Architecture:** as in phases 3–7: pure logic in `code/vis_milk2/features/` with tests in `unit_tests`, and small hookups in the existing code. In phase 8, a single file (`now_playing_winrt.cpp`) compiles as C++17 with C++/WinRT and runs on its own thread. It writes to a mutex-protected box (`SongInfoBox`), and `CPlugin::GetSongTitle` (which returns an empty string today) reads from it. The existing `m_szSongTitlePrev` and `LaunchSongTitleAnim` mechanism does the rest. In phase 9 the existing MilkDrop 2 `texmgr` is extended rather than writing a new engine: each slot gains a user offset, scale and blend, applied after the sprite's EEL code. A sprite loaded from a file or a preset is a "static" sprite: its init code is built from the saved position, and it has no per-frame code.

**Tech Stack:** C++14 in `plugin` (`CharacterSet=NotSet`), except `now_playing_winrt.cpp`, which compiles as C++17 (a per-file setting). C++17 and Unicode in `unit_tests`. C++/WinRT from the Windows SDK (`windowsapp.lib`), GDI+ (`gdiplus.lib`, part of Windows), `comdlg32.lib`, D3D9/D3DX9, `mini_test.h`.

**Spec:** [2026-10-24-milkdrop-suri-roadmap.md](2026-10-24-milkdrop-suri-roadmap.md) phases 8 and 9; [PROJECT_STATUS.md](../../PROJECT_STATUS.md).

## Decisions (approved by the user)

The plan is built on these decisions. Changing one of them changes the Task noted next to it.

1. **Message format:** `Artist – Song title` (an en dash, U+2013). If there is no artist, only the song title is shown. If there is no song title, nothing is shown. (Task 1)
2. **A paused player** still counts as "playing", so the title is not shown again when playback resumes. Only when there is no session at all (the player was closed) are the details cleared. (Task 2)
3. **Moving is by dragging with the left button** on the sprite, not the right one, because the right button opens our menu. Resizing is with the wheel while the cursor is over the sprite. (Task 7)
4. **A new blend mode 5, "image transparency":** it uses the file's alpha channel (PNG/GIF). MilkDrop 2's modes 0–4 do not change, because in mode 0 they ignore the texture's alpha, so old presets look as before. A new sprite loaded from a file starts in mode 5. (Task 6)
5. **Animated GIF:** only the first frame is loaded. Full animation would be a separate addition if wanted. (Task 6)
6. **A sprite saved in a preset is saved as a static image** at its position and size at the moment of saving. Motion from the EEL code of `milk2_img.ini` is not saved. (Task 8)
7. **Preset sprites change with the preset:** when a preset loads, the sprites belonging to the previous preset disappear. Sprites launched manually (K or from the menu) stay, as in MilkDrop 2. (Task 8)

## Global Constraints

- **Approval gates:** start only after the user approves the plan and the decisions above. Stop for a manual test and approval at the end of phase 8 (after Task 2) and at the end of phase 9 (after Task 8).
- **Base:** phase 8 was done on the `suri/phase-8` branch, in the `..\MilkDrop3-phase8` worktree, from the tip of `suri/phase-5-7`. This happened while another session worked on phases 5–7 in the main tree. Phase 9 starts only after phases 5–7 are merged. Before any whole-tree operation (stash/reset/checkout), run `ListAgents` and `git status`.
- Our keys in `.milk` start with `suri_sprite`.
- The sample files that are generated (Task 3) use only textures from `content/textures` (source documented in `content/PROVENANCE.md`).
- The pure `features/*.cpp` code compiles as C++14, as Unicode and as MBCS: no `std::optional`, no structured bindings, and explicit calls to the `W` versions. Only `now_playing_winrt.cpp` is C++17, and it is not included in `unit_tests`.
- **`CState` is copied with `memcpy`** (`CState::Import`, line ~1146). Every new field in it must be trivially copyable: fixed arrays, no `std::vector` and no `std::wstring`.
- **Numbers in text** (sprite init code, values in `.milk`) are written and read in the "C" locale, i.e. always with a decimal point, even when Windows is set to a comma.
- No new external dependencies. `windowsapp.lib`, `gdiplus.lib` and `comdlg32.lib` are system libraries. Win32 (x86), v143.
- Each Task ends when `.\build.ps1` passes with no errors and no new warnings, and `.\build.ps1 -Test` prints `0 failed`.
- Every new Cmd appears in the menu exactly once (`TestMenu_EveryCommandExactlyOnce`) and has a label (`TestMenu_LabelsExist`). **New `IDS_*` IDs start at 800.** The range 737–799 is reserved for phases 5–7.
- One commit per Task. The commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A song title longer than 511 characters, or with an emoji exactly at the cut point:** the existing callers pass `sizeof(buf)`, i.e. bytes rather than characters, to `GetSongTitle`. That is a 2x buffer overflow. Expected: no crash, and the cut never breaks a surrogate pair. Tested in `TestSongInfo_TruncateKeepsSurrogatePair` (Task 1), and by fixing the callers to use `_countof` (Task 2).
2. **Exiting the program while the Now Playing thread is waiting on Windows**, or a computer where the API does not exist or fails: exit is expected to finish within less than 3 seconds, and without the API the feature is silently turned off (a disabled menu item and one LOG line). Tested manually in Task 2. The timeouts are defined in Task 2.
3. **`milk2_msg.ini` with no messages, and Y or "random message":** the existing code computes `rand()%count` with `count==0`, which is a crash (division by zero). The automatic draw (`m_fTimeBetweenRandomCustomMsgs`) reaches it too. Expected: nothing happens, and a short message is shown only when the user asked. Tested in `TestMessages_PickRandomNone` (Task 3).
4. **A preset referring to a sprite whose file was deleted or moved, or a Hebrew path:** the preset is expected to load, one error with the file name to be shown, and a Hebrew path to be saved and read back unchanged (UTF-8 in the `.milk` file). Tested in `TestSpritePreset_HebrewPathRoundTrip`, `TestSpritePreset_MissingImgSkipped` (Task 5), and manually in Task 8.
5. **Windows with a decimal comma (for example a German locale):** sprite init code like `x=0,5;` is invalid EEL, and such a value in `.milk` is read as 0. Numbers are expected to always be written and read with a point. Tested in `TestSpriteInitCode_IgnoresLocale` and `TestSpritePreset_ReadIgnoresLocale` (Task 5).

---

## File structure

| File | Responsibility | Built in |
|---|---|---|
| `code/vis_milk2/features/now_playing.h/.cpp` | `SongInfo`, message formatting, UTF-16 truncation, change detection, `SongInfoBox` | plugin + tests |
| `code/vis_milk2/features/now_playing_winrt.cpp` | `CNowPlaying`: the thread that calls `GlobalSystemMediaTransportControlsSessionManager` | plugin only (C++17) |
| `code/vis_milk2/features/messages.h/.cpp` | Random message choice, ini file templates | plugin + tests |
| `code/vis_milk2/features/sprite_layout.h/.cpp` | Sprite corners on screen, hit test, coordinate conversions, wheel, file type | plugin + tests |
| `code/vis_milk2/features/sprite_preset.h/.cpp` | `SpriteDesc`, init code, writing and reading `suri_sprite*` in `.milk`, relative paths | plugin + tests |
| `code/vis_milk2/features/image_load_win32.cpp/.h` | Converting GIF to an in-memory PNG through GDI+ | plugin only |
| `code/tests/test_now_playing.cpp`, `test_messages.cpp`, `test_sprite_layout.cpp`, `test_sprite_preset.cpp` | Tests | tests |
| `code/vis_milk2/features/commands.h/.cpp`, `context_menu.cpp` | New Cmds, extended `AppSnapshot`, Sprites and Messages menus | plugin + tests |
| `code/vis_milk2/texmgr.h/.cpp` | User fields in `td_tex`, GIF loading | – |
| `code/vis_milk2/milkdropfs.cpp` | Drawing the sprite through `SpriteCorners`, blend 5, the user fields | – |
| `code/vis_milk2/state.h/.cpp` | `m_sprites` in `CState`, Import and Export | – |
| `code/vis_milk2/plugin.cpp/.h` | `GetSongTitle`, `m_nowPlaying`, `LaunchStaticSprite`, mouse, Dispatch, load and save | – |
| `code/vis_milk2/plugin.rc`, `resource.h`, both vcxproj files and the filters | Strings, files, libraries | – |
| `code/resources/Milkdrop2/docs/user-guide.html` | New chapters: Now Playing, Sprites, Messages | – |

---

# Phase 8: Now Playing song info

### Task 1: The pure part of Now Playing

**Files:**
- Create: `code/vis_milk2/features/now_playing.h`, `code/vis_milk2/features/now_playing.cpp`, `code/tests/test_now_playing.cpp`
- Modify: `code/tests/unit_tests.vcxproj`, `code/vis_milk2/plugin.vcxproj` (+ `.filters`)

**Interfaces:**
- Produces:
  ```cpp
  struct SongInfo { std::wstring title, artist; };
  std::wstring FormatSongInfo(const SongInfo& s);                 // L"Artist \u2013 Title", L"Title", or L""
  std::wstring TruncateUtf16(const std::wstring& s, size_t maxUnits); // never ends on a lone high surrogate
  bool SongChanged(const std::wstring& prev, const std::wstring& cur); // cur non-empty && cur != prev
  enum class NowPlayingState { Unknown, Available, Unavailable };
  class SongInfoBox {                                              // thread-safe (std::mutex)
  public:
      void Set(const SongInfo& s);        // stores FormatSongInfo(s)
      std::wstring Text() const;
      void SetState(NowPlayingState st);
      NowPlayingState State() const;
  };
  ```

- [x] **Step 1: Write failing tests** in `test_now_playing.cpp`:
  ```cpp
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
  ```
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure (`now_playing.h` doesn't exist).
- [x] **Step 3:** implement `now_playing.cpp` according to the Interfaces. Add the file to `plugin.vcxproj` and to `unit_tests.vcxproj` (like `ui_scale.cpp`).
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: all `TestSongInfo_*` pass and `0 failed`.
- [x] **Step 5:** commit `feat(nowplaying): song info formatting and thread-safe box`.

### Task 2: The WinRT thread and hooking into the program

**Files:**
- Create: `code/vis_milk2/features/now_playing_winrt.cpp`, `now_playing_winrt.h`
- Modify: `code/vis_milk2/plugin.vcxproj` (+ `.filters`), `plugin.h`, `plugin.cpp`, `features/commands.h/.cpp`, `features/context_menu.cpp`, `plugin.rc`, `resource.h`, `code/tests/test_commands.cpp`, `code/resources/Milkdrop2/docs/user-guide.html`

**Interfaces:**
- Consumes: `SongInfoBox`, `SongChanged`, `TruncateUtf16` (Task 1).
- Produces:
  ```cpp
  class CNowPlaying {
  public:
      void Start(SongInfoBox* box);   // starts the poll thread; returns at once
      void Stop();                    // signals and joins; at most kNowPlayingJoinMs
  };
  const DWORD kNowPlayingPollMs = 2000, kNowPlayingCallTimeoutMs = 1500, kNowPlayingJoinMs = 3000;
  ```
  `Cmd::ViewNowPlaying`; `AppSnapshot::nowPlayingAvailable`, `nowPlayingOn`, `hasSongTitle`.

- [x] **Step 1: Failing tests** in `test_commands.cpp`:
  ```cpp
  TEST(TestCommands_NowPlayingState) {
      AppSnapshot s;
      s.nowPlayingAvailable = false; s.nowPlayingOn = true;
      CHECK(!QueryState(Cmd::ViewNowPlaying, s).enabled);
      s.nowPlayingAvailable = true;
      CHECK(QueryState(Cmd::ViewNowPlaying, s).enabled);
      CHECK(QueryState(Cmd::ViewNowPlaying, s).checked);
  }
  TEST(TestCommands_SongTitleNeedsTitle) {
      AppSnapshot s;
      s.hasSongTitle = false;
      CHECK(!QueryState(Cmd::ViewSongTitle, s).enabled);
      s.hasSongTitle = true;
      CHECK(QueryState(Cmd::ViewSongTitle, s).enabled);
  }
  ```
  `Cmd::ViewNowPlaying` is added after `ViewSongTitle` in the enum and in the View menu, with no key. The label: `IDS_MENU_VIEW_NOW_PLAYING` (800) "Show &now playing".
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3: Compile settings.** In `plugin.vcxproj`, at the file level for `now_playing_winrt.cpp` only: `<LanguageStandard>stdcpp17</LanguageStandard>` and `<ConformanceMode>true</ConformanceMode>`. Add `windowsapp.lib` to `AdditionalDependencies`. The file defines `NOMINMAX` and `WIN32_LEAN_AND_MEAN` before any include, and includes no plugin header except `now_playing.h` and `now_playing_winrt.h`. Check: `.\build.ps1` passes with a file that includes only `<winrt/Windows.Media.Control.h>` and empty `Start`/`Stop`.
- [x] **Step 4: Implement `CNowPlaying`.**
  - The thread is created with `_beginthreadex` and gets a stop event (`CreateEventW`, manual reset).
  - At the start of the thread: `winrt::init_apartment(winrt::apartment_type::multi_threaded)`, then `GlobalSystemMediaTransportControlsSessionManager::RequestAsync()`.
  - **Every** async call waits with `op.wait_for(std::chrono::milliseconds(kNowPlayingCallTimeoutMs))`. If the result is not `AsyncStatus::Completed`, call `op.Cancel()` and skip the round. Do not use `.get()` or `co_await`, so `/await` is not needed.
  - If `RequestAsync` throws or does not complete: `box->SetState(Unavailable)`, a `LOG(L"nowplaying: unavailable (0x%08X)")` line, and the thread exits. Otherwise: `SetState(Available)`.
  - Loop: `WaitForSingleObject(stopEvent, kNowPlayingPollMs)`. On each round: `GetCurrentSession()`. If it is null, `box->Set(SongInfo{})`. Otherwise: `TryGetMediaPropertiesAsync()`, `Title()` and `Artist()` (decision 2: the playback state is not checked).
  - The whole round body is wrapped in `try { } catch (winrt::hresult_error const&) { } catch (...) { }`, and an error keeps the previous value.
  - `Stop()`: `SetEvent`, then `WaitForSingleObject(thread, kNowPlayingJoinMs)`. If it times out, log with `LOG` and continue without `TerminateThread`, because the process is exiting anyway.
- [x] **Step 5: Hookup in `CPlugin`.**
  - Fields: `SongInfoBox m_songBox; CNowPlaying m_nowPlaying; bool m_bNowPlaying;`. In the ini: `bNowPlaying` (default 1), read in `MyReadConfig` and written in `MyWriteConfig`, next to `bSongTitleAnims`.
  - `AllocateMyNonDx9Stuff`: if `m_bNowPlaying`, then `m_nowPlaying.Start(&m_songBox)`. `CleanUpMyNonDx9Stuff`: `m_nowPlaying.Stop()`.
  - `GetSongTitle(wchar_t* sz, int nSize)` (plugin.cpp ~8095): if `m_bNowPlaying`, copy `TruncateUtf16(m_songBox.Text(), nSize - 1)`. Otherwise an empty string. **Fix both callers** (~3300, ~3544) to use `_countof(...)` instead of `sizeof(...)`.
  - At ~3301 replace `wcscmp(...)` with `SongChanged(m_szSongTitlePrev, m_szSongTitle)`. Always update `m_szSongTitlePrev` when the text changed, even to empty, so the same song is announced again after the player is closed and reopened.
  - `Dispatch(Cmd::ViewNowPlaying)`: toggles `m_bNowPlaying`, `Start` or `Stop`, `m_songBox.Set(SongInfo{})` when turning off, a short `AddError` "Now playing: on/off" (`IDS_NOW_PLAYING_ON` 801, `IDS_NOW_PLAYING_OFF` 802), and saving to the ini.
  - An existing test in `test_commands.cpp` that expects `ViewSongTitle` to be enabled with an empty `AppSnapshot` gets `s.hasSongTitle = true`.
  - In the snapshot: `nowPlayingAvailable = m_songBox.State() != NowPlayingState::Unavailable`, `nowPlayingOn = m_bNowPlaying`, `hasSongTitle = m_szSongTitle[0] != 0`.
- [x] **Step 6:** `user-guide.html`: a paragraph on Now Playing: which players (anything shown in the Windows media controls), T shows it again, and the item in the View menu.
- [x] **Step 7:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`, including `TestMenu_EveryCommandExactlyOnce` and `TestMenu_LabelsExist`.
- [x] **Step 8: Manual test (user):** Spotify or YouTube in a browser: a song change shows `Artist – Song` ✔; a song with a Hebrew name is shown correctly and in the right direction ✔; a name with an emoji does not break the text ✔; pause and play do not show it again ✔; with no player at all nothing is shown, and T is disabled in the menu ✔; turning it off from the menu persists after a restart ✔; exiting with Alt+F4 while a song plays takes less than 3 seconds ✔ (Review Focus 2).
- [x] **Step 9:** commit `feat(nowplaying): show the current song from Windows media controls`. **Stop for phase 8 approval.**

---

# Phase 9: Sprites and messages

### Task 3: Checking what exists, fixing the random message, and sample files

**Files:**
- Create: `code/vis_milk2/features/messages.h/.cpp`, `code/tests/test_messages.cpp`
- Modify: `plugin.cpp` (`LaunchCustomMessage` ~7733), both vcxproj files

**Interfaces:**
- Produces:
  ```cpp
  int PickRandomMessage(const bool* hasText, int count, int randomValue); // -1 if none; else the k-th (randomValue % n) non-empty index
  std::wstring MessagesIniTemplate();  // [font00], [message00..01] with comments
  std::wstring SpritesIniTemplate();   // [img00..01] using textures that ship in content/textures
  bool WriteUtf16IniIfMissing(const wchar_t* path, const std::wstring& text); // UTF-16LE + BOM; false if exists or fails
  ```

- [x] **Step 1: Manual check of what exists (no code).** Create `build\Release\plugins\Milkdrop2\milk2_msg.ini` and `milk2_img.ini` by hand (`GetConfigIniFile()` is in the same folder). Y then 00 shows a message; K then 00 shows a sprite; Shift+K then 00 removes it. Record in this plan what works and what doesn't. A finding not covered by Tasks 3–8 is shown to the user before continuing.
- [x] **Step 2: Failing tests** in `test_messages.cpp`:
  ```cpp
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
      CHECK(SpritesIniTemplate().find(L"[img00]") != std::wstring::npos);
      CHECK(SpritesIniTemplate().find(L"textures\\") != std::wstring::npos);
  }
  ```
- [x] **Step 3:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 4:** implement. In `LaunchCustomMessage`, when `nMsgNum < 0`, use `PickRandomMessage` (a `bool` array from `m_CustomMessage[i].szText[0]`) and return if -1 was returned. The `milk2_msg.ini` template includes one English message and one Hebrew message (this shows that the file must be saved as UTF-16; say so in a comment in the file). The `milk2_img.ini` template includes two sprites: one static and one with `code_1` that moves it by `bass`. The images are two existing files from `content/textures`, for example `OIchess1.jpg` and `OIbeans1.jpg`.
- [x] **Step 5:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 6:** commit `fix(messages): no crash on random message when none are defined`.

### Task 4: Sprite geometry (pure)

**Files:**
- Create: `code/vis_milk2/features/sprite_layout.h/.cpp`, `code/tests/test_sprite_layout.cpp`
- Modify: both vcxproj files

**Interfaces:**
- Produces:
  ```cpp
  struct SpriteXform { float x, y, sx, sy, rot; bool flipx, flipy; }; // x,y already in clip space (var_x*2-1)
  // Same math and vertex order as milkdropfs.cpp (v3[0..3], triangle strip), without the burn-in step.
  void SpriteCorners(const SpriteXform& t, float imgAspectHW, float screenAspectWH, float out[4][2]);
  bool PointInSpriteQuad(const float q[4][2], float px, float py);  // strip order: polygon 0,1,3,2
  struct SpriteHit { bool live; int startFrame; float q[4][2]; };
  int  TopmostSpriteAt(const SpriteHit* items, int n, float px, float py); // newest live hit, -1 if none
  void WindowToClip(int mx, int my, int clientW, int clientH, float* cx, float* cy); // cy = 1 - 2*my/h
  void ClipDeltaToSpriteDelta(float dcx, float dcy, float screenAspectWH, float* dx, float* dy); // undoes the "second aspect ratio" step
  float WheelScale(float scale, int wheelDelta);  // * 1.1^(delta/120), clamped [0.05, 10]
  enum class SpriteImageKind { D3dx, Gif, Unsupported };
  SpriteImageKind ImageKindFromPath(const wchar_t* path); // .png .jpg .jpeg .bmp .tga .dds -> D3dx; .gif -> Gif; case-insensitive
  ```

- [x] **Step 1: Failing tests** in `test_sprite_layout.cpp` (float comparison with `fabsf(a-b) < 1e-4f`, through a local `Near` helper):
  ```cpp
  TEST(TestSpriteLayout_SquareAtCenter) {
      float q[4][2];
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1.0f, 1.0f, q);
      CHECK(Near(q[0][0], -0.5f) && Near(q[0][1], -0.5f));
      CHECK(Near(q[3][0],  0.5f) && Near(q[3][1],  0.5f));
      CHECK(PointInSpriteQuad(q, 0, 0));
      CHECK(!PointInSpriteQuad(q, 0.9f, 0.9f));
  }
  TEST(TestSpriteLayout_WideImageAndWideScreen) {
      float q[4][2];
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 0.5f, 2.0f, q); // image 2:1, screen 2:1
      CHECK(Near(q[3][0], 0.5f));
      CHECK(Near(q[3][1], 0.5f));   // 0.5 * 0.5 (image) * 2 (screen)
  }
  TEST(TestSpriteLayout_Rotated45) {
      float q[4][2];
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0.785398f, false, false }, 1.0f, 1.0f, q);
      CHECK(PointInSpriteQuad(q, 0, 0.6f));    // a corner now points up
      CHECK(!PointInSpriteQuad(q, 0.45f, 0.45f));
  }
  TEST(TestSpriteLayout_TopmostIsNewest) {
      SpriteHit h[3] = {};
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[0].q); h[0].live = true; h[0].startFrame = 10;
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[1].q); h[1].live = true; h[1].startFrame = 20;
      SpriteCorners(SpriteXform{ 0, 0, 0.5f, 0.5f, 0, false, false }, 1, 1, h[2].q); h[2].live = false; h[2].startFrame = 30;
      CHECK(TopmostSpriteAt(h, 3, 0, 0) == 1);
      CHECK(TopmostSpriteAt(h, 3, 0.9f, 0.9f) == -1);
  }
  TEST(TestSpriteLayout_WindowToClip) {
      float x, y;
      WindowToClip(0, 0, 800, 600, &x, &y);     CHECK(Near(x, -1) && Near(y, 1));
      WindowToClip(400, 300, 800, 600, &x, &y); CHECK(Near(x, 0) && Near(y, 0));
  }
  TEST(TestSpriteLayout_DragDeltaUndoesAspect) {
      float dx, dy;
      ClipDeltaToSpriteDelta(0.1f, 0.1f, 16.0f / 9.0f, &dx, &dy);
      CHECK(Near(dx, 0.1f) && Near(dy, 0.1f * 9.0f / 16.0f));
      ClipDeltaToSpriteDelta(0.1f, 0.1f, 0.5f, &dx, &dy);
      CHECK(Near(dx, 0.05f) && Near(dy, 0.1f));
  }
  TEST(TestSpriteLayout_WheelScale) {
      CHECK(Near(WheelScale(1.0f, 120), 1.1f));
      CHECK(Near(WheelScale(1.1f, -120), 1.0f));
      CHECK(Near(WheelScale(9.9f, 1200), 10.0f));
      CHECK(Near(WheelScale(0.06f, -1200), 0.05f));
  }
  TEST(TestSpriteLayout_ImageKind) {
      CHECK(ImageKindFromPath(L"C:\\a\\b.PNG") == SpriteImageKind::D3dx);
      CHECK(ImageKindFromPath(L"x.jpeg") == SpriteImageKind::D3dx);
      CHECK(ImageKindFromPath(L"x.Gif") == SpriteImageKind::Gif);
      CHECK(ImageKindFromPath(L"x.webp") == SpriteImageKind::Unsupported);
      CHECK(ImageKindFromPath(L"noext") == SpriteImageKind::Unsupported);
  }
  ```
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement. `SpriteCorners` is a copy of the math in `milkdropfs.cpp` (~2955–3000): flip, image aspect, rotation, translation and screen aspect, without the burn-in step. `PointInSpriteQuad`: a cross-product sign check for each of the 4 edges in the order 0→1→3→2. A point is inside when all signs are equal, which also works with a flip that reverses the winding.
- [x] **Step 4:** `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(sprites): sprite geometry, hit test and wheel scaling`.

### Task 5: Sprites in a preset (pure)

**Files:**
- Create: `code/vis_milk2/features/sprite_preset.h/.cpp`, `code/tests/test_sprite_preset.cpp`
- Modify: both vcxproj files

**Interfaces:**
- Produces:
  ```cpp
  const int kMaxPresetSprites = 8;
  const int kSpriteBlendImageAlpha = 5;          // new mode, see decision 4
  struct SpriteDesc {                              // trivially copyable: lives inside CState
      wchar_t img[MAX_PATH];                       // relative to the Milkdrop2 dir, or absolute
      float x, y, scale, rot;                      // x,y in 0..1 like var_x/var_y
      int blend;                                   // 0..5
      unsigned colorkey;                           // 0x00RRGGBB
  };
  std::string SpriteInitCode(const SpriteDesc& d);   // "x=0.5000;y=0.5000;sx=0.5000;sy=0.5000;rot=0.0000;blendmode=5;" in "C" locale
  std::string WritePresetSprites(const SpriteDesc* s, int n); // lines "suri_sprite_count=N\n" + per-sprite keys, UTF-8 paths
  typedef std::function<bool(const char* key, std::string* value)> PresetLookup;
  int  ReadPresetSprites(const PresetLookup& get, SpriteDesc* out, int maxOut); // returns count, skips bad entries
  std::wstring MakeSpritePathRelative(const std::wstring& baseDir, const std::wstring& path); // strips baseDir prefix (case-insensitive)
  std::wstring ResolveSpritePath(const std::wstring& baseDir, const std::wstring& img);     // prepends baseDir unless "X:" or "\\\\"
  ```
  The keys in the file: `suri_sprite_count`, then for each i: `suri_sprite<i>_img`, `_x`, `_y`, `_scale`, `_rot`, `_blend`, `_colorkey` (hex, `0x%06X`). Float values in `%.4f` format.
  Reading rules: a missing key gets a default (x=y=0.5, scale=1, rot=0, blend=5, colorkey=0); count is clamped to `[0, maxOut]`; an entry without `_img` or with an empty `_img` is skipped; scale is clamped to `[0.05, 10]`; x and y to `[-1, 2]`; a blend outside `[0, 5]` becomes 5.

- [x] **Step 1: Failing tests** in `test_sprite_preset.cpp`. A local helper `MapLookup(const std::map<std::string,std::string>&)` that returns a `PresetLookup`, and a helper that splits `WritePresetSprites` output into `key=value` lines in a map:
  ```cpp
  TEST(TestSpritePreset_RoundTrip) {
      SpriteDesc a = {}; wcscpy_s(a.img, L"textures\\OIchess1.jpg");
      a.x = 0.25f; a.y = 0.75f; a.scale = 0.5f; a.rot = 0.1f; a.blend = 2; a.colorkey = 0x102030;
      std::map<std::string, std::string> m = ParseLines(WritePresetSprites(&a, 1));
      CHECK(m["suri_sprite_count"] == "1");
      CHECK(m["suri_sprite0_colorkey"] == "0x102030");
      SpriteDesc b[kMaxPresetSprites];
      CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 1);
      CHECK(wcscmp(b[0].img, a.img) == 0 && Near(b[0].x, 0.25f) && Near(b[0].scale, 0.5f) && b[0].blend == 2 && b[0].colorkey == 0x102030);
  }
  TEST(TestSpritePreset_HebrewPathRoundTrip) {
      SpriteDesc a = {}; wcscpy_s(a.img, L"C:\\\u05EA\u05DE\u05D5\u05E0\u05D5\u05EA\\\u05E4\u05E8\u05D7.png");
      a.scale = 1; a.blend = 5;
      SpriteDesc b[1];
      CHECK(ReadPresetSprites(MapLookup(ParseLines(WritePresetSprites(&a, 1))), b, 1) == 1);
      CHECK(wcscmp(b[0].img, a.img) == 0);
  }
  TEST(TestSpritePreset_MissingImgSkipped) {
      std::map<std::string, std::string> m = { {"suri_sprite_count","2"}, {"suri_sprite0_x","0.5"},
                                               {"suri_sprite1_img","a.png"} };
      SpriteDesc b[kMaxPresetSprites];
      CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 1);
      CHECK(wcscmp(b[0].img, L"a.png") == 0);
      CHECK(Near(b[0].x, 0.5f) && Near(b[0].scale, 1.0f) && b[0].blend == 5);  // defaults
  }
  TEST(TestSpritePreset_ClampsAndLimits) {
      std::map<std::string, std::string> m = { {"suri_sprite_count","99"} };
      for (int i = 0; i < 99; i++) m["suri_sprite" + std::to_string(i) + "_img"] = "a.png";
      m["suri_sprite0_scale"] = "500"; m["suri_sprite0_blend"] = "9"; m["suri_sprite0_x"] = "-7";
      SpriteDesc b[kMaxPresetSprites];
      CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == kMaxPresetSprites);
      CHECK(Near(b[0].scale, 10.0f) && b[0].blend == 5 && Near(b[0].x, -1.0f));
  }
  TEST(TestSpritePreset_NoKeysNoSprites) {
      std::map<std::string, std::string> m;
      SpriteDesc b[kMaxPresetSprites];
      CHECK(ReadPresetSprites(MapLookup(m), b, kMaxPresetSprites) == 0);
      CHECK(WritePresetSprites(nullptr, 0) == "");   // old-style preset stays byte-identical
  }
  TEST(TestSpriteInitCode_Format) {
      SpriteDesc d = {}; d.x = 0.5f; d.y = 0.25f; d.scale = 2; d.rot = 0; d.blend = 5;
      CHECK(SpriteInitCode(d) == "x=0.5000;y=0.2500;sx=2.0000;sy=2.0000;rot=0.0000;blendmode=5;");
  }
  TEST(TestSpriteInitCode_IgnoresLocale) {
      setlocale(LC_ALL, "de-DE");
      SpriteDesc d = {}; d.x = 0.5f; d.scale = 1; d.blend = 0;
      std::string s = SpriteInitCode(d);
      setlocale(LC_ALL, "C");
      CHECK(s.find("x=0.5000;") != std::string::npos);
  }
  TEST(TestSpritePreset_ReadIgnoresLocale) {
      setlocale(LC_ALL, "de-DE");
      std::map<std::string, std::string> m = { {"suri_sprite_count","1"}, {"suri_sprite0_img","a.png"}, {"suri_sprite0_x","0.75"} };
      SpriteDesc b[1];
      int n = ReadPresetSprites(MapLookup(m), b, 1);
      std::string w = WritePresetSprites(b, 1);
      setlocale(LC_ALL, "C");
      CHECK(n == 1 && Near(b[0].x, 0.75f));
      CHECK(w.find("suri_sprite0_x=0.7500") != std::string::npos);
  }
  TEST(TestSpritePreset_RelativePaths) {
      std::wstring base = L"C:\\App\\plugins\\Milkdrop2\\";
      CHECK(MakeSpritePathRelative(base, L"c:\\app\\PLUGINS\\Milkdrop2\\textures\\a.png") == L"textures\\a.png");
      CHECK(MakeSpritePathRelative(base, L"D:\\pics\\a.png") == L"D:\\pics\\a.png");
      CHECK(ResolveSpritePath(base, L"textures\\a.png") == base + L"textures\\a.png");
      CHECK(ResolveSpritePath(base, L"D:\\pics\\a.png") == L"D:\\pics\\a.png");
      CHECK(ResolveSpritePath(base, L"\\\\server\\share\\a.png") == L"\\\\server\\share\\a.png");
  }
  ```
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement. Numbers: `_create_locale(LC_NUMERIC, "C")` once (static), `_snprintf_s_l` for writing and `_strtof_l` for reading. Paths: `WideCharToMultiByte(CP_UTF8, …)` and `MultiByteToWideChar(CP_UTF8, …)`. A path that doesn't fit in `MAX_PATH` is skipped on reading.
- [x] **Step 4:** `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(sprites): sprite entries in presets`.

### Task 6: Drawing, transparency, GIF and static sprites

**Files:**
- Create: `code/vis_milk2/features/image_load_win32.h/.cpp`
- Modify: `texmgr.h/.cpp`, `milkdropfs.cpp` (~2938–3150), `plugin.h/.cpp`, `plugin.vcxproj` (`gdiplus.lib`), `plugin.rc`, `resource.h`

**Interfaces:**
- Consumes: `SpriteCorners`, `ImageKindFromPath` (Task 4); `SpriteDesc`, `SpriteInitCode`, `ResolveSpritePath`, `kSpriteBlendImageAlpha` (Task 5).
- Produces:
  ```cpp
  // td_tex (texmgr.h), reset in KillTex and after LoadTex (also after the instancing memcpy):
  bool  bOwnedByPreset;   // false
  float userDx, userDy;   // 0, clip-space offsets added after the EEL code
  float userScale;        // 1, multiplies sx and sy
  int   userBlend;        // -1 = use var_blendmode
  // image_load_win32.h
  HRESULT LoadGifAsPngInMemory(const wchar_t* path, std::vector<unsigned char>* png); // first frame, keeps transparency
  bool GdiplusStartupOnce(); void GdiplusShutdownOnce();
  // CPlugin
  int  FindSpriteSlot();                                   // the slot search from LaunchSprite: free slot or the oldest (killed)
  bool LaunchStaticSprite(const SpriteDesc& d, bool ownedByPreset); // returns false + AddError on failure
  int  m_nSelectedSprite;                                  // -1; set by LaunchStaticSprite/LaunchSprite and by clicks
  ```

- [x] **Step 1:** `texmgr::LoadTex`: when `ImageKindFromPath(szFilename) == Gif`, call `LoadGifAsPngInMemory` and then `D3DXCreateTextureFromFileInMemoryEx` with **the same** parameters (pool, colorkey, mip) as the existing call. That way the colorkey and the device-reset handling stay the same. `Unsupported` returns `TEXMGR_ERR_FORMAT`. GDI+ is initialized in `AllocateMyNonDx9Stuff` and shut down in `CleanUpMyNonDx9Stuff`.
- [x] **Step 2:** `milkdropfs.cpp`: after clamping x, y, sx and sy: `x += userDx; y += userDy; sx *= userScale; sy *= userScale;`. `blendmode`: if `userBlend >= 0`, use it, clamped with `min(5, …)`. Replace the corner calculation with a call to `SpriteCorners` (the burn-in step stays after it as is), and store the corners in `CPlugin`'s `float m_spriteQuad[NUM_TEX][4][2]` for the hit test. Mode 5 in `switch(blendmode)`: `SRCBLEND=SRCALPHA`, `DESTBLEND=INVSRCALPHA`, `ALPHAOP=MODULATE` with `ALPHAARG1=TEXTURE` and `ALPHAARG2=DIFFUSE`. That way the file's transparency is multiplied by the code's `a`.
- [x] **Step 3:** `CPlugin::FindSpriteSlot` extracts the slot search from `LaunchSprite` (no behavior change), and `LaunchSprite` calls it. `LaunchStaticSprite`: `ResolveSpritePath(m_szMilkdrop2Path, d.img)`, init code from `SpriteInitCode(d)`, empty per-frame code, colorkey `d.colorkey`, `nUserData = -1`, and `bOwnedByPreset`. On error: `AddError` with `IDS_SPRITE_FILE_ERROR` (803) "Sprite image not found or unreadable: %s" (the file name only, without the path), for 6 seconds.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: no errors, `0 failed`. Smoke test: K then 00 with the Task 3 template looks exactly as before the change (same place and same size).
- [x] **Step 5:** commit `feat(sprites): image transparency blend mode, GIF loading, static sprites`.

### Task 7: Mouse, and the Sprites and Messages menus

**Files:**
- Modify: `features/commands.h/.cpp`, `features/context_menu.cpp`, `plugin.h/.cpp`, `plugin.rc`, `resource.h`, `plugin.vcxproj` (`comdlg32.lib`), `code/tests/test_commands.cpp`

**Interfaces:**
- Consumes: `TopmostSpriteAt`, `WindowToClip`, `ClipDeltaToSpriteDelta`, `WheelScale` (Task 4); `LaunchStaticSprite`, `m_nSelectedSprite`, `m_spriteQuad` (Task 6); `MessagesIniTemplate`, `SpritesIniTemplate`, `WriteUtf16IniIfMissing` (Task 3).
- Produces: new Cmds after `FxClearAll`, in this order:
  `SpriteLoadImage, SpriteLaunchPrompt, SpriteRemoveSelected, SpriteRemoveAll, SpriteBlendImage, SpriteBlendAlpha, SpriteBlendDecal, SpriteBlendAdditive, SpriteBlendColorMask, SpriteBlendColorKey, SpriteEditList, MessageRandom, MessagePrompt, MessageEditList`.
  `AppSnapshot`: `int spriteCount = 0; int selectedSprite = -1; int selectedSpriteBlend = -1;`.
  A pure function: `int SpriteBlendOf(Cmd c);` returns 5, 0, 1, 2, 3, 4 in command order, or -1.

- [x] **Step 1: Failing tests** in `test_commands.cpp`:
  ```cpp
  TEST(TestCommands_SpriteSelectionGates) {
      AppSnapshot s;
      CHECK(QueryState(Cmd::SpriteLoadImage, s).enabled);
      CHECK(!QueryState(Cmd::SpriteRemoveSelected, s).enabled);
      CHECK(!QueryState(Cmd::SpriteRemoveAll, s).enabled);
      CHECK(!QueryState(Cmd::SpriteBlendAlpha, s).enabled);
      s.spriteCount = 1; s.selectedSprite = 3; s.selectedSpriteBlend = 2;
      CHECK(QueryState(Cmd::SpriteRemoveSelected, s).enabled);
      CHECK(QueryState(Cmd::SpriteRemoveAll, s).enabled);
      CHECK(QueryState(Cmd::SpriteBlendAdditive, s).checked);
      CHECK(!QueryState(Cmd::SpriteBlendImage, s).checked);
  }
  TEST(TestCommands_FileDialogsNotInFullscreen) {
      AppSnapshot s; s.fullscreen = true;
      CHECK(!QueryState(Cmd::SpriteLoadImage, s).enabled);
      CHECK(!QueryState(Cmd::SpriteEditList, s).enabled);
      CHECK(!QueryState(Cmd::MessageEditList, s).enabled);
  }
  TEST(TestCommands_PromptsBlockedWhileTyping) {
      AppSnapshot s; s.textEditActive = true;
      CHECK(!QueryState(Cmd::SpriteLaunchPrompt, s).enabled);
      CHECK(!QueryState(Cmd::MessagePrompt, s).enabled);
  }
  TEST(TestCommands_SpriteBlendOf) {
      CHECK(SpriteBlendOf(Cmd::SpriteBlendImage) == 5);
      CHECK(SpriteBlendOf(Cmd::SpriteBlendAlpha) == 0);
      CHECK(SpriteBlendOf(Cmd::SpriteBlendColorKey) == 4);
      CHECK(SpriteBlendOf(Cmd::AppQuit) == -1);
  }
  ```
  The menu: two new submenus after Effects. **Sprites**: Load image…, Launch from list (K)…, Remove selected, Remove all, a separator, a **Blend** submenu (the 6 items), a separator, Edit sprite list…. **Messages**: Random message, Show message (Y)…, Edit messages…. The labels are `IDS_*` from 804 onward. `SpriteLaunchPrompt` and `MessagePrompt` are the commands for the `k` and `y` keys (KeyScope::Char), and Shift+K stays in the existing code. Since they become commands with a key, they must also appear in the phase 6 help text (`TestHelpText_EveryKeyListed`).
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement `QueryState` and `Dispatch`:
  - `SpriteLoadImage`: `GetOpenFileNameW` with the filter `Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tga;*.dds)`, and the starting folder `<Milkdrop2>\textures`. The created `SpriteDesc`: `MakeSpritePathRelative`, x=y=0.5, scale=0.5, rot=0, blend=5, colorkey=0, then `LaunchStaticSprite(d, false)`.
  - `SpriteLaunchPrompt` and `MessagePrompt`: the same code as `k` and `y` today (`m_nNumericInputMode` …). Move the code from `HandleRegularKey` into `Dispatch`.
  - `SpriteRemoveSelected`: `KillSprite(m_nSelectedSprite)` then `-1`. `SpriteRemoveAll`: all slots.
  - `SpriteBlend*`: `m_texmgr.m_tex[sel].userBlend = SpriteBlendOf(c)`.
  - `SpriteEditList` and `MessageEditList`: `WriteUtf16IniIfMissing(path, template)`, then `ShellExecuteW(NULL, L"open", L"notepad.exe", path, …)`.
  - `MessageRandom`: first `ReadCustomMessages()` (to pick up edits), then `LaunchCustomMessage(-1)`. If there are no messages at all: `AddError(IDS_NO_MESSAGES …)` "No messages yet – use Messages > Edit messages", for 3 seconds. `MessagePrompt` also calls `ReadCustomMessages()` before it starts.
  - In the snapshot: `spriteCount` (the number of slots with a `pSurface`), `selectedSprite`, and `selectedSpriteBlend` (`userBlend` if it is `>= 0`, otherwise `(int)*var_blendmode`).
- [x] **Step 4: Mouse** in `CPlugin::MyWindowProc`. First make sure `WM_LBUTTONDOWN` gets there: in `StaticWndProc` the `default` passes to `PluginShellWindowProc`. If not, add a case that passes it on.
  - `WM_LBUTTONDOWN`: `WindowToClip` with `GetClientRect`, then `TopmostSpriteAt` over `m_spriteQuad` and the live slots. On a hit: `m_nSelectedSprite = i`, `SetCapture`, and save the start point. Otherwise: the existing behavior.
  - `WM_MOUSEMOVE` while dragging: `ClipDeltaToSpriteDelta` of the difference, added to `userDx` and `userDy`, and update the start point.
  - `WM_LBUTTONUP`: `ReleaseCapture`. `WM_CAPTURECHANGED` also ends the drag.
  - `WM_MOUSEWHEEL`: `ScreenToClient` on the point from `lParam`. If the cursor is over a sprite: `userScale = WheelScale(userScale, GET_WHEEL_DELTA_WPARAM(wParam))` and it becomes selected. Otherwise the message continues on.
  - A selected sprite that is deleted (`KillTex`) resets `m_nSelectedSprite` to -1.
- [x] **Step 5:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`, including `TestMenu_EveryCommandExactlyOnce` and `TestMenu_LabelsExist`.
- [x] **Step 6:** commit `feat(sprites): mouse move/resize and sprites and messages menus`.

### Task 8: Saving and loading with the preset, documentation and manual test

**Files:**
- Modify: `state.h/.cpp`, `plugin.cpp` (`LoadPreset` ~6473, `SavePresetAs` ~7458), `code/resources/Milkdrop2/docs/user-guide.html`, `docs/PROJECT_STATUS.md`, `docs/superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md`

**Interfaces:**
- Consumes: `SpriteDesc`, `WritePresetSprites`, `ReadPresetSprites`, `MakeSpritePathRelative` (Task 5); `LaunchStaticSprite`, the `td_tex` fields (Task 6).
- Produces: `CState::m_nSprites`, `CState::m_sprites[kMaxPresetSprites]`; `int CPlugin::CollectLiveSprites(SpriteDesc* out, int maxOut)`.

- [x] **Step 1:** `CState`: the new fields are reset in `Default()` when `STATE_GENERAL` is in the flags. In `Import`, inside the block that checks `STATE_GENERAL && STATE_MOTION && STATE_WAVE` (so a mash-up doesn't change sprites), call `ReadPresetSprites` with a lookup that wraps `GetFastString(key, "~!@#$", buf, …, f)` and returns false when the sentinel is returned. In `Export`, right after the general values in `[preset00]` and before the waves code, write `WritePresetSprites(m_sprites, m_nSprites)` as is. With no sprites an empty string is written, so a preset without sprites is saved exactly as before.
- [x] **Step 2:** `CollectLiveSprites`: for each slot with a `pSurface`, in `nStartFrame` order, build a `SpriteDesc`: `img` from `MakeSpritePathRelative(m_szMilkdrop2Path, szFileName)`; `x = ((*var_x*2-1) + userDx + 1) / 2`, and `y` likewise; `scale = *var_sx * userScale`; `rot = *var_rot`; `blend` as in the snapshot; `colorkey` from loading (store it in `td_tex` as `unsigned colorkey` in `LoadTex`). `SavePresetAs`: before `Export`, `m_pState->m_nSprites = CollectLiveSprites(m_pState->m_sprites, kMaxPresetSprites)`.
- [x] **Step 3:** `LoadPreset`: after the `Import` of the new state (also in the blend path), delete every slot with `bOwnedByPreset`, then call `LaunchStaticSprite(m_sprites[i], true)` for each sprite of the new state. A missing file shows the error from Task 6, and the preset continues to load. Nothing changes in `LoadPresetTick`.
- [x] **Step 4:** `user-guide.html`: a Sprites chapter (loading, dragging, wheel, Blend, saving in a preset, `milk2_img.ini` and K) and a Messages chapter (`milk2_msg.ini` in UTF-16, Y, Random). `PROJECT_STATUS.md`: a row in the work table. In the roadmap: the status of phases 8 and 9 and a link to this plan.
- [x] **Step 5:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 6: Manual test (user):**
  - A PNG with a transparent background loads from the menu and looks transparent ✔; transparent GIF ✔; JPG ✔.
  - Dragging moves the sprite exactly with the cursor, in both a wide and a narrow window ✔; the wheel resizes only over a sprite ✔.
  - All 6 blend modes from the menu, and the check mark shows the current mode ✔.
  - Save As with two sprites, load another preset (both sprites disappear), and go back to the saved preset (they return to the same place and size) ✔.
  - Opening the saved `.milk` in an editor: there are `suri_sprite*` keys, and no other keys changed ✔.
  - Deleting the image file and loading the preset: one error message and the preset loads ✔; an image in a folder with a Hebrew name ✔ (Review Focus 4).
  - Messages > Random message with no file: a "No messages yet" message, no crash ✔ (Review Focus 3). Edit messages creates the file and opens Notepad, and an added Hebrew message is shown correctly ✔.
  - An old preset without sprites, and K then 00 from `milk2_img.ini`: behave as before the phase ✔.
- [x] **Step 7:** commit `feat(sprites): save sprites with the preset`. **Stop for phase 9 approval.**
