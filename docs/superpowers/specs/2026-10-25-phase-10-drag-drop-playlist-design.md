# Phase 10: Dragging presets and a Playlist (design document)

> Replaces the phase 10 definition in the [roadmap](../plans/2026-10-24-milkdrop-suri-roadmap.md). The phase grew from "small" to "medium–large", because it now includes a playlist and a screen for managing it.
> Notation: **(user decision)** = settled in conversation with the user. **(design decision)** = my decision, not yet explicitly approved by the user, and open to change.

## Intent

Change what is playing quickly, without menus: drag a folder (or `.milk` files) from Explorer onto the window, and playback switches to them immediately. Alongside that, a Playlist screen where you see what is playing and what isn't, move presets between the sides, and save or load lists.

## Goals

1. Dragging a folder or `.milk` files onto the window.
2. Detecting what already exists in the library and what is new, and asking about the new ones.
3. A session playlist that restricts playback.
4. An on-screen Playlist screen, with save and load.

## Not in this phase

- **Dragging images or sprites.** It won't exist at all, not even after phase 9. Images are loaded in the settings screens through a file picker **(user decision)**. Dragging an image shows the toast `Images: load from settings` and does nothing **(design decision)**.
- Automatically saving the playlist between sessions **(user decision)**. Every launch starts from "play everything".
- Scanning subfolders of a dragged folder **(design decision)**.
- Dragging in Desktop mode (phase 7). The window is behind the icons and doesn't receive drops, which is fine **(design decision)**.

## Anti-goals (what counts as a failure even if it "works")

- **Today's behavior changes for someone who doesn't touch the feature.** Without dragging and without opening the screen, everything behaves exactly as today **(user decision)**.
- **Silent copying.** Files are copied into the library only after explicit approval.
- **Overwriting an existing file in the library.** Never.
- The screen hangs or gets slow with ~10,000 presets.

## Terms

- **Library:** the current presets folder, `m_szPresetDir`. Today it has 9,795 files in one folder. The user can change it in the `L` browser.
- **Playlist:** the set of presets from the library that may be played. The default is **the whole library** **(user decision)**.

## 1. Dragging onto the window

- `DragAcceptFiles(hwnd, TRUE)` on the visualizer window, and handling `WM_DROPFILES` (`DragQueryFileW`, `DragFinish`).
- **Classify each dragged path** (a pure function, unit-tested): folder, `.milk` (case-insensitive), image (png/jpg/jpeg/gif/bmp/tga/dds), or unsupported.
- **Folder:** take all the `.milk` files in it, top level only.
- **Several `.milk` files together** (or a folder plus files): they are all gathered into one group and handled like a folder **(design decision)**.
- **A single `.milk` file:** goes through the same existing-or-new detection. Afterward it **plays immediately, and the playlist does not change** **(design decision)**. If it is new, a "new files" window appears with one row. If `Esc` is pressed there, nothing happens: the file is neither played nor copied. That way every playing preset is always in the library, and `LoadPreset` never needs to accept paths outside it **(design decision)**.
- A drag without any `.milk`: the toast `No presets found` appears and nothing changes.

### Existing-or-new detection **(user decision)**

For each dragged file, against the library:

| Case | Result |
|---|---|
| A file with the same name exists (case-insensitive, as in Windows) **and the same size in bytes** | **Existing**. Use the library's file and don't copy. |
| A file with the same name exists, with a different size | **New**. If approved, it is copied under a numbered name. |
| No file with that name | **New**. If approved, it is copied under its original name. |

- **Numbering:** `Name.milk` → `Name (2).milk`. If that is taken too, `(3)` and so on, up to the first free number. This is the same format that already exists in the library, like `Mashup (10).milk` **(design decision on the format details)**.
- If `Name (2).milk` already exists **with the same size**, there is no need to check it as existing: the comparison is only against the original name **(design decision; simple, and at worst creates a duplicate copy)**.
- Files are copied to the root of the library.

### Copying textures **(user decision: textures are copied too)**

Presets from another collection sometimes use textures that sit next to them. For each new preset approved for adding:

