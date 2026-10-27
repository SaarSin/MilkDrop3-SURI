# MilkDrop3 SURI – Improvements and additions plan (Roadmap)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** upgrade the open build (version **3.0.S**, based on upstream 3.0.A from https://github.com/milkdrop2077/MilkDrop3, commit `6b39088`) with new features: stable audio, a menu, effects, Desktop mode, song info and more.

**Architecture:** each feature has its own module (a separate `.cpp/.h` file under `code/vis_milk2/features/` or `code/audio/`). The module hooks into `CPlugin` and the WndProc through small connection points. Pure logic (no D3D and no window) is tested in a separate console test project. The user tests the graphical and windowing parts by running the program.

**Tech Stack:** C++17, MSVC v143 (VS2022), Win32, Direct3D 9 with D3DX9 (NuGet), WASAPI. In phase 8: C++/WinRT.

**Spec:** [docs/PROJECT_STATUS.md](../../PROJECT_STATUS.md) (the current state).

## Global Constraints

- **Approval gate per phase:** no code is written in a phase before the user explicitly approves it. At the start of each phase that does not yet have step-level detail, first write a detailed plan (a separate file in `docs/superpowers/plans/`) and wait for its approval.
- **Webcam: out of scope.** Not implemented, no menu item, and Media Foundation is not linked.
- **Win32 (x86)** architecture only (the move to x64 was removed from the plan). Toolset v143.
- **No new external dependencies without approval.**
- Each phase ends when `.\build.ps1` passes with no errors, `.\build.ps1 -Test` passes (once there are tests), and the user has tested the feature by running it and approved.
- Logging through the existing `LOG`/`ERR` (`code/audio/log.h`) to the `MilkDrop3.log` file.
- Commit at the end of each phase, after approval.

## Review Focus

1. **The audio device is disconnected or switched mid-run** (USB or Bluetooth headset, default change): capture is expected to resume within less than 3 seconds, with no crash and no exit. Tested in phase 2 (`TestAudioRecovery_*`).
2. **An effect on a preset without `shader_body`, on a preset with PS < 2, an effect toggled twice, and saving a preset while an effect is active:** expected no crash, a reversible toggle, and the preset saved **without** the effects. Tested in phase 5 (`TestShaderFx_*`).
3. **Exiting the program or an Explorer crash during Desktop mode:** the wallpaper, the tray icon and the window are expected to be restored, and Desktop mode to resume after the WorkerW is recreated. Tested in phase 7.
4. **Song info when no player is active, and a title in Hebrew or with emoji:** expected to show nothing, and to display the text correctly (UTF-16). Tested in phase 8 (`TestSongInfo_*`).
5. **The right-click menu in fullscreen (D3D exclusive):** a Win32 menu is not visible in this mode. The program is expected to switch to a screen-sized window (fake fullscreen) before the menu opens, or the menu simply does not open. Tested in phase 4.

---

## Phase -1: Clean up old and irrelevant code (before git)

**User's decision (2026-10-24):** no git repository until the old code is cleaned up. git init and the first commit (Step 1 of phase 0) happen only after this phase.

