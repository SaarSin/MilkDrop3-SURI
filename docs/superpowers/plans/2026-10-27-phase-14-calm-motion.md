# Phases 14–15: Calm motion (speed, audio sensitivity, anti-flicker) and the calm meter – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** wild presets can be calmed down without editing their code. Three settings **per preset**: **Speed** (how fast the preset moves), **Sensitivity** (how strongly it reacts to the music, shown on screen and changed with Ctrl+[+] / Ctrl+[-]) and **Anti-flicker** (Off / Soft / Strong). Like the preset rating, each preset keeps its own values: presets that already work well stay at the defaults, and only the wild ones are tuned.

**Architecture:** the preset code is never changed. MilkDrop changes what it gives the preset, what it takes back, and how the result is shown:
1. **Steps:** the preset advances (per-frame code, warp feedback, shapes) at most 60 times per second, whatever the display rate. The display frames in between show the last image again. Presets see `fps` = steps per second.
2. **Speed** scales, inside each step: the motion the preset asks for (zoom, rot, warp, dx, dy, sx, sy, also per vertex), the clock the preset sees (`time`, shader time, warp animation, built-in waves, the hue cycle), and how fast the waveform and spectrum it draws may change.
3. **Sensitivity** reshapes bass/mid/treb (and `_att`) around 1.0, and makes the drawn waveform and spectrum smaller below 100%.
4. **Anti-flicker** smooths the audio values, limits how fast colours and shape values may change, and blends each new image with the previous one (persistence).

All maths is in a new pure module `features/calm.*` (no D3D), unit-tested like the other features. `plugin.cpp`, `milkdropfs.cpp` and `state.cpp` only call it.

**Per preset, per computer:** the values are written into the preset file itself, the same way `+` / `-` writes `fRating` today (`SetCurrentPresetRating`, `plugin.cpp` 8912). The file that changes is the copy in the program folder (`build\Release\plugins\Milkdrop2\presets\...`), which is not in git (`build/` is ignored). The sources in `content/presets` are never changed, so the values stay on this computer and are not committed.

**Tech Stack:** C++ (MSVC v143, Win32 x86), Direct3D 9, the existing `mini_test.h` unit tests, `build.ps1`.

**Spec:** the user's requests (2026-10-27/28): "a mechanism that restrains the display speed of presets; some go wild, flicker and move too fast. Also add a parameter that shows the sensitivity, changeable with Ctrl + [+] or [-]"; "some presets work well, so these settings are per preset, like its rating; that way they stay on the computer and don't go to git". Roadmap rules: [roadmap](2026-10-24-milkdrop-suri-roadmap.md), Global Constraints. Design validated by the spike (see the appendix).

**Phase 15 (Tasks 8–10, after phase 14 is approved):** a calm meter that measures the image on screen and **suggests** values for the current preset. **It never changes anything by itself in this phase:** the suggestion is only shown, and it is applied only when the user presses Ctrl+7. Running it automatically on every preset load is a later decision, not part of this plan.

## Global Constraints