- **Which textures:** the preset refers to a texture in its shader code as `sampler_<name>`, possibly with an `fw_`/`fc_`/`pw_`/`pc_` prefix. The builder extracts the names with the same logic as loading in `plugin.cpp` (around line 2380). Skip the built-in names (`main`, `blur1`–`blur3`, `noise_*`) and `rand*`, which picks a random texture from the existing ones **(design decision)**.
- **Where to look on the dragged side:** in the `.milk`'s folder, and in its `textures\` subfolder if there is one. The extensions are those of `texture_exts` (jpg, dds, png, tga, bmp, dib) **(design decision)**.
- **Where to copy:** to MilkDrop's `textures\` (`m_szMilkdrop2Path\textures\`), because loading looks there first **(design decision)**.
- **Collision:** if a texture with the same name and the same size exists there, no copy is needed. If the name is the same and the size differs, **don't overwrite and don't rename**, because the preset refers to the texture by name. Keep the existing one, and at the end the toast `K textures kept (name already used)` appears **(design decision; follows from the "never overwrite" anti-goal)**.
- A texture not found on the dragged side: do nothing. It probably already exists on the user's side, or is missing in the source too.
- Textures are copied only with presets **checked** in the "new files" window. Presets that already exist don't trigger copying.
- Unit test: `TestDrop_TextureRefs`, which extracts `sampler_` names from preset text and skips the built-in names.

### The "new files" window **(user decision: choose one by one)**

An on-screen card in the phase 6 style ("Cards", direction B). The mockup: `.superpowers/brainstorm/1252-1792882475/content/import-prompt.html`, option B.

- Title `N new presets`. Subtitle `Not in your library yet. Choose which to add.`
- A list of the new files, all **checked in advance**. Next to a name that will be numbered, the new name is shown (`Galaxy.milk → Galaxy (2).milk`).
- Keys: arrows to navigate, `Space` toggles a check, `Ctrl+A` checks all (pressing again clears all) **(design decision)**, `Enter` adds the checked ones (`Enter add K`), `Esc` skips all the new ones.
- Scrolling when the list is longer than the card.
- **After closing:** the checked ones are copied into the library, the library is rescanned, and the playlist is built: the existing ones plus those copied.
- **If there are no new files:** the window doesn't appear, and things continue directly.
- **If the playlist comes out empty** (all files were new and `Esc` was pressed): nothing changes.

### What happens after dragging a group

1. The playlist is replaced by the new group. It **does not merge** with the previous one **(design decision)**.
2. The first preset in the playlist starts immediately, with a normal blend **(user decision: "it will pick them for playback" immediately)**. "First" means by file name order **(design decision)**.
3. Toast: `Playlist: N presets`.

## 2. The playlist and its effect on playback

- **Initial state:** the whole library. Behavior identical to today.
- When the playlist doesn't include the whole library, `Space` (random), the automatic switch, `H` (next) and `Backspace` (previous) choose **only from the playlist**. Random or sequential mode (`R`) keeps working as today, but only within the playlist. Rating weights are kept.
- **History and sequential switching** **(design decision)**: when the playlist is replaced (drag, load, or closing the screen after a change), the history (`m_presetHistory`) is reset. `Backspace` goes back in history as today, including presets chosen from `L`, because those really played. Only choosing a **new** preset is restricted to the playlist (random, or next in sequential mode). In sequential mode, "next" is the next name in the playlist in alphabetical order after the current preset. If the current one isn't in the playlist, it's the first name after it alphabetically, and at the end of the list it wraps to the start.
- The `L` browser keeps showing the whole library, and any preset can be chosen from it even if it's not in the playlist. Such a choice does not change the playlist **(design decision)**.
- **An empty playlist** (the user moved everything to the "Not playing" side): play everything, so the screen isn't left without a preset. When the screen closes, the toast `Playlist empty: playing all` appears **(design decision)**.
- **A change in the library** (deleting or renaming in `L`, or switching to another folder): the playlist holds file names. A name that no longer exists drops out of it silently. When the library folder is switched, the playlist returns to "all" **(design decision)**.
- The playlist is not saved in the ini. A new launch starts from "all".

## 3. The Playlist screen **(user decision: an on-screen card, direction B)**

The mockup: `.superpowers/brainstorm/1252-1792882475/content/playlist-layout.html`, option B. The design follows phase 6, Task 6: the same colors, fonts, `DrawRoundedPanel`, and DPI scaling.

**Opening** **(design decision)**:
- The `P` key (free in the `commands.cpp` table). A new `Cmd::PlaylistScreen` in the table, so it appears automatically in the F1 help.
- A `Playlist...` line in the right-click menu, next to the Presets items.
- Only `Esc` closes. While the screen is open, `P` and the other characters go to search (see "Keys") **(design decision)**.

**Layout:**
- A centered card. Two columns: **Not playing** on the left and **Playing** on the right. Each column's header shows its item count.
- By default the whole library is on the **Playing** side. After a drag, only the playlist is on the Playing side and everything else is in Not playing **(user decision)**.
- The lists are sorted by name.
- A bottom line with the keys: `Tab side · Ctrl+A all · Ctrl+S save · Ctrl+O load · Esc close`.

