# Test runner for tools\*.ps1. Prints PASS/FAIL per case; exit code = number of failures.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$script:Root = Join-Path $env:TEMP ("suri-tools-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $script:Root | Out-Null
$script:Failures = 0

function New-Case([string]$name) {
    $d = Join-Path $script:Root $name
    New-Item -ItemType Directory -Path (Join-Path $d 'content') -Force | Out-Null
    return $d
}
function Put([string]$file, [string]$text) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $file) -Force | Out-Null
    [IO.File]::WriteAllText($file, $text, (New-Object Text.UTF8Encoding($false)))
}
function Assert([bool]$cond, [string]$detail) { if (-not $cond) { throw $detail } }

$script:Repo = Split-Path -Parent $here
function Get-StageDir {
    $defs = Get-Content -LiteralPath (Join-Path $script:Repo 'code\vis_milk2\defines.h') -Raw
    if ($defs -notmatch '#define\s+MILKDROP_VERSION_STR\s+"([^"]+)"') { throw 'MILKDROP_VERSION_STR not found in defines.h' }
    return [pscustomobject]@{ Ver = $Matches[1]; Stage = (Join-Path $script:Repo "dist\stage\MilkDrop3-SURI-$($Matches[1])-win32") }
}
function Run-Build([string[]]$extra) {
    $ErrorActionPreference = 'Continue'   # a throwing build.ps1 writes to stderr; capture it, don't abort
    $out = & pwsh -NoProfile -File (Join-Path $script:Repo 'build.ps1') -SkipBuild @extra 2>&1 | ForEach-Object { "$_" }
    return [pscustomobject]@{ Exit = $LASTEXITCODE; Out = ($out -join "`n") }
}
function Have-BuildOutput {
    $b = Join-Path $script:Repo 'build\Release'
    return ('MilkDrop3.exe', 'D3DX9_43.dll', 'D3DCompiler_43.dll' | ForEach-Object { Test-Path -LiteralPath (Join-Path $b $_) }) -notcontains $false
}
function Test-Package_NoUserState {
    if (-not (Have-BuildOutput)) { return 'SKIP' }
    $b = Join-Path $script:Repo 'build\Release'
    $created = @()   # remove exactly what we created, newest first
    try {
        foreach ($d in 'plugins\Milkdrop2\presets', 'plugins\Milkdrop2\playlists') {
            $full = Join-Path $b $d
            $missing = @(); $p = $full
            while (-not (Test-Path -LiteralPath $p)) { $missing += $p; $p = Split-Path -Parent $p }
            [void](New-Item -ItemType Directory -Path $full -Force)
            $created = @($missing) + $created
        }
        foreach ($f in 'milk2.ini', 'plugins\Milkdrop2\presets\dragged.milk', 'plugins\Milkdrop2\playlists\a.m3u') {
            $full = Join-Path $b $f
            # A real pre-existing file serves the "not in stage" assertion just as well: never overwrite or delete it.
            if (Test-Path -LiteralPath $full) { continue }
            Put $full "user state`n"
            $created = @($full) + $created
        }
        $s = Get-StageDir
        $r = Run-Build @('-StageOnly')
        Assert ($r.Exit -eq 0) "exit $($r.Exit): $($r.Out)"
        $all = Get-ChildItem -LiteralPath $s.Stage -Recurse -Force
        $bad = @($all | Where-Object { $_.Name -like '*.ini' -or $_.Name -like '*.log' -or $_.Name -like '*.pdb' -or $_.Name -eq 'dragged.milk' -or $_.Name -eq 'playlists' })
        Assert ($bad.Count -eq 0) "user state in stage: $($bad.FullName -join ', ')"
        $got = @(Get-ChildItem -LiteralPath (Join-Path $s.Stage 'plugins\Milkdrop2\presets') -File).Count
        $want = @(Get-ChildItem -LiteralPath (Join-Path $script:Repo 'content\presets') -File).Count
        Assert ($got -eq $want) "presets in stage $got, in content $want"
    }
    finally {
        foreach ($c in $created) { Remove-Item -LiteralPath $c -Recurse -Force -ErrorAction SilentlyContinue }
    }
}
function Test-Package_WeirdNames {
    if (-not (Have-BuildOutput)) { return 'SKIP' }
    $s = Get-StageDir
    $r = Run-Build @('-StageOnly')
    Assert ($r.Exit -eq 0) "exit $($r.Exit): $($r.Out)"
    $names = @(Get-ChildItem -LiteralPath (Join-Path $script:Repo 'content\presets') -File | Where-Object { $_.Name.StartsWith('$$$') } | ForEach-Object { $_.Name })
    Assert ($names.Count -gt 0) 'no $$$ presets in content (test would be vacuous)'
    foreach ($n in $names) {
        Assert (Test-Path -LiteralPath (Join-Path $s.Stage "plugins\Milkdrop2\presets\$n")) "missing in stage: $n"
    }
}
function Test-Curated_RoundRobin {
    $c = New-Case 'curated-rr'
    foreach ($n in 'A - 1', 'A - 2', 'A - 3', 'B - 1', 'B - 2', 'C - 1') { Put "$c\content\$n.milk" "fRating=5`n" }
    $r = & pwsh -NoProfile -File (Join-Path $here 'make-curated.ps1') -PresetsDir (Join-Path $c 'content') -Count 4 -Out (Join-Path $c 'out.txt') 2>&1 | ForEach-Object { "$_" }
    Assert (Test-Path -LiteralPath (Join-Path $c 'out.txt')) "no output: $($r -join "`n")"
    $got = @([IO.File]::ReadAllLines((Join-Path $c 'out.txt')))
    $want = @('A - 1.milk', 'B - 1.milk', 'C - 1.milk', 'A - 2.milk')
    Assert (($got -join '|') -eq ($want -join '|')) "got: $($got -join '|')"
    $bytes = [IO.File]::ReadAllBytes((Join-Path $c 'out.txt'))
    Assert (-not ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)) 'output has a BOM'
}
function Test-Package_CuratedMissing {
    if (-not (Have-BuildOutput)) { return 'SKIP' }
    $list = Join-Path $script:Root 'missing-list.txt'
    Put $list "nope.milk`n"
    $r = Run-Build @('-StageOnly', '-PresetSet', 'Curated', '-CuratedList', $list)
    Assert ($r.Exit -ne 0) "expected failure, exit 0: $($r.Out)"
    Assert ($r.Out -match 'curated preset missing: nope\.milk') "no exception text: $($r.Out)"
}
function Test-Package_CuratedListFormat {
    if (-not (Have-BuildOutput)) { return 'SKIP' }
    $first = (Get-ChildItem -LiteralPath (Join-Path $script:Repo 'content\presets') -File | Select-Object -First 1).Name
    $list = Join-Path $script:Root 'format-list.txt'
    Put $list "# a comment line`n`n  $first  `n"
    $s = Get-StageDir
    $r = Run-Build @('-StageOnly', '-PresetSet', 'Curated', '-CuratedList', $list)
    Assert ($r.Exit -eq 0) "exit $($r.Exit): $($r.Out)"
    $got = @(Get-ChildItem -LiteralPath (Join-Path $s.Stage 'plugins\Milkdrop2\presets') -File).Count
    Assert ($got -eq 1) "presets in stage $got, expected 1"
}
function Test-Package_Curated {
    if (-not (Have-BuildOutput)) { return 'SKIP' }
    $want = @(Get-Content -LiteralPath (Join-Path $script:Repo 'content\curated.txt') -Encoding UTF8 | Where-Object { $_.Trim() }).Count
    Assert ($want -gt 0) 'content\curated.txt is missing or empty'
    $s = Get-StageDir
    $r = Run-Build @('-StageOnly', '-PresetSet', 'Curated')
    Assert ($r.Exit -eq 0) "exit $($r.Exit): $($r.Out)"
    $got = @(Get-ChildItem -LiteralPath (Join-Path $s.Stage 'plugins\Milkdrop2\presets') -File).Count
    Assert ($got -eq $want) "presets in stage $got, in curated.txt $want"
}

try {
    $tests = Get-ChildItem Function:\Test-* | Sort-Object Name
    foreach ($t in $tests) {
        $name = $t.Name.Substring(5)
        try { $res = & $t.Name; if ($res -eq 'SKIP') { "SKIP $name (no build\Release output)" } else { "PASS $name" } }
        catch { "FAIL ${name}: $($_.Exception.Message)"; $script:Failures++ }
    }
}
finally {
    Remove-Item -LiteralPath $script:Root -Recurse -Force -ErrorAction SilentlyContinue
}
exit $script:Failures