- **Approval gate:** execute only after the user approves this plan. The manual test at the end is run **by the user**. Agents may only launch/close MilkDrop3, post messages to its own window and capture the screen; no clicks, no other windows.
- **Base:** branch `suri/phase-14` in a separate worktree, starting from `main`. Before any whole-tree operation, run `ListAgents` and `git status`. No stash, reset or checkout on the shared tree. The spike branch `suri/spike-calm` is a reference only: re-implement, do not merge it.
- **Defaults keep today's look:** a preset with no calm keys runs at Speed 100%, Sensitivity 100%, Anti-flicker Off. At 60 fps and the defaults, every value handed to presets and read back is bit-identical to today's (all scaling is skipped when the factor is exactly 1).
- **Ranges and steps:** Speed 10%–150%, step 5%. Sensitivity 0%–200%, step 10%. Values are rounded to the step after every change (no 0.30000001).
- **Step rate:** at most 60 preset steps per second (`kRefFps`). A display slower than 60 fps runs one step per frame (no catch-up).
- **Not scaled:** `decay` (scaling it with Speed made images brighter in the spike), the preset `frame` counter, `q` variables, hard-cut detection (keeps the raw audio), preset blending, the auto-change timer, toasts.
- **Keys (all act on the current preset):** Ctrl+[+] / Ctrl+[-] = Sensitivity up / down (`VK_OEM_PLUS`/`VK_OEM_MINUS` and numpad `VK_ADD`/`VK_SUBTRACT`). Ctrl+Shift+[+] / Ctrl+Shift+[-] = Speed up / down. Ctrl+0 = this preset back to the defaults. Plain `+` / `-` keep rating the preset. No Ctrl+letter (it also produces a WM_CHAR control code).
- **Persistence, per preset:** in the current preset file (`m_szCurrentPresetFile`), section `[preset00]`, next to `fRating`: `fCalmSpeed`, `fCalmSensitivity`, `nCalmAntiFlicker`. Written immediately on every change. A key whose value is the default is **deleted**, so an untuned preset file is unchanged. Read in `CState::Import` and clamped. No global ini setting.
- **Saving a preset (S)** writes the non-default calm keys into the new file.
- **No preset file** (nothing loaded yet): the calm commands are disabled.
- **The file can't be written:** the value still applies until the preset changes, and an error toast says `Couldn't save to the preset file`.
- **Messages:** every change shows a 2.5 s toast (`AddError(..., ERR_NOTIFY)`), for example `Sensitivity 80%`, `Speed 15%`, `Anti-flicker: Soft`.
- Each Task ends when `.\build.ps1 -Test` prints `0 failed` and `.\build.ps1` builds with no new warnings.
- One commit per Task, with a message ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No mention of other MilkDrop releases in code, docs or commits.

## Review Focus

1. **Display faster than 60 fps** (F2 at 144/240/unlimited): steps stay at 60/s, hold frames show the last image, and presets see `fps` = 60. At 144 Hz the step count over one second is 60 ± 1. Tested in `TestCalm_StepDue` (Task 1). Hold frames are checked in the manual test.
2. **Speed changed in the middle of a preset:** presets using `time` must not jump (the animation clock adds up `dt × speed`). Tested in `TestCalm_AnimClockNoJump` (Task 1).
3. **Preset file round trip:** a value typed by hand (`fCalmSensitivity=50`, `-3`, `nan`, `nCalmAntiFlicker=7`) is clamped on load. A preset without keys gets the defaults even if the previous preset was tuned (Import resets them before reading). Back at the defaults, the keys are removed. Tested in `TestCalm_ClampSettings` and `TestCalm_PresetKeys` (Task 1). The Import reset is checked in the manual test.
4. **Device reset** (window resize, fullscreen toggle, desktop mode): the saved-image texture is released in `CleanUpMyDX9Stuff` and recreated on the next step; until then every display frame runs a step (no hold frame without an image). Checked in the manual test (Alt+Enter twice while Speed is 15%).
5. **Changing from a tuned preset to an untuned one:** speed and sensitivity slide from the old values to the new ones during the blend (`BlendCalm`); Anti-flicker uses the stronger of the two until the blend ends. The colour limiter of a newly loaded state starts fresh. Tested in `TestCalm_BlendCalm` and `TestCalm_SlewFirstSamplePassesThrough` (Task 1).

---

## File structure

| File | Responsibility |
|---|---|
| `code/vis_milk2/features/calm.h/.cpp` (new) | Pure maths: settings, step/clamp, labels, preset-file keys, blend between presets, step scheduling, motion scale, audio shaping and smoothing, waveform follow rate, slew limiter, persistence amount, animation clock |
| `code/tests/test_calm.cpp` (new) | Unit tests for `calm.*` |
| `code/vis_milk2/state.h/.cpp` | `CalmSettings m_calm` and `CalmSlew m_calmSlew` per `CState`; read in `Import`, written in `Export` |
| `code/vis_milk2/plugin.h/.cpp` | Effective settings per frame, step/hold decision, audio and waveform shaping, writing the keys to the preset file, `DispatchCommand`, key routing, info card line, D3D resource cleanup |
| `code/vis_milk2/milkdropfs.cpp` | Animation clock, shaped audio, step fps, motion scale, colour/shape limiter, persistence and hold-frame drawing |
| `code/vis_milk2/features/commands.*`, `context_menu.cpp`, `help_text.cpp` | New `Cmd`s, Ctrl key scopes, labels, "Motion" submenu, F1 lines |
| `plugin.rc`, `resource.h` | New strings |
| `plugin.vcxproj(.filters)`, `unit_tests.vcxproj` | Add `calm.cpp` / `test_calm.cpp` |
| `CHANGES.md`, `docs/PROJECT_STATUS.md`, roadmap | Phase 14 entry |

---

### Task 1: `features/calm.*` – the pure maths

**Files:**
- Create: `code/vis_milk2/features/calm.h`, `code/vis_milk2/features/calm.cpp`, `code/tests/test_calm.cpp`
- Modify: `code/tests/unit_tests.vcxproj`, `code/vis_milk2/plugin.vcxproj`, `code/vis_milk2/plugin.vcxproj.filters`

**Interfaces – Produces:**
```cpp
enum class AntiFlicker : int { Off = 0, Soft = 1, Strong = 2 };
struct CalmSettings { float speed = 1.0f; float sensitivity = 1.0f; AntiFlicker antiFlicker = AntiFlicker::Off; };

const float kSpeedMin = 0.10f, kSpeedMax = 1.5f, kSpeedStep = 0.05f;
const float kSensMin  = 0.0f,  kSensMax  = 2.0f, kSensStep  = 0.10f;
const float kRefFps   = 60.0f;

float StepSpeed(float cur, int dir);                 // dir = +1 / -1; clamp, round to the step
float StepSensitivity(float cur, int dir);
CalmSettings ClampSettings(float speed, float sens, int antiFlicker); // NaN -> default, else clamp
AntiFlicker NextAntiFlicker(AntiFlicker a);          // Off -> Soft -> Strong -> Off
bool IsDefault(const CalmSettings& c);