**Keys:**
- `↑`/`↓`, `PgUp`/`PgDn`, `Home`/`End`: navigation in the active column.
- `Tab`: switching between the columns.
- `Shift+↑/↓`: extending the selection. `Ctrl+Space`: adding or removing a row from the selection. A plain `Space` is a space in the search **(design decision)**.
- `Ctrl+A`: selects all the items in the active column. If there is a search filter, it selects all the items shown **(user decision on `Ctrl+A`; the behavior with search is a design decision)**.
- `→`: moves the selection from Not playing to Playing. `←`: moves it the other way. Moved items stay sorted.
- **Search:** any printable character, including space and `P`, filters the active column (contains, case-insensitive). `Backspace` deletes a character. `Esc` clears the search if there is one, otherwise closes the screen **(design decision)**.
- Changes apply **immediately**, and there is no "cancel" button **(design decision)**. If the currently playing preset leaves the playlist, it keeps playing until the next switch.
- While the screen is open, the regular preset keys (`Space`, `H`, `L` etc.) don't work, as with `textEditActive` in `QueryState`.

**Performance:** ~10,000 rows. Only the visible rows are drawn, and the filter runs on every keystroke (linear, fast enough) **(design decision)**.

## 4. Saving and loading a playlist **(user decision: they exist)**

- `Ctrl+S` / `Ctrl+O` in the Playlist screen. They may also appear in the right-click menu under `Playlist`.
- **The file picker:** the regular Windows `GetSaveFileNameW` / `GetOpenFileNameW` **(design decision; typing a path in the card is clumsy)**. In fullscreen, make sure the dialog appears on top, or leave fullscreen temporarily, at the builder's choice.
- Default folder: `playlists\` next to the ini (`m_szMilkdrop2Path`). Created if it doesn't exist.
- **Format** **(design decision)**: `.m3u` in UTF-8 with a BOM. The first line is `#EXTM3U`, then one file name per line, **name only, no path** (relative to the library). That way the file keeps working even when the library moves. Lines starting with `#` and blank lines are not read.
- **Loading:** replaces the playlist. A name that doesn't exist in the library is skipped. At the end the toast `Loaded X of Y presets` appears **(user decision)**, where Y is the number of valid lines in the file. If X is 0, the playlist doesn't change.
- After loading, if the playing preset isn't in the playlist, switch to the first one in the playlist **(design decision; same as dragging)**.

## Constraints

- C++ and Win32 like the existing code, with no new dependency. `shell32` (`DragAcceptFiles`) and `comdlg32` are already available in the Windows SDK.
- Pure logic in `code/vis_milk2/features/` (like `commands.cpp`, `ui_scale.cpp`), with unit tests in `code/tests/`. Only the hookup lives in `plugin.cpp` / `pluginshell.cpp`.
- Depends on phase 6 (the design, `DrawRoundedPanel`, `ToastQueue`). Phase 6 currently exists only in `suri/phase-5-7`, and part of it is not yet committed. So building starts after phase 6 is committed, on a branch based on it. Phase 10 doesn't depend on phases 8 and 9: image dragging was taken out of it, so it can be built before them **(design decision)**.
- Approval by phases, as in the rest of the roadmap: stop for the user's manual test at the end.
- Work in a separate worktree. Other sessions work on the same tree.

## Tests

**Unit** (names for illustration):
- `TestDrop_Classify`: folder, `.milk`/`.MILK`, images, an unknown extension.
- `TestDrop_ExistsOrNew`: same name and same size → existing; same name and a different size → new with `(2)`; a new name → new; a collision at `(2)` → `(3)`; upper and lower case.
- `TestPlaylist_M3uRoundTrip`: save and load, Hebrew names (UTF-8), `#` and blank lines, BOM.
- `TestPlaylist_LoadMissing`: 34 lines, 3 missing → `Loaded 31 of 34`.
- `TestPlaylist_PickRestricted`: next, previous and random choose only from the playlist; an empty playlist → the whole library.
- `TestPlaylistScreen_MoveAndFilter`: `Ctrl+A` with a filter, and moving between the sides.

**Manual (user):** dragging a mixed folder (existing, new and a name collision), a single file, an image, `Esc` in the new-files window, the screen with the whole library (performance), saving, deleting a file and loading (`X of Y`), dragging in fullscreen, and at 150% DPI.

## Left to the builder

- The code structure and classes, and how the playlist fits into the existing preset choice (`LoadRandomPreset`, next and previous, mash-up).
- The details of drawing the screen within the phase 6 design framework (row height, column width, marking the active row versus the selection).
- How to handle the file dialogs in fullscreen.
- The order of work and the breakdown into tasks.
