# MilkDrop3 (SURI build) – Project status

**Goal:** a standalone Windows executable that shows MilkDrop visualizations
reacting to the sound playing through the speakers (WASAPI loopback).

**Base:** https://github.com/milkdrop2077/MilkDrop3 (commit `6b39088`, version 3.0.A). All changes since are listed in [CHANGES.md](../CHANGES.md).

Last updated: 2026-10-27

---

## How to build and run

Requirements: Visual Studio 2022 with "Desktop development with C++" (toolset v143, Windows 10/11 SDK).

```powershell
.\build.ps1              # Release build  ->  build\Release\MilkDrop3.exe
.\build.ps1 -Run         # build + run
.\build.ps1 -Config Debug
.\build.ps1 -Clean       # full rebuild
.\build.ps1 -Test        # build + run the unit tests
.\build.ps1 -Package     # dist\MilkDrop3-SURI-<version>-win32.zip (all presets, large)
.\build.ps1 -Package -PresetSet Curated   # dist\MilkDrop3-SURI-<version>-win32-curated.zip (only the presets in content\curated.txt)
pwsh tools\make-curated.ps1               # regenerates content\curated.txt (500 names, round-robin by author). The list is a starting point and can be edited by hand
```

`build.ps1` downloads D3DX9 (NuGet `Microsoft.DXSDK.D3DX`) into `third_party\d3dx` if it is missing.
The 2010 DirectX SDK is not needed.

**Folder layout produced by the build:**
```
build\Release\
  MilkDrop3.exe
  D3DX9_43.dll, D3DCompiler_43.dll
  MilkDrop3.log                ← run log (created on start)
  plugins\Milkdrop2\
    data\      base shaders (.fx)
    docs\
    presets\   9,795 presets ("Cream of the Crop")
    textures\
```
The program looks for its files in `<exe folder>\plugins\Milkdrop2\` (see `plugin.cpp`, `SUBDIR` in `defines.h`).
To move it to another computer, copy the whole `build\Release` folder, or use the zip from `-Package`.

---

## Code layout

| Path | Role |
|---|---|
| `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` | `WinMain`, window, D3D, fullscreen, render thread, starting audio capture |
| `code/vis_milk2/plugin.cpp` | Main logic: keyboard shortcuts, preset loading, transitions, mash-up |
| `code/vis_milk2/milkdropfs.cpp` | Rendering: warp/comp shaders, shapes, waves |
| `code/vis_milk2/pluginshell.cpp` | Shell: frame loop, window messages, FFT |
| `code/vis_milk2/state.cpp` | Reading/writing `.milk` / `.milk2` |
| `code/vis_milk2/menu.cpp`, `textmgr.cpp` | The on-screen menu (drawn with D3D) |
| `code/vis_milk2/plugin.rc` | Win32 dialogs (settings) |
| `code/vis_milk2/features/shader_fx.*` | Effects (phase 5): injecting and removing `// SURI-FX` blocks in the comp shader, and stripping them before saving |
| `code/vis_milk2/features/ui_scale.*` | DPI-based font sizes and the toast queue (phase 6) |
| `code/vis_milk2/features/help_text.*` | F1 help text, paged by topic, 5 topics (phase 6) |
| `code/vis_milk2/features/desktop_mode.*` | Desktop mode logic: WorkerW selection, reattach rules and tray menu layout (phase 7, testable without Win32) |
| `code/vis_milk2/features/desktop_mode_win32.cpp` | The Win32 part: `SetParent`, tray icon, `RegisterHotKey`, `TaskbarCreated` (phase 7) |
| `code/audio/` | WASAPI loopback capture (`loopback-capture.cpp`), device selection (`prefs.cpp`), buffer (`audiobuf.cpp`) |
| `code/ns-eel2/` | The preset expression compiler. It generates x86 code with inline asm, **so Win32 only** |
| `content/` | Presets and textures. Copied to the output after the build |
| `tools/` | Curated list generator and the packaging self-tests (`pwsh tools/test-tools.ps1`) |
| `third_party/d3dx/` | D3DX9 from NuGet |

---

## Findings

1. **x86 architecture only.** ns-eel2 compiles preset code to x86 machine code using MSVC inline asm,
   and MSVC does not support inline asm on x64. Moving to x64 would require replacing ns-eel2
   (for example with the newer EEL2 from WDL). This is not needed: a 32-bit exe runs normally on 64-bit Windows.