**Cleanup candidates** (list from an initial survey; each item is checked for use before deletion, and each item needs the user's approval):
1. **Winamp's old Desktop mode** (`vms_desktop.dll`, the icon drawing, its help strings): about 73 references. Phase 7 replaces it.
2. **The Wasabi/Winamp language layer** (`wasabi.cpp`, `wasabiApi*`, about 261 references): replaced with direct calls to `LoadStringW` and `CreateDialogParamW`.
3. **Winamp player control** (IPC messages to the Winamp window).
4. **WAV file writing in `loopback-capture.cpp`** (`WriteWaveHeader`/`FinishWaveFile`/`hFile`): a leftover from Microsoft's sample code.
5. **History comments and old TODO lists** at the top of `plugin.cpp` and `pluginshell.cpp`, and an `#if 0` block in `milkdropfs.cpp`.
6. **Strings in `plugin.rc` for dialogs that don't exist** (e.g. `IDS_DM_MORE_HELP`).
7. **The `_archive/` folder** (3.4MB: old project files, the Linux version, prebuilt binaries).

**Check after each item:** `.\build.ps1` passes with no errors and no new warnings, and the program starts and reacts to sound (the user approves).

**Status (2026-10-24):** all 7 items done. A clean Rebuild passes with the 2 pre-existing warnings (C4477). A run with a 9-second test tone captured 432,960 frames and exited with code 0. Remaining: the user's visual check, then git (phase 0).
Decisions made during the phase:
- `SetAudioBuf` detects int16 or float by `wBitsPerSample`, not by a flag.
- The Wasabi layer was replaced by a single helper, `LoadResString`, rather than direct calls in 254 places.
- Unused functions in `utility.cpp` unrelated to Desktop mode were removed as well: Pentium timers, ComboBox helpers, DownloadDirectX and myOpenURL.

## Phase 0: Infrastructure (git and a test project). Starts only after phase -1

**Files:**
- Create: `.gitignore` (`build/`, `third_party/d3dx/`, `dist/`, `*.log`)
- Create: `code/tests/unit_tests.vcxproj`, `code/tests/test_main.cpp`, `code/tests/mini_test.h`
- Modify: `code/MilkDrop3.sln` (add the project), `build.ps1` (add a `-Test` switch)

**Interfaces:**
- Produces: `mini_test.h` with `TEST(name)` and `CHECK(expr)` (static registration, prints `PASS`/`FAIL name:line`, and returns an exit code equal to the number of failures). `build.ps1 -Test` builds `unit_tests.exe` and runs it.

- [x] **Step 1:** `git init`, then `.gitignore` and a first commit of the current state (`chore: baseline`).
- [x] **Step 2:** write a deliberately failing test, `TEST(Sanity){ CHECK(1+1==3); }`. Run `.\build.ps1 -Test`. Expected: `FAIL Sanity` and exit code 1.
- [x] **Step 3:** fix it to `==2` and run again. Expected: `PASS Sanity` and exit code 0.
- [x] **Step 4:** Commit `chore: add unit test project`.

## Phase 2: Audio stability (recovery from disconnection or device switch, and an error message)

**Files:**
- Create: `code/audio/device-watch.h`, `code/audio/device-watch.cpp`
- Modify: `code/audio/loopback-capture.cpp` (the outer loop restarts capture), `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` (MessageBox instead of a silent exit)
- Test: `code/tests/test_device_watch.cpp`

**Interfaces:**
- Produces (pure, unit-tested):
  `enum class AudioAction { None, Restart, SwitchToDefault };`
  `AudioAction DecideAudioAction(const AudioSnapshot& cur, const AudioSnapshot& now, HRESULT lastHr);`
  where `struct AudioSnapshot { std::wstring deviceId; DWORD state; WORD formatTag; DWORD sampleRate; bool followDefault; };`
- Produces (Win32): `class CDeviceWatch : public IMMNotificationClient`, which signals `HANDLE hChangedEvent` when the default device or the device state changes.

- [x] **Step 1: failing tests**: `TestAudioRecovery_DefaultChanged` (followDefault and a different deviceId, expect SwitchToDefault), `TestAudioRecovery_DeviceInvalidated` (`lastHr==AUDCLNT_E_DEVICE_INVALIDATED`, expect SwitchToDefault), `TestAudioRecovery_FormatChanged` (different sampleRate, expect Restart), `TestAudioRecovery_Unplugged` (state != `DEVICE_STATE_ACTIVE`, expect SwitchToDefault), `TestAudioRecovery_Steady` (everything equal, expect None), and `TestAudioRecovery_PinnedDevice` (followDefault=false, the default changed but the device is still active, expect None).
- [x] **Step 2:** run `.\build.ps1 -Test`. Expected: FAIL, because the function does not exist.
- [x] **Step 3:** implement `DecideAudioAction` in `device-watch.cpp`.
- [x] **Step 4:** run again. Expected: PASS.
- [x] **Step 5:** wire everything up. `LoopbackCaptureThreadFunction` wraps `LoopbackCapture` in a loop: on `Restart` or `SwitchToDefault`, capture starts again (up to 5 attempts, 500ms apart), and every decision is logged. If capture fails completely, a MessageBox with a clear message is shown, and the visualization keeps running without audio.
- [x] **Step 6: manual test (user)**: play music. Unplug the headset or switch the default device in Windows settings. The graphics are expected to react again within less than 3 seconds, and the log to show `Switched to default device`.
- [x] **Step 7:** Commit `feat(audio): recover from device change`.

**Status (2026-10-24):** Steps 1–5 and 7 done, and 8 `TestAudioRecovery_*` tests pass. Decisions: (1) added a test `TestAudioRecovery_OtherFailure`: any other failure on a device that is still active leads to Restart. (2) The error message is shown once per run, on a separate thread so capture keeps responding to shutdown. After 5 failures capture waits for a device change and then tries again. (3) Fixed an existing bug: closing the window while the preset scan was running hung the process (`TerminateThread` immediately, and `DeleteCriticalSection` before the thread had finished). The manual test (Step 6) passed (2026-10-25).

---

> **The following phases are described at the "what and how" level only.** When the user approves a phase, a detailed step-level plan (tests, signatures and commands) is written for it as in phases 0–2, and its approval is awaited.

## Phase 3: Command layer

**Status (2026-10-25):** phases 3 and 4 were done together, according to [2026-10-24-phase-3-4-commands-and-context-menu.md](2026-10-24-phase-3-4-commands-and-context-menu.md). All tests pass. The user's manual test passed (2026-10-25). The Audio menu was postponed, because phase 2 does not expose an API for listing or choosing devices.

- **What:** one central command table: `enum class Cmd`, `bool Dispatch(Cmd)` and `CmdState QueryState(Cmd)` (check mark and enabling or disabling an item). The keyboard, the menu and later the tray icon all call the same functions. This avoids duplication with the huge switch in `plugin.cpp`.
- **Files:** `code/vis_milk2/features/commands.h/.cpp`. Hooked in gradually from `CPlugin::HandleRegularKey`.
- **Tests:** `TestCommands_*` for the key→command mapping table, i.e. no two keys accidentally bound to the same command.

## Phase 4: Right-click menu

- **What:** `WM_CONTEXTMENU` in `StaticWndProc` builds a Win32 menu from the phase 3 table. Our own structure, arranged for convenience:
  - Preset: next, previous, lock, random, load, save, delete
  - Effects (phase 5)
  - Audio: automatic and a device list (from phase 2)
  - View: fullscreen, always on top, Desktop (phase 7), song info (phase 8)
  - Help and About
- **Review Focus 5:** in fullscreen, switch to fake fullscreen before the menu opens.
- **Test:** a manual test in a window and in fullscreen, and a unit test that every `Cmd` in the menu exists in the table.

## Phase 5: Real-time effects (Geometry and Colors)

**Status (2026-10-25):** done according to [2026-10-25-phase-5-7-effects-screens-desktop.md](2026-10-25-phase-5-7-effects-screens-desktop.md). Effects are injected only into the comp shader (11 effects, plus Brightness and Contrast), persist across presets and are stripped before saving. The user's manual test passed (2026-10-25).

- **What:** `ShaderFx::Toggle(std::string& shaderText, FxId id) -> bool` adds or removes a block marked `// SURI-FX:<name> begin/end` right after `shader_body {`. `ShaderFx::StripAll(std::string&)` runs before a preset is saved. If the PS version is below 2, the shaders are upgraded before injection. After every change the shaders are recompiled (`CPlugin::RecompilePShader`).
- **The HLSL is written from scratch.** Effects: Mirror X/Y, Kaleido, Wave distort, Zoom-in, Swap axes, Polar, Invert, Mono, Posterize, Hue shift, Brightness ±, Contrast ±. The final list is approved in the detailed plan.
- **Files:** `code/vis_milk2/features/shader_fx.h/.cpp`, `code/tests/test_shader_fx.cpp`.
- **Tests (Review Focus 2):** `TestShaderFx_ToggleTwiceRestores`, `TestShaderFx_NoShaderBody_ReturnsFalse`, `TestShaderFx_TwoEffectsIndependent`, `TestShaderFx_StripAllBeforeSave`, `TestShaderFx_BrightnessClamp` (0.2–3.0).

## Phase 6: Friendlier screens

**Status (2026-10-25):** done according to [2026-10-25-phase-5-7-effects-screens-desktop.md](2026-10-25-phase-5-7-effects-screens-desktop.md). The user chose direction B (cards): an info card, toast, paged help, M and L in cards, DPI-based fonts and a monospace font in the code editor. The manual test passed (2026-10-25).

- **What:** improve the screens drawn over the visualization: the help screen (F1), the preset edit menu (M), the info messages (preset name, FPS, rating), and the error messages.
- **How:**
  1. **Survey:** screenshots of the current state and a list of problems (readability, density, contrast, font size at high DPI, undocumented keys).
  2. **Mockups:** 2–3 design directions as an HTML page for comparison. **The user picks a direction.**
  3. **Implementation:**
     - A semi-transparent background behind the text (`textmgr.cpp`).
     - A font that scales with DPI.
     - Help split into pages by topic.
     - Toast messages instead of fixed text.
     - A link to the help from the right-click menu.
- **Files:** `menu.cpp`, `textmgr.cpp`, `plugin.cpp` (drawing the help screen), and `plugin.rc` (the strings).
- **Test:** the user approves by comparing before and after screenshots, at 1080p and 4K.

## Phase 7: Desktop mode (animated background) and a tray icon

**Status (2026-10-25):** done according to [2026-10-25-phase-5-7-effects-screens-desktop.md](2026-10-25-phase-5-7-effects-screens-desktop.md). The shortcut is Ctrl+Alt+Shift+D, and the mode can also be entered from the View menu. The manual test showed that when Explorer is killed the window disappears with it, so the program restarts itself (`/desktop-resume`) and returns to the background. A single-instance mechanism (mutex) was added. All manual tests passed (2026-10-25).

- **What:**
  - Progman receives `0x052C`, the WorkerW is found with `EnumWindows`, and `SetParent` attaches the window to it. The window spans the virtual screen.
  - A tray icon (`Shell_NotifyIconW`) with a short menu.
  - A global `RegisterHotKey` shortcut. The key combination is set in the detailed plan.
- **Review Focus 3:**
  - Full restore in `WM_DESTROY`.
  - When Explorer crashes, the `TaskbarCreated` message (`RegisterWindowMessage`) re-creates the tray icon and re-attaches to the WorkerW.
- **Files:** `code/vis_milk2/features/desktop_mode.h/.cpp`.
- **Test:** manual: enter and exit, `taskkill explorer` during Desktop mode, and multiple monitors.

## Phase 8: Now Playing song info

**Status (2026-10-27):** done and manually tested. Fixed after the test (the title stays 7 seconds, then fades out over 3 seconds; long titles shrink to fit). The retest passed.

- **What:** `GlobalSystemMediaTransportControlsSessionManager` through C++/WinRT. A separate thread checks every 2 seconds and updates `struct SongInfo { std::wstring title, artist; }`. When the song changes, the details are shown as an on-screen message.
- **Requires:** `/std:c++17` (C++/WinRT comes with the Windows SDK, so no new dependency) and Windows 10 1809 or later. If the API is not available, the feature is silently disabled.
- **Tests (Review Focus 4):** `TestSongInfo_FormatEmpty` (no session, so an empty string and no message), `TestSongInfo_HebrewTitle` (UTF-16 integrity), and `TestSongInfo_ChangeDetection`.

## Phase 9: Sprites and messages

- **First check what exists.** Our code already has MilkDrop 2 support for `milk_img.ini` and `milk_msg.ini` (`ReadCustomMessages`). Verify that it works in the current build.
- **Additions:**
  - Loading PNG and GIF (including transparency) through GDI+ or D3DX.
  - Moving a sprite with the mouse and resizing it with the wheel.
  - Blend modes.
  - Saving a sprite inside the `.milk`, in a format we define.

**Status (2026-10-25):** done according to [2026-10-25-phase-8-9-now-playing-sprites.md](2026-10-25-phase-8-9-now-playing-sprites.md). K and Y work, Sprites and Messages menus, PNG/GIF with transparency, drag and wheel, 6 blend modes, and sprites are saved in the `.milk` as `suri_sprite*` keys. The manual test passed.

## Phase 10: Dragging files onto the window (Drag & Drop)

- **Replaced by a design document:** [2026-10-25-phase-10-drag-drop-playlist-design.md](../specs/2026-10-25-phase-10-drag-drop-playlist-design.md) (approved 2026-10-25).
- **In short:** drag a folder or `.milk` onto the window, detect existing or new (name and size), a card for choosing new files (including copying their textures), a session playlist, and an on-screen Playlist screen with save and load. No image dragging: images are loaded from the settings screens.

## Phase 13: Packaging

- **What:** `-Package` builds from the source files and gains a `-PresetSet Curated` option (about 500 selected presets) for a smaller package.

**Status (2026-10-27):** done according to [2026-10-27-phase-13-packaging.md](2026-10-27-phase-13-packaging.md). Packages are built from the source files in a staging folder. The manual test passed.

---

## Suggested order and approvals

| Phase | Depends on | Size | Status |
|---|---|---|---|
| 0 Infrastructure | – | Small | Done |
| 2 Audio | 0 | Medium | Done and manually tested |
| 3 Commands | 0 | Medium | Done and manually tested |
| 4 Menu | 3 | Medium | Done and manually tested (without Audio) |
| 5 Effects | 0 (4 recommended too) | Medium | Done (manually tested) |
| 6 Friendlier screens | 4 | Medium–large | Done (manually tested) |
| 7 Desktop | 3 | Medium | Done (manually tested) |
| 8 Song info | 0 | Small–medium | Done and manually tested. Fixed after the test (7 seconds, then a 3-second fade; long titles fit); retested ([plan](2026-10-25-phase-8-9-now-playing-sprites.md)) |
| 9 Sprites | 0 | Large | Done and manually tested ([plan](2026-10-25-phase-8-9-now-playing-sprites.md)) |
| 10 Drag & Drop and Playlist | 6 | Medium–large | Done and manually tested ([plan](2026-10-25-phase-10-drag-drop-playlist.md)) |
| ~~11 Spout~~ | – | – | **Removed from the plan by the user's decision (2026-10-27):** no need to send the image to OBS or Resolume. OBS Window Capture can be used instead |
| ~~12 x64~~ | – | – | **Removed from the plan by the user's decision (2026-10-27):** a 32-bit exe runs normally on 64-bit Windows, and no phase requires x64 |
| 13 Packaging | 0 | Small | Done and manually tested ([plan](2026-10-27-phase-13-packaging.md)) |
| ~~Webcam~~ | – | – | **Not in the plan, by the user's decision** |
