# Phase 13: Packaging – implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a `.\build.ps1 -Package` that builds a clean package (full, or Curated with 500 presets) **from the source files**, not from the build folder where the user has already saved settings and dragged-in presets.

**Architecture:** PowerShell only, no C++:
- `tools/make-curated.ps1` generates a Curated list that is committed to the repo.
- `build.ps1 -Package` copies into a staging folder and creates a zip from it.
- A self-test script (`tools/test-tools.ps1`) runs on temporary folders and checks exit codes and output.

**Tech Stack:** PowerShell 7, git.

**Spec:** [roadmap](2026-10-24-milkdrop-suri-roadmap.md), phase 13 and Global Constraints.

## Global Constraints

- **Approval gate:** execute only after the plan is approved.
- **Base:** a `suri/phase-1-13` branch in a separate worktree starting from `main`. Before any whole-tree operation, run `ListAgents` and `git status`. No stash, reset or checkout on the shared tree.
- `build.ps1` gets no new dependencies. All its other switches (`-Run`, `-Test`, `-Clean`, `-Config`) behave as today.
- Each Task ends when `pwsh tools/test-tools.ps1` exits 0, and `.\build.ps1 -Test` prints `0 failed`.
- One commit per Task. The message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **File names with `$`, spaces, square brackets and Hebrew** (for example `$$$ Royal - Mashup (1).milk`; `[` is a wildcard in PowerShell): every file is expected to be packaged. Use `-LiteralPath` everywhere. Tested in `Package_WeirdNames` (Task 1).
2. **The user has already run the program** (`build\Release` contains `milk2.ini`, `MilkDrop3.log`, `.pdb`, dragged-in presets and `playlists\`): none of these are expected to enter the package, because the staging folder is built from the sources. Tested in `Package_NoUserState` (Task 1).
3. **A name in `curated.txt` that doesn't exist in `content/presets`** (a deleted preset or a typo): packaging is expected to fail with `curated preset missing: <name>`, rather than silently creating an incomplete package. Tested in `Package_CuratedMissing` (Task 2).

---

## File structure

| File | Responsibility |
|---|---|
| `tools/make-curated.ps1` | Builds `content/curated.txt` |
| `tools/test-tools.ps1` | Self-tests for the curated list and for packaging, on temporary fixtures in `$env:TEMP` |
| `content/PROVENANCE.md` | Source and license of the content |
| `content/curated.txt` | One preset name per line (UTF-8) |
| `build.ps1` | Staging-based `-Package`, `-PresetSet All\|Curated` |

---

### Task 1: Packaging from staging

**Files:**
- Modify: `build.ps1`, `tools/test-tools.ps1`

**Interfaces:**
- Produces:
  - **New parameters:** `-PresetSet All|Curated` (default `All`), and `-StageOnly` (builds the staging folder without a zip, for the tests).
  - **The staging folder:** `dist\stage\MilkDrop3-SURI-<ver>-win32\`. The `<ver>` is read from `MILKDROP_VERSION_STR` in `code\vis_milk2\defines.h`.
  - **Staging content:**
    - `MilkDrop3.exe`, `D3DX9_43.dll`, `D3DCompiler_43.dll` from `build\<Config>`.
    - `LICENSE.txt` and `PROVENANCE.md` from the sources.
    - `plugins\Milkdrop2\{data,docs}` from `code\resources\Milkdrop2`.
    - `plugins\Milkdrop2\textures` from `content\textures`.
    - `plugins\Milkdrop2\presets` from `content\presets`.
  - **The zip:** `dist\MilkDrop3-SURI-<ver>-win32[-curated].zip`.

- [x] **Step 1: Failing tests in `test-tools.ps1`.** `build.ps1` takes an internal `-SkipBuild` (a hidden switch), so the tests don't build the code. The exe and the dlls are copied from `build\Release`, and if they don't exist the test is skipped with `SKIP`.
  - `Package_NoUserState`: create `build\Release\milk2.ini`, `build\Release\plugins\Milkdrop2\presets\dragged.milk` and `playlists\a.m3u`, and run `-StageOnly`. Expected: the staging folder has no `*.ini`, `*.log`, `*.pdb`, `dragged.milk` or `playlists`, and the number of presets in staging equals `content\presets`. Clean up what was created at the end.
  - `Package_WeirdNames`: verify that all the `content\presets` names starting with `$$$` exist in staging.
- [x] **Step 2:** run `pwsh tools/test-tools.ps1`. Expected FAIL in both cases.
- [x] **Step 3: Implement.**
  - Delete the previous staging folder.
  - Copy with `Copy-Item -LiteralPath`.
  - Create the zip from the staging folder. Delete a previous zip of the same name.
- [x] **Step 4:** run `pwsh tools/test-tools.ps1` and `.\build.ps1 -Package`. Expected exit 0, and the zip created. Record the size and the file count in the ledger.
- [x] **Step 5:** Commit `build: package from sources`.

### Task 2: The Curated package

**Files:**
- Create: `tools/make-curated.ps1`, `content/curated.txt`
- Modify: `build.ps1`, `tools/test-tools.ps1`, `docs/superpowers/plans/2026-10-24-milkdrop-suri-roadmap.md`, `docs/PROJECT_STATUS.md`

**Interfaces:**
- Produces:
  - `tools/make-curated.ps1 [-Count 500] [-Out content\curated.txt]`. `-PresetSet Curated` in `build.ps1` copies only the names in `curated.txt`.
  - **The selection algorithm (deterministic).** Every preset has `fRating=5`, so the rating doesn't help.
    - The author is the text before the first ` - `. If there is no ` - `, the author is the whole name.
    - Sort authors by their preset count, largest first, and ties by name.
    - Within each author, sort by name (ordinal, case-insensitive).
    - Pick round-robin: each author's first, then each author's second, and so on, up to `-Count`.
    - Write UTF-8 without a BOM, one name per line.
  - The list is a starting point, and the user edits it by hand.

- [x] **Step 1: Failing tests.**
  - `Curated_RoundRobin`: a fixture with `A - 1`, `A - 2`, `A - 3`, `B - 1`, `B - 2`, `C - 1`, and `-Count 4`. Expected exactly `A - 1`, `B - 1`, `C - 1`, `A - 2`, in this order.
  - `Package_CuratedMissing`: a temporary `curated.txt` (the hidden switch `-CuratedList`) with `nope.milk`. Expected the exception `curated preset missing: nope.milk`.
  - `Package_Curated`: the number of presets in staging is expected to equal the number of non-blank lines in `curated.txt`.
- [x] **Step 2:** run the tests. Expected FAIL.
- [x] **Step 3:** implement `make-curated.ps1` and `-PresetSet Curated`. Run `make-curated.ps1` and commit `content/curated.txt` to the repo.
- [x] **Step 4:** run `pwsh tools/test-tools.ps1` and `.\build.ps1 -Package -PresetSet Curated`. Expected a zip with 500 presets.
- [x] **Step 5: Update the documentation.**
  - Roadmap: phase 13 moves to "done, awaiting test".
  - `PROJECT_STATUS.md`: add the packaging commands.
  - User guide: unchanged.
- [x] **Step 6:** Commit `build: curated preset set`.
- [x] **Step 7: The user's manual test.**
  - Unpack both packages into an empty folder and run them.
  - Expected: the program starts, presets change, and there are no messages about missing textures.
  - In the full package, expected no `milk2.ini` upfront, i.e. default settings.
