# Content provenance

This file records where everything in `content/` comes from.

| Folder | Source | License |
|---|---|---|
| `presets/` (9,795 files) | The **"Cream of the Crop"** pack selected and organized by ISOSCELES, as distributed by projectM: https://github.com/projectM-visualizer/presets-cream-of-the-crop | See below |
| `textures/` (67 files) | projectM's **Milkdrop Texture Pack** (the original MilkDrop textures and more): https://github.com/projectM-visualizer/presets-milkdrop-texture-pack | See below |

**Preset and texture license** (summary of projectM's notice): almost all presets were released without a stated license. Copyright belongs to each author, but since they were released for free and have been used in many packs for about 20 years, they can be treated as public domain. If an author asks for their preset to be removed, we will remove it.

## Base shaders

The 7 original MilkDrop 2 shader files in `code/resources/Milkdrop2/data/` (`blur_vs`, `blur1_ps`, `blur2_ps`, `comp_vs`, `comp_ps`, `warp_vs`, `warp_ps`) have the same text as the files in the public Winamp source code, https://github.com/alexfreud/winamp (commit `0954e03b3acbc11b6c90298598db6b6469568d72`, path `Src/resources/data/Milkdrop2/data/`). Our copies use CRLF line endings and the public source uses LF; otherwise there is no difference.
