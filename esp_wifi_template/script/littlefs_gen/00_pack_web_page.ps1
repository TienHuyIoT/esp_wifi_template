<#
    pack_web_page.ps1 - pack a page from data_org/ into data/<name>.gz for LittleFS.

    Repo convention (same shape as the deployed data/home.htm.gz):
      - the source in data_org/ uses CRLF
      - the .gz has mtime = 0, XFL = 04, OS = 00 (System.IO.Compression, level Fastest)
      - on the board ESPAsyncWebServer serves /keypad.htm from data/keypad.htm.gz with
        a Content-Encoding: gzip header (see _searchFile in WebHandlers.cpp)

    Usage:
      .\pack_web_page.ps1 -Name keypad.htm
      .\pack_web_page.ps1 -Name keypad.htm,home.htm
      .\pack_web_page.ps1 -Name keypad.htm -DryRun      # check only, write nothing

    After packing, flash LittleFS with 01_littlefs_gen.bat then 02_littlefs_gen_flash.bat
    (or .\script\ota\Flash-OTA.ps1 -Target littlefs).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]] $Name,

    # check the source and report what would happen, without writing anything
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$srcDir = Join-Path $repoRoot 'data_org'
$outDir = Join-Path $repoRoot 'data'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-Endings([byte[]] $bytes) {
    $crlf = 0
    $bareLf = 0
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if ($bytes[$i] -eq 10) {
            if ($i -gt 0 -and $bytes[$i - 1] -eq 13) { $crlf++ } else { $bareLf++ }
        }
    }
    return @{ Crlf = $crlf; BareLf = $bareLf }
}

foreach ($item in $Name) {
    $leaf = Split-Path $item -Leaf
    $src = Join-Path $srcDir $leaf
    $dst = Join-Path $outDir ($leaf + '.gz')

    if (-not (Test-Path -LiteralPath $src)) {
        throw "Source page not found: $src"
    }
    Write-Host "== $leaf"
    Write-Host "   source : $src"

    $text = [System.IO.File]::ReadAllText($src)
    $normalized = ($text -replace "`r`n", "`n") -replace "`n", "`r`n"
    $bytes = $utf8NoBom.GetBytes($normalized)
    $endings = Get-Endings $bytes

    if ($DryRun) {
        Write-Host ("   [dry-run] CRLF={0} bareLF={1}, would write {2}" -f $endings.Crlf, $endings.BareLf, $dst)
        continue
    }

    if ($normalized -ne $text) {
        [System.IO.File]::WriteAllText($src, $normalized, $utf8NoBom)
        Write-Host '   source normalised to CRLF'
    }

    $in = [System.IO.File]::OpenRead($src)
    try {
        $out = [System.IO.File]::Create($dst)
        try {
            $gzip = New-Object System.IO.Compression.GZipStream($out, [System.IO.Compression.CompressionLevel]::Fastest)
            try { $in.CopyTo($gzip) } finally { $gzip.Dispose() }
        } finally { $out.Dispose() }
    } finally { $in.Dispose() }

    # verify: the decompressed payload must be byte identical to the source
    $packed = [System.IO.File]::ReadAllBytes($dst)
    $fin = [System.IO.File]::OpenRead($dst)
    $ms = New-Object System.IO.MemoryStream
    try {
        $gunzip = New-Object System.IO.Compression.GZipStream($fin, [System.IO.Compression.CompressionMode]::Decompress)
        try { $gunzip.CopyTo($ms) } finally { $gunzip.Dispose() }
    } finally { $fin.Dispose() }
    $plain = $ms.ToArray()
    $same = [System.Linq.Enumerable]::SequenceEqual($plain, $bytes)

    Write-Host ("   target : {0}" -f $dst)
    Write-Host ("   result : {0} B -> {1} B ({2:N1}%), CRLF={3} bareLF={4}, round-trip={5}" -f `
            $bytes.Length, $packed.Length, (100 * $packed.Length / $bytes.Length), `
        $endings.Crlf, $endings.BareLf, $same)
    Write-Host ("   header : {0}" -f (($packed[0..9] | ForEach-Object { $_.ToString('x2') }) -join ' '))

    if (-not $same) { throw "Decompressed payload differs from the source: $dst" }
}

Write-Host 'Done. Flash LittleFS: script\littlefs_gen\01_littlefs_gen.bat -> 02_littlefs_gen_flash.bat'