std::wstring SpeedLabel(float speed);                // L"Speed 15%"
std::wstring SensitivityLabel(float sens);           // L"Sensitivity 80%"
std::wstring AntiFlickerLabel(AntiFlicker a);        // L"Anti-flicker: Soft"

struct CalmKey { const wchar_t* name; std::wstring value; };   // empty value = delete the key
std::vector<CalmKey> CalmPresetKeys(const CalmSettings& c);     // fCalmSpeed, fCalmSensitivity, nCalmAntiFlicker; floats "%.2f"
CalmSettings BlendCalm(const CalmSettings& oldC, const CalmSettings& newC, float mix); // lerp speed/sens; stronger AF until mix >= 1

bool  StepDue(float* accum, float dtSec);            // accum += dt*60; step when >= 1; accum capped at 1 after a step
float StepFps(float displayFps);                     // min(displayFps, 60), at least 1; NaN -> 60

struct CalmMotion { float zoom, rot, warp, dx, dy, sx, sy; };
void  ScaleMotion(CalmMotion* m, float k);           // k == 1: untouched; else zoom/sx/sy ^ k, rot/warp/dx/dy * k

float ShapeAudio(float rel, float sens);             // max(0, 1 + (rel - 1) * sens)
float WaveFollow(float speed, float dtSec);          // speed >= 1: 1; else 1 - (1 - speed^2)^(60*dt)
float WaveAmplitude(float sens);                     // min(1, sens)

class AudioSmoother { public: float Step(float x, float dtSec, AntiFlicker a); void Reset(); };
// Off: passes through. Soft: attack 0.04 s, release 0.25 s. Strong: attack 0.10 s, release 0.60 s.

const int kSlewChannels = 64;                        // 0..13 main colours, 16 + shape*12 .. shape outputs
struct CalmSlew {                                    // POD: CState is memcpy'd
    float value[kSlewChannels]; bool valid[kSlewChannels];
    float Step(int channel, float target, float maxPerSec, float dtSec);   // first sample after Reset passes; maxPerSec <= 0: no limit
    void  Reset();
};
enum class SlewKind { Colour, Angle, Size };
float SlewRate(AntiFlicker a, SlewKind k);           // Off 0 (no limit). Soft: colour 4, angle 8, size 3 per s. Strong: 1.5, 3, 1

float PersistenceAmount(AntiFlicker a, float stepFps); // Off 0; Soft 0.5, Strong 0.8 at 60 steps/s, ^(60/stepFps)

