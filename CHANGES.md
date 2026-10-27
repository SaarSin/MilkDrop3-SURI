# MilkDrop 3.0.S (SURI build): changes from upstream

**Upstream:** https://github.com/milkdrop2077/MilkDrop3, commit `6b39088f789441e18cc1665519ee7ffe439cece5` (the `main` branch as downloaded on 2026-09-27; the upstream `code/` folder was last changed on 2023-04-17). Upstream calls its version **3.0.A**. This build is **3.0.S**.

Everything below was added or changed on top of that commit. How each part was planned and built is in the work plans under [docs/superpowers/](docs/superpowers/) (see [Work plans](#work-plans)). The current state of the project, with build instructions and findings, is in [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md).

---

## Build and project

- **Visual Studio 2022** (toolset v143, Windows 10/11 SDK). The upstream project files for older Visual Studio versions (`plugin.dsp`, `plugin.vcproj`) were removed.
- **D3DX9 from NuGet** (`Microsoft.DXSDK.D3DX`), downloaded automatically into `third_party\d3dx`. The 2010 DirectX SDK is no longer needed.
- **`build.ps1`**: builds from the command line without opening Visual Studio.
  - `-Run` builds and runs.
  - `-Test` builds and runs the unit tests.
  - `-Clean` does a full rebuild.
  - `-Config Debug` builds the Debug configuration.
  - `-Package` creates a zip.
  - `-PresetSet Curated` limits the zip to the curated presets.
- **Output layout:** `build\<Config>\MilkDrop3.exe`, the D3DX DLLs next to it, and the program's files in `plugins\Milkdrop2\` (`data`, `docs`, `presets`, `textures`). A post-build step copies them.
- **Unit tests:** a new console project, `code/tests/unit_tests.vcxproj`, with a small test framework (`mini_test.h`). The pure logic of every new feature (no D3D and no window) is tested there.
- **Win32 (x86) only.** ns-eel2 compiles preset code with MSVC inline asm, which x64 does not support. A 32-bit exe runs normally on 64-bit Windows.
- **Packaging** (`-Package`): the zip is built from the source files in a staging folder (`dist\stage\`), never from the build folder. The user's ini, the log and dragged-in presets never end up in the package.
  - `dist\MilkDrop3-SURI-<version>-win32.zip` has all the presets.
  - `-curated.zip` has the 500 presets in `content/curated.txt`. Regenerate that list with `tools/make-curated.ps1`.
- `tools/test-tools.ps1` contains self-tests for the packaging and the curated list generator.

## Fixes

- **Build error C2169:** VS2022 reserves the name `__floor`. It was renamed in `nseel-compiler.c`.
- **Audio capture on multichannel devices:** a 7.1 headset (8 channels, float, 48kHz) made `IAudioClient::Initialize` fail with `AUDCLNT_E_UNSUPPORTED_FORMAT`, and the program exited silently with code -520. Capture now logs the mix format. If the device rejects it, capture retries as stereo float with `AUTOCONVERTPCM`.
- **A log file:** `LOG`/`ERR` output is also written to `MilkDrop3.log` next to the exe. Before, it went only to `OutputDebugString`.
- **A clear error message** (MessageBox) when audio capture fails, instead of a silent exit with a negative code. The visualization keeps running without audio.
- **Hang on exit:** closing the window while the preset scan was running hung the process.
- **No crash** when a random custom message is requested but none are defined.
- **Sprites in non-4:3 windows** were stretched. This MilkDrop 2 bug is fixed.

## Removed

- **Files:**
  - The screenshots and `README.md`.
  - The Linux launcher (`linux/`).
  - The non-Windows ns-eel2 asm sources (`asm-nseel-ppc-gcc.c`, `asm-nseel-x86-gcc.c`, `asm-nseel-x64-macho.o`).
  - The prebuilt executables in `code/vis_milk2/Debug/`.
  - `manifest.xml`, `plugin_icon2.ico`, `text1.bin` and `plugin.vcxproj.user`.
  - The old help pages (`milkdrop.html`, `milkdrop_preset_authoring.html` and their images). New guides replace them; see below.
  - The two license files. They were combined into one root `LICENSE.txt`.
- **Code:**
  - The Wasabi/Winamp language layer (`wasabi.cpp/.h`). One helper, `LoadResString` (`resstrings.*`), replaces it.
  - Winamp's playlist and player-control commands.
  - Leftovers of the musikcube plugin.
  - Winamp's old Desktop mode (`vms_desktop`) and its settings.
  - In the audio code: WAV writing, the int16 mode and command-line parsing. `CPrefs` opens only the default device.
  - 13 unused functions in `utility.cpp`.
  - 202 unused strings and 77 unused IDs in `plugin.rc`/`resource.h`.
  - History comments, old TODO lists and an `#if 0` block.

## New features

### Stable audio
- Capture recovers when the audio device is unplugged or the Windows default device changes. It restarts within a few seconds (up to 5 attempts, 500ms apart), and every decision is logged.
- **Code:** `code/audio/device-watch.*`.

### One command table and a right-click menu
- **Command table:** all the preset and View keys go through one table: h, Backspace, Space, `` ` ``, r, l, s, m, +, -, d, t, F1–F7, F9, Alt+Enter, Alt+Shift and Alt+F4. The keyboard, the menu and the tray icon call the same commands.
- **Right-click menu:** right-click, Shift+F10 or the Apps key opens a Win32 menu built from that table, with check marks and greyed-out items.
- **Menu behavior:** in fullscreen the cursor is shown while the menu is open, and rendering continues while the menu is open. Help opens the guides from `docs\`.
- **Code:** `features/commands.*`, `features/context_menu*`.

### Real-time effects
- **The effects:** 11 effects that can be combined.
  - Geometry: Mirror X, Mirror Y, Kaleido, Ripple, Zoom-in, Swap axes and Tunnel.
  - Color: Invert, Mono, Posterize and Hue spin.
  - Brightness and Contrast controls.
- **How they work:** effects are injected into the comp shader as blocks marked `// SURI-FX`. They stay on when the preset changes, and are stripped before a preset is saved, so saved files are clean.
- **Code:** `features/shader_fx.*`.

### Friendlier on-screen screens
- **Cards:** the on-screen text uses a card design: an info card, toast messages, paged F1 help with 5 topics, and the M and L screens.
- **Fonts:** font sizes scale with DPI, and the code editor uses a monospace font.
- **Help:** the help lists every key that works.
- **Code:** `features/ui_scale.*`, `features/help_text.*`.

### Desktop mode (animated wallpaper)
- **Entering:** from the View menu, or with Ctrl+Alt+Shift+D. MilkDrop renders behind the desktop icons, across the virtual screen.
- **Tray icon:** while the mode is on, a tray icon appears. Right-click it for a short menu, or double-click it to return to a normal window.
- **Explorer restart:** if Explorer restarts, MilkDrop relaunches itself with `/desktop-resume` and returns to the wallpaper with the same preset.
- **Leaving:** the user's wallpaper is untouched (no `SPI_SETDESKWALLPAPER`).
- **Single instance:** a second launch brings the running instance to the front and exits.
- **ini setting:** `DesktopModeOnStart`.
- **Code:** `features/desktop_mode*`.

### Song info (Now Playing)
- **How it works:** the title and artist of the song playing are read from the Windows media controls (C++/WinRT, `GlobalSystemMediaTransportControlsSessionManager`) and shown when the song changes. The T key shows them again.
- **Display:** the title stays for 7 seconds and then fades out over 3 seconds. A long title shrinks before it is cut.
- **ini settings:** `bNowPlaying`, `fSongTitleAnimDuration`, `fSongTitleHold` and `fSongTitleFade`.
- **Code:** `features/now_playing*`, `features/title_anim.*`.

### Sprites and messages
- **Loading:** PNG, GIF (first frame) and JPG sprites load from the menu with transparency.
- **Moving and resizing:** drag a sprite with the left mouse button, and resize it with the wheel.
- **Blend modes:** 6 modes. The new mode 5 uses the image's own alpha.
- **Menus:** Sprites and Messages menus. `milk2_msg.ini` is in UTF-16.
- **Saving:** sprites are saved inside the `.milk` (`suri_sprite*` keys) and restored when the preset loads.
- **Code:** `features/sprite_*`, `features/messages.*`, `features/image_load_win32.*`.

### Drag & drop and Playlist
- **Dragging:** drag a folder or `.milk` files onto the window.
  - Presets that already exist play immediately.
  - New presets are first shown in a confirmation card, then copied without overwriting anything. Their textures are copied only when needed.
- **Playlist screen** (P key):
  - Two lists, Not playing and Playing.
  - Multi-select and type-to-search.
  - `.m3u` save and load in `playlists\`.
- **Code:** `features/drop_import.*`, `features/playlist*`.

## Content

- **`content/presets/`:** 9,795 presets from the "Cream of the Crop" pack by ISOSCELES, as distributed by projectM. Upstream ships no presets.
- **`content/textures/`:** 67 textures from projectM's Milkdrop Texture Pack.
- **More details:** the sources and licenses are in [content/PROVENANCE.md](content/PROVENANCE.md).

## Documentation

- **New guides:** `code/resources/Milkdrop2/docs/user-guide.html` and `preset-authoring.html` are offline HTML guides that open from the Help menu.
- **`LICENSE.txt`:** the MilkDrop 2 and MilkDrop3/BeatDrop licenses in one file.

## Work plans

The project was planned on top of the upstream commit above, as a roadmap of phases. Each phase was approved separately before it was built.

| Document | Contents |
|---|---|
| [Roadmap](docs/superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md) | The full plan: phases -1 (cleanup), 0 (infrastructure), 2 (audio), 3–10 and 13, with each phase's status |
| [Phases 3–4](docs/superpowers/plans/2026-10-24-phase-3-4-commands-and-context-menu.md) | Command table and right-click menu |
| [Phases 5–7](docs/superpowers/plans/2026-10-25-phase-5-7-effects-screens-desktop.md) | Effects, friendlier screens and Desktop mode |
| [Phases 8–9](docs/superpowers/plans/2026-10-25-phase-8-9-now-playing-sprites.md) | Song info, sprites and messages |
| [Phase 10 design](docs/superpowers/specs/2026-10-25-phase-10-drag-drop-playlist-design.md) and [plan](docs/superpowers/plans/2026-10-25-phase-10-drag-drop-playlist.md) | Drag & drop and Playlist |
| [Phase 13](docs/superpowers/plans/2026-10-27-phase-13-packaging.md) | Packaging |

Not built, by decision: webcam input, sending the image to other programs (Spout), and an x64 build. The Audio device menu is postponed until the audio code exposes device selection.

## Known issues

- After pressing K, the digit keys keep triggering sprites until the program restarts (`keyMappings` is not filled).
- The tray icon handle is not released (`DestroyIcon`).
- VJ mode (`vj_mode=1` in `[settings]`, inherited from MilkDrop 2) has no menu item and was not tested in this build.
