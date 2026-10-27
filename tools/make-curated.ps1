<#
.SYNOPSIS
  Generates content\curated.txt: a deterministic, author-diverse starting list of presets.

.DESCRIPTION
  Author = text of the file name (without .milk) before the first " - "; with no " - " the author is the whole name.
  Authors are ordered by preset count (descending), ties by name; presets within an author by name
  (ordinal, case-insensitive). Selection is round-robin: the first preset of every author, then the
  second of every author, and so on, until -Count names are chosen. Output is UTF-8 without BOM,
  one file name per line. The list is a starting point; edit it by hand.
#>
param(
    [int]$Count = 500,
    [string]$Out,
    [Parameter(DontShow)] [string]$PresetsDir
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $repo 'content\curated.txt' }
if (-not $PresetsDir) { $PresetsDir = Join-Path $repo 'content\presets' }

# Author grouping is deliberately case-insensitive (ruling R4: "Flexi" == "flexi"): PowerShell's @{} is case-insensitive.
$groups = @{}
foreach ($f in Get-ChildItem -LiteralPath $PresetsDir -Filter '*.milk' -File) {
    $base = [IO.Path]::GetFileNameWithoutExtension($f.Name)
    $i = $base.IndexOf(' - ', [StringComparison]::Ordinal)
    $author = if ($i -ge 0) { $base.Substring(0, $i) } else { $base }
    if (-not $groups.ContainsKey($author)) { $groups[$author] = New-Object 'System.Collections.Generic.List[string]' }
    $groups[$author].Add($f.Name)
}
$authors = New-Object 'System.Collections.Generic.List[string]'
$authors.AddRange([string[]]@($groups.Keys))
$authors.Sort([Comparison[string]] {
    param($a, $b)
    $d = $groups[$b].Count - $groups[$a].Count
    if ($d -ne 0) { return $d }
    return [string]::Compare($a, $b, [StringComparison]::OrdinalIgnoreCase)
})
foreach ($a in $authors) { $groups[$a].Sort([Comparison[string]] { param($x, $y) [string]::Compare($x, $y, [StringComparison]::OrdinalIgnoreCase) }) }

$picked = New-Object 'System.Collections.Generic.List[string]'
for ($round = 0; $picked.Count -lt $Count; $round++) {
    $any = $false
    foreach ($a in $authors) {
        if ($round -lt $groups[$a].Count) {
            $any = $true
            if ($picked.Count -lt $Count) { $picked.Add($groups[$a][$round]) }
        }
    }
    if (-not $any) { break }
}
[IO.File]::WriteAllText($Out, (($picked -join "`n") + "`n"), (New-Object Text.UTF8Encoding($false)))
Write-Host "Wrote $($picked.Count) names from $($authors.Count) authors -> $Out"