class AnimClock { public: void Advance(float dtSec, float speed); float Now() const; void Reset(); };
// t += clamp(dt, 0, 0.25) * speed
```

- [ ] **Step 1: Write the failing tests** in `code/tests/test_calm.cpp` (`mini_test.h` style, helper `Near(a, b, eps = 1e-4f)`):
  - `TestCalm_Steps`: `StepSpeed(1.0f,+1)≈1.05`; 30 × down from 1.0 ends at `0.10`; `StepSpeed(1.5f,+1)==1.5f`; `StepSensitivity(1,-1)≈0.9`; from 0 down stays 0; 3 × down then 3 × up from 1.0 is exactly `1.0f`.
  - `TestCalm_ClampSettings`: `(50, -3, 7)` gives `{1.5, 0.0, Strong}`; `(NAN, NAN, -1)` gives `{1, 1, Off}`.
  - `TestCalm_Labels`: `SpeedLabel(0.15f)==L"Speed 15%"`, `SensitivityLabel(0.8f)==L"Sensitivity 80%"`, `AntiFlickerLabel(Soft)==L"Anti-flicker: Soft"`.
  - `TestCalm_PresetKeys`: defaults give 3 keys with empty values; `{0.15, 1, Soft}` gives `fCalmSpeed=L"0.15"`, empty sensitivity, `nCalmAntiFlicker=L"1"`.
  - `TestCalm_BlendCalm`: old `{0.4, 0.5, Strong}`, new defaults: mix 0 = old; mix 0.5 → speed 0.7, AF Strong; mix 1 = new.
  - `TestCalm_StepDue`: 144 frames of 1/144 s → 59–61 steps; 30 frames of 1/30 s → 30 steps; `StepFps(144)==60`, `StepFps(30)==30`.
  - `TestCalm_ScaleMotion`: k = 1 leaves every field bit-identical; k = 0.5: zoom 1.21 → 1.1, rot 0.2 → 0.1, sx 1 stays 1.
  - `TestCalm_Audio`: `ShapeAudio(1.7,1)≈1.7`, `(1.7,0)==1`, `(1.7,0.5)≈1.35`, `(0.1,2)==0`; `WaveFollow(1, 1/60.f)==1`; `WaveFollow(0.5f,1/60.f)≈0.25`; `WaveFollow(0.15f,1/60.f)≈0.0225`; `WaveAmplitude(1.5f)==1`.
  - `TestCalm_AudioSmoother`: Off passes through; Soft rises 1 → 3 past 2.9 within 0.2 s and falls 3 → 1 still above 1.3 after 0.2 s.
  - `TestCalm_SlewFirstSamplePassesThrough`: after `Reset`, `Step(0,0.9,1.5,1/60)==0.9`; next `Step(0,0,1.5,1/60)≈0.875`; `maxPerSec 0` returns the target.
  - `TestCalm_Persistence`: Off 0; Strong at 60 ≈ 0.8; Strong at 30 ≈ 0.64.
  - `TestCalm_AnimClockNoJump`: 60 × (1/60, 1) then 60 × (1/60, 0.5) ≈ 1.5; `Advance(5,1)` adds 0.25; `Advance(-1,1)` adds 0.
- [ ] **Step 2:** add the files to both projects and run `.\build.ps1 -Test`. Expected: fails to compile (no `calm.h`).
- [ ] **Step 3:** implement `calm.h` / `calm.cpp`. Header comment in the style of `commands.h`.
- [ ] **Step 4:** `.\build.ps1 -Test` → `0 failed`.
- [ ] **Step 5: Commit** `feat(calm): pure speed/sensitivity/anti-flicker maths`.

---

### Task 2: Speed and Sensitivity in the renderer

**Files:** `plugin.h`, `plugin.cpp` (frame start ~3280, `DoCustomSoundAnalysis` end ~9640, preset start lines 3290/7735/7808), `milkdropfs.cpp` (`LoadPerFrameEvallibVars` 389–397, `GetFrame()==0` block ~666, mesh loop before `fZoom2` ~1506, `fWarpTime` 1418, shape vars ~1975, wave vars ~2017, built-in wave angles and mod-wave alpha ~2338–2498, `ApplyShaderParams` ~3273–3298, hue shade ~3474–3476)

**Interfaces – Produces** (members of `CPlugin`):
- `CalmSettings m_calm;` – the **effective** values this frame (Task 4 fills it from the preset; until then defaults)
- `AnimClock m_animClock; float m_fAnimPresetStart;`
- `float m_presetImm[3], m_presetAtt[3]; AudioSmoother m_audioSmooth[6];`
- `float m_calmWave[2][576], m_calmSpec[2][NUM_FREQUENCIES];`
- `float m_fCalmStepFps;`

- [ ] **Step 1: Clock.** Advance `m_animClock` by `dt × m_calm.speed` where `dt` is computed (plugin.cpp ~3280). Reset it in the `GetFrame()==0` block. Set `m_fAnimPresetStart = m_animClock.Now()` next to every `m_fPresetStartTime = GetTime()`. Use `m_animClock.Now()` for `var_pf_time` (main, shapes, waves), shader `time`, `time_since_preset_start` (current state), `fWarpTime`, the built-in wave angles and the hue shade (`sinf(GetTime()*30.0f*...)`).
- [ ] **Step 2: Audio.** At the end of `DoCustomSoundAnalysis`: `m_presetImm/Att` = `ShapeAudio(smoother.Step(raw, 1/fps, af), sens)`. Then for the waveform and spectrum: `hist += (raw - hist) * WaveFollow(speed, 1/fps)`; write `hist * WaveAmplitude(sens)` back into `m_sound.fWaveform`, `mysound.fWave` and `m_sound.fSpectrum`. Skip the whole waveform block when follow == 1 and amplitude == 1 (defaults unchanged). Use `m_presetImm/Att` in every `var_pf_bass..treb_att` feed, shader `h[3]`/`h[4]` and the mod-wave alpha lines. Keep raw `imm_rel` in the hard-cut test (~729).
- [ ] **Step 3: Motion.** In the mesh loop, right before `fZoom2`, apply `ScaleMotion(..., m_calm.speed)` to fZoom, fRot, fWarp, fDX, fDY, fSX, fSY, only when speed != 1.
- [ ] **Step 4: Step fps.** `m_fCalmStepFps = StepFps(GetFps())` each frame; feed it as `var_pf_fps` (main, shapes, waves) and shader `h[2].y`.
- [ ] **Step 5:** `.\build.ps1 -Test` → `0 failed`; `git grep -n "imm_rel\|avg_rel" code/vis_milk2/milkdropfs.cpp` shows only the hard-cut line.
- [ ] **Step 6: Commit** `feat(calm): animation clock, motion scale, audio and waveform shaping`.

---

### Task 3: Steps, hold frames, persistence and the limiter

**Files:** `plugin.h`, `plugin.cpp` (the `RenderFrame(redraw)` call ~3361, init ~606, `CleanUpMyDX9Stuff` ~3143), `milkdropfs.cpp` (after per-frame range checks ~585, after shape per-frame code ~1858, end of `RenderFrame` after `DrawUserSprites`), `state.h/.cpp` (`CalmSlew m_calmSlew`, `Reset()` at the end of `Import`)

**Interfaces – Produces:**
- `LPDIRECT3DTEXTURE9 m_calmPrevTex; bool m_bCalmPrevValid; float m_calmStepAccum;`
- `void CalmPostFrame(); void CalmHoldFrame(); void CalmDrawTexture(LPDIRECT3DTEXTURE9 tex, float alpha);`
- `void CalmSlewMainOutputs(CState*, float dt); void CalmSlewShapeOutputs(CState*, int shape, float dt);`

- [ ] **Step 1: Step or hold.** Replace `RenderFrame(redraw)` with: `if (redraw || !m_bCalmPrevValid || StepDue(&m_calmStepAccum, 1/fps)) RenderFrame(redraw); else CalmHoldFrame();`.
- [ ] **Step 2: Keep the image.** `CalmPostFrame()` at the end of `RenderFrame` (after `DrawUserSprites`, before the VS flip): create `m_calmPrevTex` (render-target texture, back-buffer size, `D3DFMT_X8R8G8B8`, `D3DPOOL_DEFAULT`) when missing or when the size changed; if `PersistenceAmount(af, m_fCalmStepFps) > 0` and the image is valid, draw it over the back buffer with that alpha; then `StretchRect` the back buffer into it. `CalmHoldFrame()` draws it with alpha 1. `CalmDrawTexture` uses a full-screen `XYZRHW|TEX1` quad (−0.5 offset), point filtering, `TFACTOR` alpha, and a `D3DSBT_ALL` state block to restore the device state. Release the texture in `CleanUpMyDX9Stuff` and set `m_bCalmPrevValid = false`.
- [ ] **Step 3: Limiter.** After the per-frame range checks, `CalmSlewMainOutputs`: wave rgba (0–3), ob rgba (4–7), ib rgba (8–11), gamma (12, 4 × colour rate), echo_alpha (13). After each shape's per-frame code, when `instances == 1`, `CalmSlewShapeOutputs`: x, y, rad (Size), ang, tex_ang (Angle), tex_zoom (Size), r, g, b, r2, g2, b2 (Colour) at channels `16 + shape*12 + j`. Both are no-ops when Anti-flicker is Off; `dt = 1 / m_fCalmStepFps`.
- [ ] **Step 4:** `.\build.ps1 -Test` → `0 failed`; `.\build.ps1` no new warnings.
- [ ] **Step 5: Commit** `feat(calm): 60 steps/s with hold frames, persistence, colour and shape limiter`.

---

### Task 4: Per-preset storage

**Files:** `state.h/.cpp` (`CalmSettings m_calm;` next to `m_fRating` (state.h 242); `Import` ~1233 in the `STATE_GENERAL` block; `Export` after `fRating` ~841), `plugin.cpp` (next to `SetCurrentPresetRating` ~8903; frame start)

**Interfaces – Produces:** `CState::m_calm`; `bool CPlugin::SaveCalmToPreset();`; effective `CPlugin::m_calm = blending ? BlendCalm(old, new, m_fBlendProgress) : m_pState->m_calm`, computed at the start of each frame.

- [ ] **Step 1:** `Import`: set `m_calm = CalmSettings()` first, then read the three keys with `GetFastFloat` and `ClampSettings`. `Export`: write the non-empty keys of `CalmPresetKeys(m_calm)`.
- [ ] **Step 2:** `SaveCalmToPreset()`: for each key, `WritePrivateProfileStringW(L"preset00", name, value.empty() ? NULL : value.c_str(), m_szCurrentPresetFile)`; return false if any call fails.
- [ ] **Step 3:** effective settings at the start of each frame (before the clock and audio use them).
- [ ] **Step 4:** `.\build.ps1 -Test` → `0 failed`.
- [ ] **Step 5: Commit** `feat(calm): settings per preset, stored in the preset file`.

---

### Task 5: Commands, keys, menu, help

**Files:** `features/commands.h/.cpp`, `features/context_menu.cpp`, `features/help_text.cpp`, `plugin.cpp` ("case 3" key routing ~5313, `DispatchCommand`, `GetAppSnapshot`), `plugin.rc`, `resource.h`, tests `test_commands.cpp`, `test_context_menu.cpp`, `test_help_text.cpp`

**Interfaces – Produces:** `Cmd::SpeedUp, SpeedDown, SensitivityUp, SensitivityDown, CalmReset, AntiFlickerOff, AntiFlickerSoft, AntiFlickerStrong` (new group after `ViewDesktopMode`); `KeyScope::VKeyCtrl, VKeyCtrlShift` (looked up in "case 3", Ctrl down, Alt up); `AppSnapshot::antiFlicker`, `AppSnapshot::hasPresetFile`.

- [ ] **Step 1: Failing tests.**
  - `TestCommands_CtrlScopes`: `VKeyCtrl`: `VK_OEM_PLUS`, `VK_ADD` → `SensitivityUp`; `VK_OEM_MINUS`, `VK_SUBTRACT` → `SensitivityDown`; `'0'` → `CalmReset`. `VKeyCtrlShift`: same keys → `SpeedUp` / `SpeedDown`. `Char '+'` is still `PresetRateUp`.
  - `TestCommands_CtrlLabels`: `L"Ctrl++"`, `L"Ctrl+-"`, `L"Ctrl+Shift++"`, `L"Ctrl+0"`.
  - `TestCommands_AntiFlickerRadio`: `antiFlicker = 1` checks only `AntiFlickerSoft`; all calm commands disabled while `textEditActive` or without `hasPresetFile`.
  - Context menu: View → "Motion": Speed faster, Speed slower, Sensitivity higher, Sensitivity lower, Reset this preset, separator, Anti-flicker Off / Soft / Strong.
  - Help: `Ctrl+ +/-\taudio sensitivity`, `Ctrl+Shift+ +/-\tspeed`, `Ctrl+0\tthis preset back to normal speed and sensitivity`.
- [ ] **Step 2:** `.\build.ps1 -Test` → the new tests fail.
- [ ] **Step 3:** implement. Each command changes `m_pState->m_calm`, calls `SaveCalmToPreset()`, resets both states' `m_calmSlew` and the audio smoothers, and shows its toast (or the save error). `CalmReset` shows `Speed 100% · Sensitivity 100%`.
- [ ] **Step 4:** `.\build.ps1 -Test` → `0 failed`.
- [ ] **Step 5: Commit** `feat(calm): Ctrl +/- sensitivity, Ctrl+Shift +/- speed, Motion menu, help`.

---

### Task 6: Show the values on screen

**Files:** `plugin.cpp` `DrawInfoCard` (~4500)

- [ ] **Step 1:** when the card is shown (F4/F5/F6), always add `SensitivityLabel` after the fps item; add `SpeedLabel` when not 100% and `AntiFlickerLabel` when not Off. Show the card for 2.5 s after any calm command (`m_fShowCalmUntilThisTime`, like `m_fShowRatingUntilThisTime`).
- [ ] **Step 2:** `.\build.ps1` and `.\build.ps1 -Test` pass.
- [ ] **Step 3: Commit** `feat(calm): sensitivity and speed in the info card`.

---

### Task 7: Docs and the manual test (run by the user)

**Files:** `CHANGES.md`, `docs/PROJECT_STATUS.md`, roadmap (Phase 14 and Phase 15 sections and table rows; Phase 15 "calm meter, suggests only" listed as not started)

- [ ] **Step 1: Docs.** CHANGES.md "Phase 14: calm motion": the three settings, their keys, the preset-file keys (they live in the program-folder copy, so `-Clean`, deleting `build\Release` or a fresh `-Package` start without them), and the limits below.
- [ ] **Step 2: Commit** `docs: phase 14 calm motion`.
- [ ] **Step 3: Hand the user this checklist:**
  1. Defaults: F5 shows `60 fps  Sensitivity 100%`; presets look as before.
  2. `Zylot - Soup (Jelly)`: Ctrl+Shift+[-] down to 15%: rings and the centre waveform move slowly and smoothly, with no jump at the key press.
  3. `catch 333 magick tracing bennett party potty`: right-click → View → Motion → Anti-flicker: Soft, then Strong: the colour strobing becomes soft pulses.
  4. `fiShbRaiN - witchcraft (neuromancer remix)` with bass-heavy music: Ctrl+[-] to 30%: the full-screen spin calms down.
  5. F2 to 144: motion speed is the same as at 60.
  6. At Speed 15%, Alt+Enter twice: no black or frozen image afterwards.
  7. The preset file in `build\Release\plugins\Milkdrop2\presets` has the calm keys under `[preset00]`; `git status` shows no change.
  8. Tune preset A, Space to an untuned B: values slide during the blend, B shows `Sensitivity 100%`. Backspace to A: tuned again. Ctrl+0 on A removes its keys.
  9. Save a tuned preset with S under a new name and load it: it keeps the values.
  10. L or K, then Ctrl+[-]: nothing changes. Quit and restart: tuned presets keep their values.
- [ ] **Step 4:** after the user approves phase 14, follow `superpowers:finishing-a-development-branch`. Phase 15 starts only after that approval.

---

## Phase 15: the calm meter (suggests only, never applies by itself)

**Rule for all of phase 15:** nothing the meter computes changes a setting or a preset file unless the user presses Ctrl+7. There is no timer, no preset-load hook and no setting that applies suggestions automatically. A test pins this (`TestCalm_SuggestNeverApplied`, Task 9).

**Keys:** Ctrl+9 = meter on / off (a line in the info card). Ctrl+7 = apply the shown suggestion to the current preset (written to the preset file like any calm change). Both are disabled without a preset file or while typing.

### Task 8: `FlickerMeter` and a reliable motion measure

**Files:** `features/calm.h/.cpp`, `code/tests/test_calm.cpp`

**Interfaces – Produces:**
```cpp
struct CalmStats {
    float flashesPerSec = 0;   // pairs of opposite luminance swings >= 0.1 in the last second, worst of whole frame / 4 quadrants
    float motionPct = 0;       // warp-mesh motion: % of screen width per second (from the mesh, not the image)
    float changePct = 0;       // mean |pixel change| per step, % (shown for information)
};
class FlickerMeter {           // fed from a 32x18 luminance grid once per step
public:
    void AddFrame(const float* luma, int w, int h, float dtSec);
    void Reset();
    float FlashesPerSec() const; float ChangePct() const;
};
float MeshMotion(const float* du, const float* dv, int n);   // mean |(u,v) - (u0,v0)| of the warp mesh vertices, in texture units
```
- Motion comes from the warp mesh, not the image: the spike's image-based estimate missed zoom motion (Soup 173 → 137 %/s while clearly slower). The mesh displacement per step is exactly the motion the preset asks for, after Speed is applied. `motionPct = MeshMotion × steps/s × 100`, smoothed over 0.5 s.
- [ ] **Step 1: Failing tests:** `TestCalm_FlickerMeterCountsStrobe` (black/white every 10 frames at 60 fps → 2.5–3.5 flashes/s), `TestCalm_FlickerMeterStillImage` (0 flashes, change < 0.01%), `TestCalm_MeshMotion` (all vertices shifted by 0.01 → 0.01; none → 0).
- [ ] **Step 2–4:** implement, `.\build.ps1 -Test` → `0 failed`.
- [ ] **Step 5: Commit** `feat(calm): flicker meter and mesh motion measure`.

### Task 9: Suggestion rules

**Files:** `features/calm.h/.cpp`, `code/tests/test_calm.cpp`

**Interfaces – Produces:** `CalmSettings SuggestCalm(const CalmStats& s, const CalmSettings& current);`
- Only ever lowers values; never raises speed or sensitivity and never turns Anti-flicker down.
- Flashes ≥ 3/s → Strong; ≥ 1.5/s → at least Soft (the 3/s line follows the common photosensitivity guideline).
- Motion above `kTargetMotionPct` → Speed × (target / motion), snapped to the 5% step, not below 10%. The starting target is 40 %/s; the manual test calibrates it (Soup at 15% was confirmed good).
- Sensitivity is lowered only when motion rises with the bass (the meter keeps the correlation of `motionPct` with `bass` over the last 4 s; correlation > 0.5 → Sensitivity × (target / motion)).
- [ ] **Step 1: Failing tests:** `TestCalm_SuggestStrobe` (4 flashes/s → Strong), `TestCalm_SuggestSlowsSelfMotion` (motion 160, no bass correlation → Speed ≤ 30%, Sensitivity unchanged), `TestCalm_SuggestLowersSensitivityForAudioMotion` (correlation 0.8 → Sensitivity lowered), `TestCalm_SuggestNeverRaises` (quiet stats → equal to current), `TestCalm_SuggestNeverApplied` (computing a suggestion leaves the input settings unchanged; `SuggestCalm` is `const`-only and has no side effects).
- [ ] **Step 2–4:** implement, `.\build.ps1 -Test` → `0 failed`.
- [ ] **Step 5: Commit** `feat(calm): suggestion rules (display only)`.

### Task 10: Meter on screen and Ctrl+7

**Files:** `plugin.h/.cpp` (measure in `CalmPostFrame` / `CalmHoldFrame`: `StretchRect` the back buffer to a 32×18 render target, `GetRenderTargetData` into one of 3 system-memory surfaces, read the one from 2 frames ago so the CPU does not wait), `milkdropfs.cpp` (mesh motion from the warp loop), `features/commands.*`, `context_menu.cpp`, `help_text.cpp`, tests

**Interfaces:** `Cmd::CalmMeterToggle, CalmApplySuggestion`; `KeyScope::VKeyCtrl` keys `'9'` and `'7'`.
- [ ] **Step 1: Failing tests:** `TestCommands_CalmMeterKeys` (Ctrl+9 / Ctrl+7 bindings and labels; both disabled while typing or without a preset file); context menu View → Motion gains "Calm meter" (check) and "Apply suggestion"; help lines `Ctrl+9\tcalm meter` and `Ctrl+7\tapply the suggested calm values`.
- [ ] **Step 2:** implement. The info card line (only while the meter is on): `flash 2.5/s  motion 45%/s  → Speed 30% · Sensitivity 100% · Anti-flicker: Soft`. Ctrl+7 copies the suggestion into `m_pState->m_calm`, calls `SaveCalmToPreset()` and shows `Applied: …`. Measurement surfaces are released in `CleanUpMyDX9Stuff`; if creating them fails, the meter shows `meter not available` and nothing else changes.
- [ ] **Step 3:** `.\build.ps1 -Test` → `0 failed`; no new warnings.
- [ ] **Step 4: Commit** `feat(calm): calm meter and apply-suggestion key`.
- [ ] **Step 5: Docs:** CHANGES.md and roadmap: Phase 15 entry, stating that suggestions are applied only with Ctrl+7. Commit `docs: phase 15 calm meter`.
- [ ] **Step 6: Manual test (user):**
  1. Ctrl+9 on catch 333: about 12–15 flashes/s, suggestion Strong. On a calm preset: 0–1 flashes/s, suggestion equals the current values.
  2. On Soup at 100%: motion is high and the suggested Speed is roughly 15–30%. Ctrl+Shift+[-] to 15%: the motion number drops in proportion.
  3. Leave the meter on for several preset changes: **no value changes by itself**, the preset files are untouched.
  4. Ctrl+7: the suggestion is applied and saved to the preset file; Ctrl+0 undoes it.
  5. Alt+Enter twice with the meter on: it keeps working (or shows `meter not available`), no crash.
- [ ] **Step 7:** after the user approves phase 15, follow `superpowers:finishing-a-development-branch`, then delete the spike worktree and branch `suri/spike-calm` (with the user's OK).

---

## Appendix: how the mechanism works, and what the spike showed

### Why some presets look wild

A preset runs once per step. It reads inputs (`time`, `fps`, `bass`, `mid`, `treb`, the waveform) and writes outputs (`zoom`, `rot`, `dx`, colours, shape values), and its warp shader turns the previous image into the next one. The spike (branch `suri/spike-calm`, 2026-10-28) tested three presets that are wild for three different reasons:

| Preset | Cause | What calms it |
|---|---|---|
| `fiShbRaiN - witchcraft (neuromancer remix)` | A full-screen textured shape with `tex_ang = bass*10`: every bass hit spins the whole screen | Sensitivity, and the shape limiter of Anti-flicker |
| `catch 333 magick tracing bennett party potty` | Its warp shader swaps and inverts the colour channels **every step**: colours strobe at ~12–15 flashes/s whatever the music does | Anti-flicker's persistence: Strong took it from 12.5 to 2.5–3 flashes/s and frame change from 13% to 3% (the image gets greyer) |
| `Zylot - Soup (Jelly)` | A constant per-pixel zoom (`zoom = 1 + rad*.05`) plus a fresh waveform drawn in the centre every step | Speed: scaling each step's motion and slowing how fast the waveform may change. 15% was confirmed good by the user |

### What did not work in the spike, and was dropped
- **Speed as "fewer steps"**: dropping steps made motion jumpy instead of slower. Speed now scales the motion inside each step; the step cap (60/s) only stops high frame rates from speeding presets up.
- **Scaling `decay` with Speed**: trails lasted longer and images became brighter and busier.
- **Light smoothing of the waveform**: at 25% it still changed 25% per frame and looked fast; the follow rate is now speed², so 15% changes about 2% per frame. The cost: the averaged waveform is smaller, so Soup at 15% is darker and sparser.

### The controls

```
            ┌──────────────── gives ─────────────────┐     ┌──────── takes back ─────────┐
 real clock ─► AnimClock (dt × Speed) ──► time ──┐    │     │ zoom,rot,warp,dx,dy,sx,sy   │
 audio ──────► smooth (Anti-flicker) ──►          │    ▼     │   ^/× Speed                 ├─► image ─► persistence ─► screen
               shape (Sensitivity) ──► bass… ─────► PRESET ─┤ colours, shape values:      │   (Anti-flicker)   (60 steps/s,
 waveform ───► follow (Speed²) × min(1,Sens) ─────┘          │   max change per second     │                     hold frames between)
                                                             └─────────────────────────────┘