2. **VS2022 reserves the name `__floor`** (error C2169). It was renamed in `nseel-compiler.c`.
3. **Audio capture failed on multichannel devices.** The device was a ROG Theta 7.1 headset
   (8 channels, float, 48kHz). `IAudioClient::Initialize` in loopback mode returned
   `0x88890008 AUDCLNT_E_UNSUPPORTED_FORMAT` for the device's own mix format.
   The program exited immediately with code -520, with no window and no message.
4. **There was no log.** All LOG/ERR output went only to `OutputDebugString`.
5. **WinMain's return code is `-__LINE__`.** Exit code -520 points to line 520
   in `Milkdrop2PcmVisualizer.cpp`. This is useful for diagnosis.
6. Audio is captured from the playback device set **as default** (`eRender/eConsole`). To capture another device,
   change the default in Windows.
7. **The upstream `code/` folder was last updated on 2023-04-17.** Our build (originally "MilkDrop 3.0.A", and since 2026-10-24 **"MilkDrop 3.0.S"**)
   shows its own version and uses the `plugins\Milkdrop2` folder. Everything added since (the menu, effects, Desktop mode, sprites and more) is our own code.
   **GitHub search (2026-10-24):** out of 115 forks, most of the active ones did not change code. `vaerot` fixed the same `__floor` bug, and `shanevbg` added IPC.
   Related open-source projects:
   - **shanevbg/MDropDX12** (v3.2.0, September 2026): a DX12 rebuild of the engine. Includes a settings window, Audio Mixer,
     `.milk2` and a preset editor. Licensed **CC-BY-NC 4.0**, i.e. non-commercial use only.
   - **IkeC/Milkwave**: a MilkDrop2-based visualizer with a separate Remote app. Active.
8. Flag `0x1` (DATA_DISCONTINUITY) shows up in the log from time to time. This is normal during silence or when playback starts and stops.

---

## Work done

