# Phases 5–7: Real-time effects, friendlier screens, Desktop mode – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** (5) Geometry and colour effects toggled from the menu on any preset. (6) Help screens, messages and the edit menu readable at high DPI. (7) A mode in which the visualization runs as the background behind the desktop icons, with a tray icon and a global shortcut.

**Architecture:** as in phases 3–4: pure logic in `code/vis_milk2/features/` with tests in `unit_tests`, and small hookups in `plugin.cpp` and `Milkdrop2PcmVisualizer.cpp`. Effects are HLSL text injected **into the composite shader only** and rebuilt each time from a state (`FxState`), so the operation is idempotent. Desktop mode is one class (`CDesktopMode`) that receives the HWND and a resize callback; the existing code does the rest (the D3D Reset).

**Tech Stack:** C++14 in the `plugin` project (`CharacterSet=NotSet`), C++17 + Unicode in `unit_tests`, Win32 (`Shell_NotifyIconW`, `RegisterHotKey`, `SetParent`, `GetDpiForWindow`), D3D9/D3DX9, `mini_test.h`.

**Spec:** [2026-10-24-milkdrop-suri-roadmap.md](2026-10-24-milkdrop-suri-roadmap.md) phases 5, 6, 7; [PROJECT_STATUS.md](../../PROJECT_STATUS.md).

## Global Constraints

- **Approval gates:** this plan is executed only after the user approves it. Stop for a manual test and approval at the end of each phase (after Task 3, after Task 6, after Task 10). In phase 6 also stop after Task 4 (the user picks a design direction) before Task 5.
- **Base:** a new branch `suri/phase-5-7` that starts where phases 3–4 end. Before any whole-tree operation (stash/reset/checkout), run `ListAgents` and `git status`.
- **Webcam is out of scope.** No menu item, no Media Foundation linking.
- All HLSL in this plan is written from scratch, and injection goes into the comp shader. Every injected block is marked `// SURI-FX:`.
- The pure `features/*.cpp` code compiles as C++14, as Unicode and as MBCS: no `std::optional`, no structured bindings, and explicit calls to the `W` versions.
- No new dependencies. Win32 (x86), v143.
- Each Task ends when `.\build.ps1` passes with no errors and no new warnings (2 existing C4477 warnings), and `.\build.ps1 -Test` prints `0 failed`.
- Every new Cmd must appear in the menu exactly once (the existing `TestMenu_EveryCommandExactlyOnce`) and get a label (`TestMenu_LabelsExist`). New `IDS_*` IDs start at 737.
- One commit per Task. The commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Saving a preset while an effect is active, and a preset with PS < 2 that the effect upgraded:** the file is expected to be saved without any `SURI-FX` line and with the original PS versions (`PSVERSION_COMP=0` stays 0). Tested in `TestShaderFx_StripAllBeforeSave` (Task 1) and manually in Task 3.
2. **Comp shader lock (`m_bCompShaderLock`) or a new preset while an effect is active:** the old preset's comp text, which already contains blocks, is copied into the new preset. The effect is expected to appear only once and not cancel out. Tested in `TestShaderFx_ApplyIsIdempotent` (Task 1).
3. **A preset whose `shader_body` block is non-standard** (no `shader_body`, a `{` inside a comment, functions before `shader_body`, a `begin` marker without `end` after a manual edit in M): expected no crash, the text unchanged when injection is impossible, and a failed compile falling back to the existing fallback. Tested in `TestShaderFx_NoShaderBody_ReturnsFalse`, `TestShaderFx_BraceInComment`, `TestShaderFx_BrokenMarkerLeftAlone` (Task 1).
4. **Exit, an Explorer crash or a screen lock (Win+L) during Desktop mode:** the wallpaper, tray icon and window are expected to be restored, and after Explorer restarts (`TaskbarCreated`) the mode is expected to resume by itself. Tested manually in Task 10, and the pure part in `TestDesktop_ReattachDecision` (Task 8).
5. **DPI and mixed monitors** (a 4K monitor at 150% next to a 1080p monitor at 100%, moving the window between them): the text size is expected to match the monitor the window is on, after `WM_DPICHANGED`. Tested in `TestUiScale_FontPixels` (Task 5) and manually in Task 6.

---

## File structure