```

- **Speed (10%–150%, Ctrl+Shift+[+] / [-]):** the clock presets see advances at `Speed × real time`; each step's motion is scaled (rotation and shifts × Speed, zoom and stretch to the power Speed); the waveform and spectrum change at most `Speed²` of the way per frame. Changing Speed never makes a preset jump.
- **Sensitivity (0%–200%, Ctrl+[+] / [-], always shown in the info card):** audio values are reshaped around their quiet level of 1.0 (`1 + (x − 1) × Sensitivity`); below 100% the drawn waveform and spectrum are also smaller. Hard cuts keep using the real audio.
- **Anti-flicker (Off / Soft / Strong, right-click → View → Motion):** audio rises quickly and falls slowly; colours, gamma, echo and the values of single shapes may change only so much per second; each new image is blended with the previous one (50% / 80% at 60 steps/s).
- **Steps:** at most 60 preset steps per second at any display rate; presets see `fps` = 60, so they run at the speed they were written for.

### Per preset
Every setting belongs to the preset on screen, like its rating. When MilkDrop blends to the next preset, speed and sensitivity slide from the old values to the new ones. An untuned preset runs at Speed 100%, Sensitivity 100%, Anti-flicker Off.

Stored in the preset file itself, section `[preset00]`, next to `fRating`, only when not the default:
```
[preset00]
fRating=4.000
fCalmSpeed=0.15
fCalmSensitivity=0.70
nCalmAntiFlicker=1
```
The file that changes is the copy in `build\Release\plugins\Milkdrop2\presets\` (not in git); `content\presets` is never touched. Like the rating, the values are lost with `.\build.ps1 -Clean`, when `build\Release` is deleted, and in a fresh `-Package`. A rebuild without `-Clean` keeps them (the post-build `xcopy /D` does not overwrite newer files).

### Limits
- Presets that count with `frame` or their own `q` variables each step are not slowed by Speed beyond the 60-steps cap.
- Strong persistence makes strobing presets greyer; very low Speed makes waveform-driven presets darker.
- Shapes drawn as many instances (`instances > 1`) are not limited by Anti-flicker (persistence still applies).

### Phase 15: the calm meter
The spike's meter read the final image at 32×18 every frame. Its flash count was reliable (catch 333: 12–15/s; calm presets: 0–1/s; threshold 3/s). Its image-based motion estimate did not follow zoom motion (Soup: 173 → 137 %/s while clearly slower), so phase 15 measures motion from the warp mesh instead. The meter only **suggests**; values change only when the user presses Ctrl+7. Applying suggestions automatically on preset load is left for a later decision.