| # | Action | Files |
|---|---|---|
| 1 | Old files moved to `_archive/`: old project files (dsp/vcproj), the Linux version, non-Windows asm code, prebuilt binaries, README and images from upstream | `_archive/*` |
| 2 | Project file updated to v143, D3DX from NuGet, output to `build\<Config>`, and a post-build copy of DLLs, data, presets and textures to `plugins\Milkdrop2` | `code/vis_milk2/plugin.vcxproj`, `code/MilkDrop3.sln` |
| 3 | Added a command-line build script | `build.ps1` |
| 4 | Fixed error C2169 (`__floor`) | `code/ns-eel2/nseel-compiler.c` |
| 5 | **Log to file:** all LOG/ERR output is also written to `MilkDrop3.log` next to the exe | `code/audio/log.cpp` |
| 6 | **Audio capture fix:** the mix format is logged. If the device rejects it, capture retries as stereo float with `AUTOCONVERTPCM`, and Windows converts the sound | `code/audio/loopback-capture.cpp`, `code/audio/cleanup.h` (`ReleaseIfSetOnExit`) |
| 7 | Deleted the old `build\Release\Milkdrop2` folder (old layout, unused) | — |
| 8 | **Old code cleanup (2026-10-24, before git):** removed WAV writing, int-16 mode and command-line parsing from the audio code (`CPrefs` only opens the default device). Also removed leftovers of the musikcube plugin, Winamp's playlist and player commands (dead code), the settings and functions of the old Desktop mode (vms_desktop), and 13 unused functions in `utility.cpp`. The Wasabi layer was replaced by `LoadResString` (`resstrings.*`). Removed 202 unused strings and 77 unused IDs from `plugin.rc`/`resource.h`, history and TODO comments, and an `#if 0` block. The `_archive/` folder was deleted too. **The build passes (clean Rebuild) with the 2 warnings that existed before.** A backup of the previous state was saved as a zip | `code/audio/*`, `code/vis_milk2/*` |
| 9 | **Phases 3–4 (2026-10-25): command layer and right-click menu.** All preset and View keys (h, Backspace, Space, `, r, l, s, m, +, -, d, t, F1–F7, F9, Alt+Enter, Alt+Shift, Alt+F4) go through one command table. Right-click, Shift+F10 or the Apps key open a Win32 menu built from the same table, with check marks and greyed-out items. In fullscreen the cursor is shown while the menu is open, and rendering continues while the menu is open. Help opens the guides from `docs\`. The Audio menu is postponed until there is an API for choosing a device | `code/vis_milk2/features/*`, `plugin.cpp`, `Milkdrop2PcmVisualizer.cpp`, `code/tests/test_commands.cpp`, `test_context_menu.cpp` |
| 10 | **Phases 5–7 (2026-10-25): effects, friendlier screens and Desktop mode.** See the "Phases 5–7" section below | `code/vis_milk2/features/*`, `plugin.cpp`, `pluginshell.cpp`, `menu.cpp`, `Milkdrop2PcmVisualizer.cpp`, `plugin.rc`, `code/tests/*` |
| 11 | **Phases 8–9 (2026-10-25): song info, sprites and messages.** Phase 8: the song title and artist are read from the Windows media controls and shown when the song changes. Phase 9: loading PNG/GIF/JPG from the menu with transparency, moving with the mouse and resizing with the wheel, 6 blend modes, Sprites and Messages menus, `milk2_msg.ini` in UTF-16, and sprites saved inside the `.milk` (`suri_sprite*` keys) and restored when the preset loads. Also fixed a MilkDrop 2 bug that stretched sprites in non-4:3 windows. Manually tested | `code/vis_milk2/features/*`, `plugin.cpp`, `state.cpp`, `texmgr.cpp`, `milkdropfs.cpp`, `plugin.rc`, `code/tests/*`, `user-guide.html` |
| 12 | **Phase 10 (2026-10-26): Drag & Drop and Playlist.** Playlist screen (P key): two lists, Not playing and Playing, moving with the arrows, multi-select, type-to-search, and saving and loading `.m3u` in the `playlists\` folder. The playlist lasts until exit, and an empty list means "all". Dragging a folder or `.milk` files onto the window: existing presets (same name and same size) play immediately, and new ones are first shown in a confirmation window and copied without overwriting any file. Textures are copied only when the existing presets don't already find a texture with the same name. A single file loads immediately, and a group becomes the session playlist. Manually tested | `code/vis_milk2/features/*` (`playlist*`, `drop_import*`, `playlist_screen*`), `plugin.cpp`, `Milkdrop2PcmVisualizer.cpp`, `plugin.rc`, `code/tests/*`, `user-guide.html` |
| 13 | **Phase 8 fixes (2026-10-27), from the manual test:**<br>• The song title from Now Playing or the T key stays for 7 seconds, then fades out over 3 seconds. ini settings: `fSongTitleHold`, `fSongTitleFade`.<br>• A long title shrinks the font down to 35% before it is cut with "...".<br>• The T key with no song does nothing.<br>After pause and resume the title is not shown again; this is intended. Retested | `features/title_anim.*`, `milkdropfs.cpp`, `plugin.cpp`, `code/tests/test_title_anim.cpp` |
| 14 | **Phase 13 (2026-10-27): packaging.**<br>• `.\build.ps1 -Package` packages from the source files in a staging folder. The package has no ini, log or dragged-in presets.<br>• The sources of the presets, textures and base shaders are recorded in `content/PROVENANCE.md`.<br>• `-PresetSet Curated` packages the 500 presets in `content/curated.txt`, generated with `tools/make-curated.ps1`.<br>• Tool tests: `pwsh tools/test-tools.ps1`.<br>Manually tested: both packages unpacked and run | `tools/*`, `build.ps1`, `content/curated.txt` |

**Verified:** the build passes. The program starts with the window "MilkDrop 3.0.A" (today "MilkDrop 3.0.S"). Capture runs in real time
(about 1.38 million frames in about 29 seconds, 48kHz) from a 7.1 headset, without crashing.

**User's manual test (2026-10-25):** phase 2 (audio recovery) and phases 3–4 (keys and right-click menu) were tested and work.

---

## Phases 5–7 (2026-10-25)

Detailed plan: [superpowers/plans/2026-10-25-phase-5-7-effects-screens-desktop.md](superpowers/plans/2026-10-25-phase-5-7-effects-screens-desktop.md).

**Phase 5, effects (manually tested):**
- Effects menu on right-click: 11 effects, plus Brightness and Contrast controls.
- Effects are injected only into the comp shader, as blocks marked `// SURI-FX`.
- Effects stay active when switching presets and are stripped before saving, so the saved preset file is free of them.

**Phase 6, friendlier screens (manually tested):**
- Design direction B (cards) was chosen by the user.
- Card-style on-screen text: an info card, toast messages, paged F1 help with 5 topics, and M and L inside cards.
- DPI-based font sizes. The code editor uses a monospace font.
- The help lists every key that works, and Shift+R works like r.

**Phase 7, Desktop mode (manually tested, except for an Explorer restart):**
- Enter desktop background mode from the right-click View menu, or with Ctrl+Alt+Shift+D.
- While in the mode there is a tray icon: right-click opens a short menu, double-click returns to a normal window.
- If Explorer restarts, MilkDrop restarts itself and returns to the desktop background within a few seconds (manually tested, fix round 2). The manual test showed that when Explorer is killed, the render window (a child of the WorkerW) is destroyed with it, sometimes without WM_DESTROY reaching us: the process kept running with no window. Now the render loop checks `IsWindow` on the window in Desktop mode (and WM_DESTROY is handled too), starts a new instance of the program with `/desktop-resume "<preset>"` and exits normally. The new instance waits for the mutex until the old one exits, stays hidden, checks every 500ms (up to 60 seconds) for a WorkerW, and enters Desktop mode with the same preset. Loop protection: an instance started by resume does not restart again within 30 seconds (in that case `DesktopModeOnStart=1` stays in the ini as a fallback).
- Single instance: a mutex named `Local\MilkDropSURI.SingleInstance`. A second launch asks the running instance to come to the front (the `MilkDropSURI.Activate` message through a message-only window) and exits with code 0 before any window, audio or D3D is created. If the running instance is in Desktop mode, a message explains to press Ctrl+Alt+Shift+D.
- Leaving the mode does not reset the wallpaper (no `SPI_SETDESKWALLPAPER`): the WorkerW window that hosted us is hidden and Progman draws the user's wallpaper; on the next entry it is shown again. Slideshow, Spotlight and per-monitor wallpapers are not changed.
- Entering the mode closes F1 help, menus and open input lines (keyboard and mouse don't reach the window in this mode). If the hotkey is taken, a message is shown on start; if the tray icon also fails, the mode exits immediately with an error message.

### Manual test (phase 7) - completed 2026-10-25

- [x] Real Ctrl+Alt+Shift+D press: enter and exit.
- [x] `taskkill /f /im explorer.exe` and then `start explorer.exe` during Desktop mode: MilkDrop restarts and returns to the background within a few seconds (tested after fix round 2).
- [x] Screen lock (Win+L) and unlock.
- [x] Two monitors.
- [x] Exit when the desktop background is Slideshow or Spotlight: the original background is restored.
- [x] A second launch of MilkDrop3.exe exits immediately and brings the running instance to the front (tested automatically in round 2).

### Known issues

- Existing bug: `keyMappings` is not filled, so after pressing K the digits keep triggering sprites until the program restarts.
- The tray icon is not released (`DestroyIcon`). The detached window keeps rendering even when hidden.

---

## Tools installed on the development machine (2026-10-24)

| Tool | Version | Location / note |
|---|---|---|
| Graphify (skill) | 0.9.95 | `~/.claude/skills/graphify`, run with `/graphify .` |

---

## Plan / open tasks

**Full plan (roadmap):** [superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md](superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md). Each phase starts only after the user approves it. Webcam is not included.

- [x] Visual check by the user: play music and make sure the graphics react (F1 for help, Space for the next preset).
- [x] Show a message (MessageBox) when capture fails, instead of a silent exit with a negative code.
- [x] `-Package`: added the `-PresetSet Curated` option to package a selection of presets only (phase 13).
- [x] git for the project.
- [x] Manual tests: phase 8 (title display time and a long title), phases 9 and 10, and phase 13 (both packages unpacked into an empty folder and run).
- [x] Old worktrees and branches cleaned up.
- [ ] Later: the Audio device menu (needs device selection in the audio code).
- [ ] Later: improve the on-screen UI and menu (user request). First define which "panel" is meant: the on-screen menu (D3D) or the settings dialogs (Win32).