| File | Responsibility | Built in |
|---|---|---|
| `code/vis_milk2/features/shader_fx.h/.cpp` | `FxId`, `FxState`, `ApplyFx`, `StripAllFx`, Brightness/Contrast steps | plugin + tests |
| `code/vis_milk2/features/ui_scale.h/.cpp` | DPI calculations, splitting the help into pages, toast queue | plugin + tests |
| `code/vis_milk2/features/desktop_mode.h/.cpp` | The pure part: tray menu layout, choosing a WorkerW from a list, the reattach decision | plugin + tests |
| `code/vis_milk2/features/desktop_mode_win32.cpp` | `CDesktopMode`, `CTrayIcon` | plugin only |
| `code/tests/test_shader_fx.cpp`, `test_ui_scale.cpp`, `test_desktop_mode.cpp` | Tests | tests |
| `code/vis_milk2/features/commands.h/.cpp`, `context_menu.cpp` | New Cmds, extended `AppSnapshot`, Effects and Desktop menus | plugin + tests |
| `code/vis_milk2/plugin.cpp/.h` | `m_fx`, `ApplyFxToState`, `SavePresetAs`, Dispatch | – |
| `code/vis_milk2/pluginshell.cpp`, `textmgr.cpp`, `menu.cpp` | DPI-based fonts, background, paged help, toast | – |
| `code/vis_milk2/Milkdrop2PcmVisualizer.cpp` | `WM_DPICHANGED`, Desktop mode, tray, `WM_HOTKEY`, `TaskbarCreated` | – |
| `code/vis_milk2/plugin.rc`, `resource.h`, the vcxproj files | Strings and files | – |

---

# Phase 5: Real-time effects

