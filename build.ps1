<#
.SYNOPSIS
  Builds MilkDrop3 from the command line (no need to open Visual Studio).

.EXAMPLE
  .\build.ps1                 # Release build
  .\build.ps1 -Config Debug   # Debug build
  .\build.ps1 -Run            # Build and launch
  .\build.ps1 -Package        # Build and create dist\MilkDrop3-SURI-<ver>-win32.zip from sources
  .\build.ps1 -Test           # Build and run the unit tests (exit code = failed tests)
#>
param(
    [ValidateSet('Release', 'Debug')] [string]$Config = 'Release',
    [switch]$Run,
    [switch]$Package,
    [switch]$Test,
    [switch]$Clean,
    [ValidateSet('All', 'Curated')] [string]$PresetSet = 'All',
    [switch]$StageOnly,
    [Parameter(DontShow)] [switch]$SkipBuild,
    [Parameter(DontShow)] [string]$CuratedList
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

if (-not $SkipBuild) {
    # --- Locate MSBuild via vswhere -------------------------------------------
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - install Visual Studio 2022 with 'Desktop development with C++'." }
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msbuild) { throw "MSBuild not found." }

    # --- Make sure D3DX9 (NuGet: Microsoft.DXSDK.D3DX) is present -------------
    $d3dx = Join-Path $root 'third_party\d3dx\build\native\include\d3dx9.h'
    if (-not (Test-Path $d3dx)) {
        Write-Host "Downloading Microsoft.DXSDK.D3DX ..." -ForegroundColor Cyan
        New-Item -ItemType Directory -Force (Join-Path $root 'third_party') | Out-Null
        $zip = Join-Path $root 'third_party\d3dx.zip'
        Invoke-WebRequest 'https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX/9.29.952.8' -OutFile $zip
        Expand-Archive $zip (Join-Path $root 'third_party\d3dx') -Force
        Remove-Item $zip
    }

    $sln = Join-Path $root 'code\MilkDrop3.sln'
    $target = if ($Clean) { 'Rebuild' } else { 'Build' }
    & $msbuild $sln /m /nologo /v:minimal /t:$target /p:Configuration=$Config /p:Platform=Win32
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
}

$out = Join-Path $root "build\$Config"
if (-not $SkipBuild) { Write-Host "`nBuild OK -> $out\MilkDrop3.exe" -ForegroundColor Green }

if ($Test) {
    $tests = Join-Path $root "build\tests\$Config\unit_tests.exe"
    & $tests
    $failed = $LASTEXITCODE
    if ($failed -ne 0) { Write-Host "Unit tests: $failed failed" -ForegroundColor Red; exit $failed }
    Write-Host "Unit tests: all passed" -ForegroundColor Green
}

if ($PresetSet -eq 'Curated' -and -not ($Package -or $StageOnly)) {
    Write-Warning "-PresetSet Curated is ignored without -Package or -StageOnly."
}

if ($Package -or $StageOnly) {
    # Version comes from defines.h; the stage is assembled from SOURCE files only
    # (never from build\<Config>\plugins, which holds the user's ini/log/dragged presets).
    $defs = Get-Content -LiteralPath (Join-Path $root 'code\vis_milk2\defines.h') -Raw
    if ($defs -notmatch '#define\s+MILKDROP_VERSION_STR\s+"([^"]+)"') { throw "MILKDROP_VERSION_STR not found in defines.h" }
    $ver = $Matches[1]
    $name = "MilkDrop3-SURI-$ver-win32"

    # Validate the curated list first so a bad list fails fast (before copying or hashing anything).
    $curatedNames = @()
    if ($PresetSet -eq 'Curated') {
        if (-not $CuratedList) { $CuratedList = Join-Path $root 'content\curated.txt' }
        if (-not (Test-Path -LiteralPath $CuratedList)) { throw "curated list not found: $CuratedList" }
        # Names are trimmed; blank lines and '#' comment lines are skipped.
        $curatedNames = @(Get-Content -LiteralPath $CuratedList -Encoding UTF8 | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
        foreach ($n in $curatedNames) {
            if (-not (Test-Path -LiteralPath (Join-Path $root "content\presets\$n") -PathType Leaf)) { throw "curated preset missing: $n" }
        }
    }
    $dist = Join-Path $root 'dist'
    $stage = Join-Path $dist "stage\$name"
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    $m2 = Join-Path $stage 'plugins\Milkdrop2'
    New-Item -ItemType Directory -Force $m2 | Out-Null

    foreach ($f in 'MilkDrop3.exe', 'D3DX9_43.dll', 'D3DCompiler_43.dll') {
        $src = Join-Path $out $f
        if (-not (Test-Path -LiteralPath $src)) { throw "Missing build output: $src" }
        Copy-Item -LiteralPath $src -Destination $stage
    }
    Copy-Item -LiteralPath (Join-Path $root 'LICENSE.txt') -Destination $stage
    Copy-Item -LiteralPath (Join-Path $root 'content\PROVENANCE.md') -Destination $stage
    Copy-Item -LiteralPath (Join-Path $root 'third_party\d3dx\LICENSE.txt') -Destination (Join-Path $stage 'D3DX9-LICENSE.txt')
    foreach ($d2 in 'data', 'docs') {
        Copy-Item -LiteralPath (Join-Path $root "code\resources\Milkdrop2\$d2") -Destination $m2 -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $root 'content\textures') -Destination $m2 -Recurse
    $presetsSrc = Join-Path $root 'content\presets'
    if ($PresetSet -eq 'Curated') {
        $pdst = Join-Path $m2 'presets'
        New-Item -ItemType Directory -Force $pdst | Out-Null
        foreach ($n in $curatedNames) {
            Copy-Item -LiteralPath (Join-Path $presetsSrc $n) -Destination $pdst
        }
    } else {
        Copy-Item -LiteralPath $presetsSrc -Destination $m2 -Recurse
    }

    Write-Host "Stage OK -> $stage" -ForegroundColor Green

    $suffix = if ($PresetSet -eq 'Curated') { '-curated' } else { '' }
    $zipOut = Join-Path $dist "MilkDrop3-SURI-$ver-win32$suffix.zip"
    if (-not $StageOnly -and (Test-Path -LiteralPath $zipOut)) { Remove-Item -LiteralPath $zipOut }

    if (-not $StageOnly) {
        # ZipFile writes '/' entry separators on every PowerShell (Compress-Archive on 5.1 writes '\').
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        try {
            [System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipOut, [System.IO.Compression.CompressionLevel]::Optimal, $false)
        } catch {
            if (Test-Path -LiteralPath $zipOut) { Remove-Item -LiteralPath $zipOut -Force -ErrorAction SilentlyContinue }
            throw
        }
        Write-Host "Package -> $zipOut" -ForegroundColor Green
    }
}

if ($Run) { Start-Process -FilePath (Join-Path $out 'MilkDrop3.exe') -WorkingDirectory $out }
