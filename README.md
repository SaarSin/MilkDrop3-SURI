# MilkDrop 3.0.S (SURI build)

A standalone Windows music visualizer. It shows MilkDrop presets reacting to whatever is playing through your speakers (Spotify, YouTube, any player), captured with WASAPI loopback. No Winamp or plugin host is needed.

This build is based on [milkdrop2077/MilkDrop3](https://github.com/milkdrop2077/MilkDrop3) (version 3.0.A, commit `6b39088`), which is itself based on MilkDrop 2 and BeatDrop. Everything that changed since is listed in [CHANGES.md](CHANGES.md).

## What's new in 3.0.S

- **Right-click menu** for presets, effects, view, sprites, messages and help. All keyboard shortcuts still work.
- **Real-time effects**: mirrors, kaleidoscope, ripple, tunnel, invert, mono, posterize, hue spin, brightness and contrast. They work on any preset, and they are never saved into the preset file.
- **Desktop mode**: the visualization becomes your animated wallpaper, behind the desktop icons. Turn it on with Ctrl+Alt+Shift+D or from the View menu. A tray icon appears while it's on.
- **Now Playing**: shows the title and artist of the current song, from the Windows media controls.
- **Sprites and messages**: PNG, GIF and JPG images over the visualization. Move them with the mouse, resize them with the wheel, and save them with the preset.
- **Drag & drop and Playlist**: drop presets or folders onto the window, and build playlists with the P key.
- **Stable audio**: capture recovers when the audio device changes or is unplugged.
- **9,795 presets** from projectM's "Cream of the Crop" pack, plus the MilkDrop textures.

## Download and run

1. Download a zip from [Releases](https://github.com/SaarSin/MilkDrop3-SURI/releases):
   - `MilkDrop3-SURI-3.0.S-win32.zip` has all the presets.
   - `MilkDrop3-SURI-3.0.S-win32-curated.zip` has 500 selected presets.
2. Unzip it into any folder.
3. Run `MilkDrop3.exe`, then play music.

It needs Windows 10 or 11. Press **F1** for help, **Space** for the next preset, or right-click for the menu. The full user guide is under Help in the menu.

## Build from source

You need Visual Studio 2022 with "Desktop development with C++".

```powershell
.\build.ps1            # Release build -> build\Release\MilkDrop3.exe
.\build.ps1 -Run       # build and run
.\build.ps1 -Test      # build and run the unit tests
.\build.ps1 -Package   # build the release zips in dist\
```

The build downloads D3DX9 from NuGet automatically. See [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md) for the code layout and notes, and [docs/superpowers/](docs/superpowers/) for the plans each feature was built from.

## License and credits

- **Code:** BSD licenses. MilkDrop 2 is © Nullsoft, MilkDrop3 / BeatDrop is © Maxim Volskiy and contributors, and the changes in this build are © SaarSin. See [LICENSE.txt](LICENSE.txt).
- **Presets and textures:** from the projectM community packs. They are credited to their authors by file name. See [content/PROVENANCE.md](content/PROVENANCE.md). If you are a preset author and want your work removed, please open an issue.
- **D3DX9 runtime DLLs:** redistributed under Microsoft's terms, which are included in the package.