**Final list of effects (for the user's approval).** Everything is in the comp shader. Geometry is injected right after the `{` of `shader_body` and modifies `uv` (which is `_uv.xy`, so it is assignable). Colour is injected right before the `}` that closes `shader_body` and modifies `ret`. The order within each group is fixed, in enum order, so the result does not depend on the order of activation.

| FxId | Group | HLSL (written from scratch) |
|---|---|---|
| MirrorX | Geometry | `uv.x = 0.5 - abs(uv.x - 0.5);` |
| MirrorY | Geometry | `uv.y = 0.5 - abs(uv.y - 0.5);` |
| Kaleido | Geometry | 6 segments: `float2 kd = uv - 0.5; float kr = length(kd); float ka = atan2(kd.y, kd.x); float ks = 1.0471976; ka = abs(ka - ks * floor(ka / ks) - 0.5 * ks); uv = 0.5 + kr * float2(cos(ka), sin(ka));` |
| Ripple | Geometry | `uv += 0.008 * sin(uv.yx * 31.0 + time * float2(1.7, 2.3));` |
| ZoomIn | Geometry | `uv = 0.5 + (uv - 0.5) * 0.7;` |
| SwapAxes | Geometry | `uv = uv.yx;` |
| Tunnel | Geometry | `float2 td = uv - 0.5; uv = float2(atan2(td.y, td.x) * 0.1591549 + 0.5, frac(0.1 / max(length(td), 0.01)));` |
| Invert | Colour | `ret = 1.0 - ret;` |
| Mono | Colour | `ret = dot(ret, float3(0.3, 0.59, 0.11)).xxx;` |
| Posterize | Colour | `ret = floor(ret * 4.0 + 0.5) * 0.25;` |
| HueSpin | Colour | Rotation around the grey axis: `float3 hk = 0.57735; float hc = cos(time * 0.6); float hs = sin(time * 0.6); ret = ret * hc + cross(hk, ret) * hs + hk * dot(hk, ret) * (1.0 - hc);` |
| Brightness | Colour, value | `ret *= <b>;` when b ≠ 1.0. Step ±0.1, range 0.2–3.0 |
| Contrast | Colour, value | `ret = (ret - 0.5) * <c> + 0.5;` when c ≠ 1.0. Step ±0.1, range 0.2–3.0 |

**Behavior:**
- Effects **stay active when switching presets** until "Clear effects". They are not saved in the ini between runs.
- A preset with `m_nCompPSVersion < MD2_PS_2_0`: when the first effect is turned on, the comp moves to `MD2_PS_2_0` with `GenDefaultCompShader()`, as in the existing `UI_UPGRADE_PIXEL_SHADER` path, and the original version is kept for saving. The warp does not change.
- If `m_nMaxPSVersion == 0` (no shader support), all Effects commands are disabled in the menu.
- The keyboard gets no new shortcuts in this phase. Menu only.

### Task 1: Effects core (pure)

**Files:**
- Create: `code/vis_milk2/features/shader_fx.h`, `code/vis_milk2/features/shader_fx.cpp`, `code/tests/test_shader_fx.cpp`
- Modify: `code/tests/unit_tests.vcxproj`, `code/vis_milk2/plugin.vcxproj` and `.filters`

**Interfaces:**
- Produces (`shader_fx.h`, includes `<string>`):

```cpp
enum class FxId : unsigned char {
    MirrorX, MirrorY, Kaleido, Ripple, ZoomIn, SwapAxes, Tunnel,   // geometry (uv), in this order
    Invert, Mono, Posterize, HueSpin,                              // colour (ret), in this order
    Count
};
struct FxState {
    unsigned mask = 0;          // bit (1u << (unsigned)FxId)
    float brightness = 1.0f;    // 0.2 .. 3.0, 1.0 = off
    float contrast   = 1.0f;    // 0.2 .. 3.0, 1.0 = off
    bool Any() const;           // mask != 0 || brightness != 1 || contrast != 1
};
void  ToggleFx(FxState& s, FxId id);
float StepLevel(float current, int direction);   // +-0.1, rounded to 0.1, clamped 0.2..3.0
// Removes every "// SURI-FX:<name> begin" .. "// SURI-FX:<name> end" block (both marker lines included).
// A begin with no matching end is left untouched. Returns true if anything was removed.
bool  StripAllFx(std::string& shaderText);
// StripAllFx, then inserts the blocks for s. Returns false (text unchanged) if there is no
// "shader_body" followed by a '{' with a matching '}' (braces inside // and /* */ comments ignored).
bool  ApplyFx(std::string& shaderText, const FxState& s);
const char* FxName(FxId id);                     // marker name: "mirror-x", "kaleido", ...
```

The two value blocks (Brightness/Contrast) are marked `SURI-FX:brightness` and `SURI-FX:contrast`. The value is written with `%.2f`.

- [x] **Step 1: Write failing tests** in `test_shader_fx.cpp`, with `const char* kBody = "sampler sampler_x;\nshader_body\n{\n    ret = tex2D(sampler_main, uv).xyz;\n}\n";`:

```cpp
TEST(TestShaderFx_ToggleTwiceRestores) {
    std::string t = kBody; FxState s;
    ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s)); CHECK(t != kBody);
    ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s)); CHECK(t == kBody);
}
TEST(TestShaderFx_NoShaderBody_ReturnsFalse) {
    std::string t = "float4 x;"; FxState s; ToggleFx(s, FxId::Mono);
    CHECK(!ApplyFx(t, s)); CHECK(t == "float4 x;");
}
TEST(TestShaderFx_TwoEffectsIndependent) {
    std::string t = kBody; FxState s;
    ToggleFx(s, FxId::MirrorX); ToggleFx(s, FxId::Invert); ApplyFx(t, s);
    CHECK(t.find("SURI-FX:mirror-x begin") < t.find("ret = tex2D"));   // geometry before user code
    CHECK(t.find("SURI-FX:invert begin")   > t.find("ret = tex2D"));   // colour after it
    ToggleFx(s, FxId::MirrorX); ApplyFx(t, s);
    CHECK(t.find("mirror-x") == std::string::npos); CHECK(t.find("SURI-FX:invert begin") != std::string::npos);
}
TEST(TestShaderFx_ApplyIsIdempotent) {            // Review Focus 2
    std::string t = kBody; FxState s; ToggleFx(s, FxId::Kaleido); s.brightness = 1.5f;
    ApplyFx(t, s); std::string once = t; ApplyFx(t, s); CHECK(t == once);
}
TEST(TestShaderFx_StripAllBeforeSave) {           // Review Focus 1
    std::string t = kBody; FxState s; ToggleFx(s, FxId::Ripple); ToggleFx(s, FxId::Posterize); s.contrast = 0.4f;
    ApplyFx(t, s); CHECK(StripAllFx(t)); CHECK(t == kBody); CHECK(!StripAllFx(t));
}
TEST(TestShaderFx_BrightnessClamp) {
    CHECK(StepLevel(1.0f, +1) == 1.1f); CHECK(StepLevel(0.2f, -1) == 0.2f);
    CHECK(StepLevel(3.0f, +1) == 3.0f); CHECK(StepLevel(0.95f, -1) == 0.9f);
}
TEST(TestShaderFx_BraceInComment) {               // Review Focus 3
    std::string t = "shader_body\n{\n    // } not the end\n    ret = 1;\n}\n";
    FxState s; ToggleFx(s, FxId::Invert); CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:invert begin") > t.find("ret = 1;"));
}
TEST(TestShaderFx_BrokenMarkerLeftAlone) {        // Review Focus 3
    std::string t = std::string(kBody) + "// SURI-FX:invert begin\n";
    CHECK(!StripAllFx(t)); CHECK(t.find("SURI-FX:invert begin") != std::string::npos);
}
TEST(TestShaderFx_FunctionsBeforeBody) {
    std::string t = "float3 f(float3 c) { return c; }\nshader_body\n{\n    ret = f(1);\n}\n";
    FxState s; ToggleFx(s, FxId::SwapAxes); CHECK(ApplyFx(t, s));
    CHECK(t.find("SURI-FX:swap-axes begin") > t.find("shader_body"));
}
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure (`shader_fx.h` doesn't exist).
- [x] **Step 3:** implement `shader_fx.cpp` according to the Interfaces and the table above. Find the first `shader_body` that is not inside a comment, then the `{` after it and the matching `}`, skipping `//…\n` and `/*…*/` when counting braces. Each block is written on separate lines: `    // SURI-FX:<name> begin\n    <hlsl>\n    // SURI-FX:<name> end\n`.
- [x] **Step 4:** `.\build.ps1 -Test`. Expected: all `TestShaderFx_*` pass and `0 failed`.
- [x] **Step 5:** commit `feat(fx): shader effect injection core`.

### Task 2: Hooking into CPlugin (injection, compile, save)

**Files:**
- Modify: `code/vis_milk2/state.h/.cpp` (`m_nFxOrigCompPSVersion`), `code/vis_milk2/plugin.h` (fields and methods), `code/vis_milk2/plugin.cpp` (`LoadPreset`, `LoadPresetTick`, `SavePresetAs`, `OnUserEditedCompShaders`)

**Interfaces:**
- Consumes: Task 1.
- Produces (in `CPlugin`):
  - `FxState m_fx;`
  - In `CState` (`state.h`), not in `CPlugin`, so it travels with the state in the `m_pState`/`m_pOldState`/`m_pNewState` swaps: `int m_nFxOrigCompPSVersion;` the comp version before an effect upgrade, -1 if not upgraded. Reset to -1 in `CState::Default()` and in `Import`.
  - `void ApplyFxToState(CState* pState);` upgrades if needed (only when `m_fx.Any()`), and runs `ApplyFx` on `m_szCompShadersText` through a `std::string`. If the result is longer than `MAX_BIGSTRING_LEN-1`, the text is left without effects and an `ERR` is logged.
  - `void RefreshFx();` calls `ApplyFxToState(m_pState)` and then `OnUserEditedCompShaders(0,0)` (recompiling the comp, with the existing fallback).

- [x] **Step 1:** call `ApplyFxToState` right after every `Import` in `LoadPreset` (both paths, to `m_pState` and to `m_pNewState`) and before `LoadShaders`. Not needed in `LoadPresetTick`: the Import has already been injected.
- [x] **Step 2:** `SavePresetAs`: before `Export`, copy `m_szCompShadersText` into a temporary buffer, run `StripAllFx` on it, and if the state was upgraded because of an effect, temporarily restore `m_nCompPSVersion`, `m_nMinPSVersion` and `m_nMaxPSVersion` to their original values. After `Export`, restore the text and the versions. (`Export` writes the comp only when `m_nCompPSVersion >= MD2_PS_2_0`, so an original PS 0 preset is saved without a comp, as before.)
- [x] **Step 3:** `OnUserEditedCompShaders` (manual editing in M) does not re-inject. The user sees the blocks and can delete them. Add one comment explaining this.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test` pass. Smoke test: run it; a `LOG` in `ApplyFxToState` shows `fx: applied mask=0x… len=…` when an effect is turned on via a temporary call in the debugger or after Task 3.
- [x] **Step 5:** commit `feat(fx): apply effects on preset load and strip on save`.

### Task 3: Effects commands and menu

**Files:**
- Modify: `features/commands.h/.cpp` (Cmd, `AppSnapshot`, `QueryState`), `features/context_menu.cpp` (submenu), `plugin.cpp` (`DispatchCommand`, `GetAppSnapshot`), `plugin.rc`, `resource.h`, `code/tests/test_commands.cpp`

**Interfaces:**
- Produces: in `Cmd`, before `HelpOverlay`: `FxMirrorX, FxMirrorY, FxKaleido, FxRipple, FxZoomIn, FxSwapAxes, FxTunnel, FxInvert, FxMono, FxPosterize, FxHueSpin, FxBrightnessUp, FxBrightnessDown, FxContrastUp, FxContrastDown, FxClearAll`.
  `Cmd FxCmd(FxId id);` and `bool CmdToFx(Cmd c, FxId* out);` to map between the two enums.
  In `AppSnapshot`: `bool shadersAvailable = false; unsigned fxMask = 0; float fxBrightness = 1, fxContrast = 1;`
- Menu: an "Effects" submenu after "View": the geometry group, a separator, the colour group, a separator, the four ± items, a separator, Clear effects. Labels (English, written from scratch): `Mirror left/right`, `Mirror top/bottom`, `Kaleidoscope`, `Ripple`, `Zoom in`, `Swap axes`, `Tunnel`, `Invert colours`, `Black and white`, `Posterize`, `Colour spin`, `Brighter`, `Darker`, `More contrast`, `Less contrast`, `Clear effects`.

- [x] **Step 1: Failing tests** in `test_commands.cpp`:

```cpp
TEST(TestCommands_FxMapping) {
    for (unsigned i = 0; i < (unsigned)FxId::Count; i++) {
        FxId back; CHECK(CmdToFx(FxCmd((FxId)i), &back) && back == (FxId)i);
    }
    FxId x; CHECK(!CmdToFx(Cmd::PresetNext, &x));
}
TEST(TestCommands_FxState) {
    AppSnapshot s; s.shadersAvailable = true; s.fxMask = 1u << (unsigned)FxId::Mono; s.fxBrightness = 3.0f;
    CHECK(QueryState(Cmd::FxMono, s).checked);
    CHECK(!QueryState(Cmd::FxInvert, s).checked);
    CHECK(!QueryState(Cmd::FxBrightnessUp, s).enabled);    // at the 3.0 ceiling
    CHECK(QueryState(Cmd::FxClearAll, s).enabled);
    s.shadersAvailable = false;
    CHECK(!QueryState(Cmd::FxMono, s).enabled);
}
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement. `DispatchCommand`: toggle or `StepLevel`, then `RefreshFx()` and a short message with `AddError(…, 2.0f, ERR_NOTIFY, false)` showing the effect name and on/off or the value. `FxClearAll` resets `m_fx` and calls `RefreshFx()`. `FxClearAll` is disabled when `!fx.Any()`.
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`, including `TestMenu_EveryCommandExactlyOnce` and `TestMenu_LabelsExist`.
- [x] **Step 5: Manual test (user):** each effect toggles on and off from the menu ✔; two effects together ✔; switching presets (H, Space with blend) keeps the effect ✔; an old MD1 preset (PS 0) with Invert ✔; Save As with an active effect, then opening the file in an editor: no `SURI-FX` and `PSVERSION_COMP` unchanged ✔; Brighter all the way, and the item is disabled at 3.0 ✔.
- [x] **Step 6:** commit `feat(fx): effects menu`. **Stop for phase 5 approval.**

---

# Phase 6: Friendlier screens

### Task 4: Survey and mockups (no code changes)

- [x] **Step 1:** run the build and take screenshots (PowerShell + `CopyFromScreen` into a scratchpad folder): the F1 help screen, the M menu, the L preset browser, the preset name message, FPS, rating, and an error message. In an 800×800 window, in fullscreen at 1080p, and at 150% DPI (Settings > Display; the user switches and confirms).
- [x] **Step 2:** write a list of problems for each screen: readability, density, contrast against a bright background, size at high DPI, keys missing from the help (compare with `KeyBindings()`), and truncated text.
- [x] **Step 3:** publish an Artifact (one private HTML page) with the screenshots, the problem list, and 3 design directions mocking the same screens: **A. "Minimal overlay"** (text on a thin dark strip, everything in a corner), **B. "Cards"** (rounded semi-transparent panels, help paged by topic), **C. "Bottom bar"** (toast and details in one bar at the bottom, fullscreen help in two columns). For each direction: the same content, at 1080p and 4K (scaled mockup).
- [x] **Step 4: Stop. The user picks a direction** (or a combination). Update Tasks 5–6 in this plan according to the choice (which colours, margins, corners, placement), and ask for approval of the change.

### Task 5: Direction-independent infrastructure (DPI, help pages, toast)

**Files:**
- Create: `code/vis_milk2/features/ui_scale.h/.cpp`, `code/tests/test_ui_scale.cpp`
- Modify: `pluginshell.cpp` (`AllocateFonts`, `RenderBuiltInTextMsgs`, `ToggleHelp`), `Milkdrop2PcmVisualizer.cpp` (`WM_DPICHANGED`), `plugin.rc` (the help text `IDR_TEXT2`)

**Interfaces:**
- Produces (`ui_scale.h`):

```cpp
int  ScaleFontPixels(int basePixels, UINT dpi);          // MulDiv(base, dpi, 96), minimum = base for dpi < 96
std::vector<std::wstring> SplitHelpPages(const std::wstring& text);   // pages separated by a line "\f" (form feed)
struct Toast { std::wstring text; float startTime; float duration; };
class ToastQueue {                                       // newest on top, at most 3 visible
public:
    void Push(const std::wstring& text, float now, float duration);
    std::vector<const Toast*> Visible(float now) const;  // drops expired; fade handled by the caller
};
```

- [x] **Step 1: Failing tests:** `TestUiScale_FontPixels` (`ScaleFontPixels(14,96)==14`, `(14,144)==21`, `(14,192)==28`, `(14,72)==14`), `TestUiScale_HelpPages` (three sections with `\f` give 3 pages, text without `\f` gives one page, a trailing `\f` line does not create an empty page), `TestUiScale_ToastQueue` (4 Push calls, `Visible` returns 3 with the newest first; after `duration` it returns 0).
- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement `ui_scale.cpp`.
- [x] **Step 4:** hook up: `AllocateFonts` uses `ScaleFontPixels(m_fontinfo[i].nSize, GetDpiForWindow(hwnd))`. `WM_DPICHANGED` calls `CleanUpFonts`+`AllocateFonts` and moves the window to `lParam` (the suggested RECT). F1 opens page 1, and PageDown/PageUp switch pages while the help is open (keys with no other use in this state). The help text in `plugin.rc` is rewritten, split into topics (Presets, View, Effects, Editing, Desktop), and built so that every `Cmd` with a key appears in it.
- [x] **Step 5:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 6:** commit `feat(ui): DPI-scaled fonts, paged help, toast queue`.

### Task 6: The chosen design

**Files:** `pluginshell.cpp` (`DrawDarkTranslucentBox` and drawing the messages), `textmgr.cpp`, `menu.cpp`, `plugin.cpp` (`AddError` moves to `ToastQueue` for `ERR_NOTIFY`), `context_menu.cpp` (Help > Keyboard help already exists; make sure it opens page 1).

- [x] **Step 1:** implement **direction B, "Cards"** (the user's choice, 2026-10-25; mockup: https://claude.ai/artifact/5pR9NTvhHV7b3AtF2q3ia3). All sizes are px at 96 DPI and go through `ScaleFontPixels`/the same ratio:
  - **Drawing:** a new function `CPluginShell::DrawRoundedPanel(const RECT* r, int radius, DWORD fill, DWORD border)`, with the same approach as `DrawDarkTranslucentBox` (`DrawPrimitiveUP` with alpha blend): a central rectangle and 4 corners as an 8-segment triangle fan, and a 1px border as a line strip. A `radius` larger than half the height = a pill.
  - **Colours:** fill `0xB8141022` (info card), `0xD1141022` (toast), `0xDB141022` (help, M menu, L browser); border `0x24FFFFFF`; primary text white, secondary `0xFFC9C4DA`; highlight (active tab, selected row) `0xFFB9A7F0` with text `0xFF1A1430`; the selected row in lists = a full bar in `0x40B9A7F0` behind the text, not just colour.
  - **Fonts:** Segoe UI for text (instead of Courier), Consolas for keys. System fonts, no new dependency.
  - **Info card (F4/F5/F6):** top-right corner, margin 28, padding 14×20, radius 14. Line 1: the preset name (22px); line 2 at 17px in the secondary colour: rating as `★` stars in `0xFFF3C969`, FPS, and "locked" when the preset is locked.
  - **Toast:** a pill at the top center, margin 28, padding 10×26, text 24px, a 10-diameter status dot on the left (green `0xFF8EE6A8` for a message, red `0xFFFF8A7A` for an error). Up to 3 stacked downward, spacing 10, fade-out in the last 0.5 seconds. `ERR_NOTIFY` → 2.5s duration; errors → 6s duration.
  - **Help (F1):** a centered card, width min(980, 90% of the screen), padding 30×38, radius 18. A tab row from `SplitHelpPages` (Presets, View, Effects, Editing, Desktop); the active one filled with the highlight. Below: a two-column grid, each row a "key" in a keycap (fill `0x1AFFFFFF`, radius 6, Consolas 18px) and a 21px description. A bottom row at 16px in the secondary colour: `PgUp / PgDn: topics` on the left, `F1 or Esc: close` on the right.
  - **M menu and L browser:** the same card (padding 20, radius 14) behind all the text, as tall as all the rows; the selected row with a highlight bar. The browser shows only the folder name (not the full path) in the card title, and "page N of M" on a bottom row inside the card. M's navigation line moves to a bottom row inside the card.
- [x] **Step 2:** `.\build.ps1` and `.\build.ps1 -Test`.
- [x] **Step 3: Manual test (user):** the same screenshots as Task 4 Step 1, before and after, in the same Artifact. At 1080p and at 150% DPI, and moving the window between two monitors with different DPI (Review Focus 5).
- [x] **Step 4:** commit `feat(ui): friendlier on-screen text`. **Stop for phase 6 approval.**

---

# Phase 7: Desktop mode and tray icon

**Decisions:**
- **Global shortcut:** **Ctrl+Alt+Shift+D** (`RegisterHotKey(hwnd, 1, MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT, 'D')`). If registration fails (taken), an `ERR` is logged and the command is available from the menu and the tray only.
- **Finding the host:** after `SendMessageTimeoutW(progman, 0x052C, …)`, search in two forms, because the structure changed in Windows 11 24H2: (a) a top-level WorkerW that is the sibling after the window containing `SHELLDLL_DefView`; (b) a WorkerW that is a child of `Progman`. If neither is found: the message "Desktop mode is not available" and stay in normal mode.
- **Styles:** `WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS` and `WS_EX_NOACTIVATE`, without `WS_EX_LAYERED` (windowed D3D9 in a layered window is not required when the window sits below the DefView).
- **Size:** the whole virtual screen, in the host's coordinates (`MapWindowPoints`), followed by a D3D Reset exactly as in `ToggleStretch`.
- **Entry is allowed only from windowed mode:** if the mode is fullscreen or stretch, leave it first.
- **On exit:** `SetParent(hwnd, NULL)`, restore the saved style/exstyle/rect, D3D Reset, remove the tray icon, and `SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, <the path from SPI_GETDESKWALLPAPER>, SPIF_SENDCHANGE)` to redraw the wallpaper.
- **The tray icon** exists only during Desktop mode (because then there is no window to right-click). Right-clicking it opens a short menu; a double-click leaves Desktop mode.

### Task 7: Experiment – does our window survive when Explorer goes down

This determines the implementation of Review Focus 4, so it comes first.

- [x] **Step 1:** a temporary experiment tool in the scratchpad (not in the repo): a small Win32 program that creates a window, attaches it to the WorkerW according to the decisions above, and prints `IsWindow(hwnd)` and `GetParent(hwnd)` every 250ms. Run it, then `taskkill /f /im explorer.exe` and `start explorer.exe`.
- [x] **Step 2:** record the result in this plan. **If the window survives** (left with no parent or an invalid parent): on `TaskbarCreated`, look for a host again and attach. **If the window is destroyed:** add to `CDesktopMode` a 250ms timer that checks `IsWindow(host)` and does `SetParent(hwnd, NULL)` + `ShowWindow(SW_HIDE)` as soon as the host is invalid, and also in `StaticWndProc`'s `WM_DESTROY`, when Desktop mode is active and we didn't ask to exit, log an `ERR` and save `DesktopModeOnStart=1` in the ini, so the next launch returns to the mode. The result and the chosen branch are shown to the user before Task 8.

### Task 8: The pure part (host selection, reattach decision, tray menu)

**Files:**
- Create: `code/vis_milk2/features/desktop_mode.h/.cpp`, `code/tests/test_desktop_mode.cpp`
- Modify: `features/commands.h/.cpp` (`Cmd::ViewDesktopMode`, `AppSnapshot::desktopMode`, `desktopAvailable`), `context_menu.cpp` (View > "Run as desktop background", and a label with the shortcut), `plugin.rc`, `resource.h`, the vcxproj files

**Interfaces:**
- Produces (`desktop_mode.h`):

```cpp
struct TopWindowInfo { HWND hwnd; std::wstring className; bool hasDefView; };
// Given the top-level windows in EnumWindows order and Progman's WorkerW child (or NULL),
// returns the window to parent into, or NULL.
HWND PickWallpaperHost(const std::vector<TopWindowInfo>& top, HWND progmanWorkerChild);

enum class DesktopEvent { None, HostLost, TaskbarCreated, SessionLocked, SessionUnlocked, DisplayChanged };
enum class DesktopAction { None, Detach, Reattach, Resize };
DesktopAction DecideDesktopAction(bool active, bool hostValid, DesktopEvent e);

const std::vector<MenuNode>& TrayMenuLayout();   // ViewDesktopMode, PresetNext, PresetRandom, PresetLockToggle,
                                                 // Effects submenu (same nodes as the main menu), FxClearAll, AppQuit
```

- [x] **Step 1: Failing tests:**

```cpp
TEST(TestDesktop_PickHost_Classic) {   // Win10: WorkerW sibling after the window holding DefView
    HWND a = (HWND)1, b = (HWND)2, c = (HWND)3;
    std::vector<TopWindowInfo> top = { {a, L"WorkerW", true}, {b, L"WorkerW", false}, {c, L"Progman", false} };
    CHECK(PickWallpaperHost(top, NULL) == b);
}
TEST(TestDesktop_PickHost_ProgmanChild) {   // Win11 24H2
    std::vector<TopWindowInfo> top = { {(HWND)3, L"Progman", true} };
    CHECK(PickWallpaperHost(top, (HWND)9) == (HWND)9);
}
TEST(TestDesktop_PickHost_None) {
    std::vector<TopWindowInfo> top = { {(HWND)3, L"Progman", true} };
    CHECK(PickWallpaperHost(top, NULL) == NULL);
}
TEST(TestDesktop_ReattachDecision) {   // Review Focus 4
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::HostLost)       == DesktopAction::Detach);
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::TaskbarCreated) == DesktopAction::Reattach);
    CHECK(DecideDesktopAction(true,  true,  DesktopEvent::DisplayChanged) == DesktopAction::Resize);
    CHECK(DecideDesktopAction(true,  true,  DesktopEvent::SessionUnlocked)== DesktopAction::None);
    CHECK(DecideDesktopAction(true,  false, DesktopEvent::SessionUnlocked)== DesktopAction::Reattach);
    CHECK(DecideDesktopAction(false, false, DesktopEvent::TaskbarCreated) == DesktopAction::None);
}
TEST(TestDesktop_TrayMenuCommandsHaveLabels) {
    // every Item in TrayMenuLayout() has CmdLabelId != 0, and ViewDesktopMode is first
}
```

- [x] **Step 2:** `.\build.ps1 -Test`. Expected: a compile failure.
- [x] **Step 3:** implement. `ViewDesktopMode` is disabled when `!desktopAvailable` (`desktopAvailable` means Progman exists) and checked when `desktopMode`. During Desktop mode, `ViewFullscreen` and `ViewStretch` are disabled.
- [x] **Step 4:** `.\build.ps1 -Test`. Expected: `0 failed`.
- [x] **Step 5:** commit `feat(desktop): host selection, reattach rules, tray menu layout`.

### Task 9: `CDesktopMode` and the tray icon (Win32)

**Files:**
- Create: `code/vis_milk2/features/desktop_mode_win32.cpp`
- Modify: `desktop_mode.h` (classes), `Milkdrop2PcmVisualizer.cpp` (`HandleWindowCommand`, `StaticWndProc`, `BuildAppSnapshot`, exit), `plugin.vcxproj` (`shell32` is already linked; make sure `wtsapi32.lib` is linked for `WTSRegisterSessionNotification`, a system library and not a new dependency)

**Interfaces:**
- Consumes: Task 8, `TrayMenuLayout`, `BuildContextMenu` (add an overload `HMENU BuildMenu(const std::vector<MenuNode>&, const AppSnapshot&)` in `context_menu_win32.cpp`, and `BuildContextMenu` calls it).
- Produces:

```cpp
typedef void (*ResizeBackBufferFn)(int width, int height);   // InitD3d's d3dPp + Reset, in Milkdrop2PcmVisualizer.cpp
class CDesktopMode {
public:
    bool Enter(HWND hwnd, ResizeBackBufferFn resize);   // false + message if no host
    void Leave();                                       // full restore, safe to call twice
    bool IsActive() const;
    void OnEvent(DesktopEvent e);                       // runs DecideDesktopAction
    HWND Host() const;
};
class CTrayIcon {
public:
    static const UINT kMsg = WM_APP + 0x51;
    bool Add(HWND hwnd, HICON icon, const wchar_t* tip);
    void Remove();
    void ReAdd();                                       // after TaskbarCreated
};
```

- [x] **Step 1:** `Milkdrop2PcmVisualizer.cpp`: extract the repeated Reset into `static void ResizeBackBuffer(int w, int h)` and use it in `ToggleFullScreen`/`ToggleStretch` too (same behavior).
- [x] **Step 2:** implement `CDesktopMode` and `CTrayIcon` according to the decisions above and the result of Task 7. Every step is logged with `LOG` (`desktop: host=%p`, `desktop: leave`, `desktop: reattach`).
- [x] **Step 3:** hookup in `StaticWndProc`: `RegisterHotKey` after the window is created and `WM_HOTKEY` → `Cmd::ViewDesktopMode`; `RegisterWindowMessageW(L"TaskbarCreated")` → `OnEvent(TaskbarCreated)` and `ReAdd`; `WM_WTSSESSION_CHANGE` → lock/unlock; `WM_DISPLAYCHANGE` → `DisplayChanged`; `CTrayIcon::kMsg` with `WM_RBUTTONUP` → the tray menu (the same `TPM_RETURNCMD` and `RunCommand` path), `WM_LBUTTONDBLCLK` → leave the mode. `WM_CLOSE`/`WM_DESTROY` and `AppQuit` call `Leave()` before exiting, and so does `CreateWindowAndRun` after the loop (a safety net).
- [x] **Step 4:** `.\build.ps1` and `.\build.ps1 -Test`. Expected: no errors, `0 failed`.
- [x] **Step 5:** commit `feat(desktop): desktop background mode and tray icon`.

### Task 10: Manual test and documents

- [x] **Step 1: Manual test (user):** enter from the menu ✔ and from Ctrl+Alt+Shift+D ✔; the desktop icons are visible and clickable above the visualization ✔; the tray menu works ✔; leaving from the tray, from the shortcut and on closing the program, and the wallpaper returns ✔; `taskkill /f /im explorer.exe` then `start explorer.exe`, and the mode returns within a few seconds ✔; Win+L and unlock ✔; two monitors, with the visualization spanning both ✔; listening to audio the whole time ✔.
- [x] **Step 2:** update `docs/PROJECT_STATUS.md` (phases 5–7), the status table in the roadmap, and `user-guide.html` (an Effects section and a Desktop section, text written from scratch).
- [x] **Step 3:** commit `docs: phase 5-7 status`. **Stop for phase 7 approval.**
