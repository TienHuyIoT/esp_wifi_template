<#
.SYNOPSIS
    Tao anh LittleFS tu data\ va nap len board esp_wifi_template (board dev, 1 board).

.DESCRIPTION
    Thay cho cap .bat cu (01_littlefs_gen.bat -> 02_littlefs_gen_flash.bat): hai file do
    dung SAI offset/size (ghi 0x290000, anh 0x170000 - nam trong app0) va hoi COM bang
    set /p nen khong tu dong hoa duoc. Script nay:
      1. mklittlefs: data\ -> <OutFile> voi DUNG size partition spiffs
         (16MB: 0x610000/0x9D0000 - 4MB: 0x310000/0xF0000, lay tu
         script\release\release-config.json, fallback hardcode).
      2. Verify anh: size, superblock "littlefs", block_size/block_count, ten file web
         trong anh, SHA-256.
      3. Nap: OTA truoc (espota -s qua script\ota\Flash-OTA.ps1, UDP 3232) hoac UART
         (script\esptool.exe @ offset spiffs). Thu tu uu tien: .clinerules/flash-device.md.

    LUU Y: nap FS THAY ca phan vung LittleFS => mat moi file sinh tren thiet bi
    (log, server1_info.txt, card_http_history_log*.csv ...). Chi nap khi can.

.PARAMETER Mem
    16MB (mac dinh, board moi) hoac 4MB (board cu) - quyet dinh offset/size cua spiffs.

.PARAMETER DataDir
    Thu muc nguon dua vao anh. Mac dinh data\ (ban da deploy). data_org\ la nguon trang,
    phai pack truoc bang 00_pack_web_page.ps1 hoac -Pack.

.PARAMETER OutFile
    File anh xuat ra. Mac dinh .build\littlefs.bin (da gitignore).

.PARAMETER Mode
    none = chi build | ota = build + nap qua IP (mac dinh) | uart = build + nap qua COM.

.PARAMETER Pack
    Danh sach trang trong data_org\ can pack lai truoc khi tao anh
    (vd: -Pack keypad.htm,home.htm).

.PARAMETER SkipBuild
    Dung anh <OutFile> dang co, khong chay mklittlefs.

.PARAMETER NoVerify
    Bo qua buoc kiem tra superblock/ten file trong anh.

.PARAMETER Yes
    Bo qua cau hoi xac nhan truoc khi ghi len board (dung cho VS Code task).

.EXAMPLE
    .\Flash-Littlefs.ps1 -Mode none                    # chi tao anh
    .\Flash-Littlefs.ps1                               # tao anh + nap OTA 192.168.3.102
    .\Flash-Littlefs.ps1 -Mode uart -ComPort COM10      # tao anh + nap qua COM
    .\Flash-Littlefs.ps1 -SkipBuild -Mode ota          # nap lai anh dang co
#>
[CmdletBinding()]
param(
    [ValidateSet('16MB', '4MB')] [string] $Mem = '16MB',
    [string] $DataDir = 'data',
    [string] $OutFile = '.build\littlefs.bin',
    [ValidateSet('none', 'ota', 'uart')] [string] $Mode = 'ota',
    [string] $Ip = '192.168.3.102',
    [int]    $OtaPort = 3232,
    [string] $ComPort = 'COM10',
    [int]    $Baud = 921600,
    [string] $FlashMode = 'dio',
    [switch] $SkipBuild,
    [switch] $NoVerify,
    [switch] $Yes,
    [string[]] $Pack = @()
)

$ErrorActionPreference = 'Stop'

if ($SkipBuild -and $Mode -eq 'none') {
    Write-Host 'XX  -SkipBuild voi -Mode none: khong co gi de lam.' -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- helpers ----
function Write-Step { param([string]$m) Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok   { param([string]$m) Write-Host "OK  $m" -ForegroundColor Green }
function Write-Warn { param([string]$m) Write-Host "!!  $m" -ForegroundColor Yellow }
function Write-Err  { param([string]$m) Write-Host "XX  $m" -ForegroundColor Red }

function Convert-HexToInt64 {
    param([string]$Value)
    return [Convert]::ToInt64(($Value -replace '^0[xX]', ''), 16)
}

# espota/esptool ghi tien do ra STDERR; voi $ErrorActionPreference = 'Stop' dong dau tien
# tro thanh NativeCommandError lam script dung truoc khi doc duoc exit code. Ha muc do
# trong luc goi lenh native, gop 2 luong ra man hinh, tra ve exit code.
function Invoke-Native {
    param([string]$File, [string[]]$Arguments)
    $eap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $File @Arguments 2>&1 | ForEach-Object {
        if ($_ -is [System.Management.Automation.ErrorRecord]) { Write-Host $_.Exception.Message }
        else { Write-Host "$_" }
    }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $eap
    return $code
}

# Tim chuoi ASCII trong $Limit byte dau cua $Bytes (tra ve offset, -1 = khong thay).
function Find-FirstBytePattern {
    param([byte[]]$Bytes, [string]$Ascii, [int]$Limit = 64)
    $needle = [System.Text.Encoding]::ASCII.GetBytes($Ascii)
    $max = [Math]::Min($Limit, $Bytes.Length - $needle.Length)
    for ($i = 0; $i -le $max; $i++) {
        $hit = $true
        for ($j = 0; $j -lt $needle.Length; $j++) {
            if ($Bytes[$i + $j] -ne $needle[$j]) { $hit = $false; break }
        }
        if ($hit) { return $i }
    }
    return -1
}

# Tim gia tri little-endian (4 byte, hoac 2 byte khi <= 0xFFFF) trong $Limit byte dau.
function Find-LePattern {
    param([byte[]]$Bytes, [uint32]$Value, [int]$Limit = 64)
    foreach ($width in 4, 2) {
        if ($width -eq 2 -and $Value -gt 0xFFFF) { continue }
        $pat = New-Object 'byte[]' $width
        for ($k = 0; $k -lt $width; $k++) { $pat[$k] = [byte](($Value -shr (8 * $k)) -band 0xFF) }
        $max = [Math]::Min($Limit, $Bytes.Length - $width)
        for ($i = 0; $i -le $max; $i++) {
            $hit = $true
            for ($j = 0; $j -lt $width; $j++) { if ($Bytes[$i + $j] -ne $pat[$j]) { $hit = $false; break } }
            if ($hit) { return $i }
        }
    }
    return -1
}

# ------------------------------------------------------------------- root ----
# File nay nam o <root>\script\littlefs_gen\
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Resolve-RootPath {
    param([string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $Root $Path))
}

$Mklittlefs = if ($env:MKLITTLEFS_EXE) { $env:MKLITTLEFS_EXE } else { Join-Path $Root 'script\littlefs_gen\mklittlefs.exe' }
$Esptool    = if ($env:ESPTOOL_EXE)    { $env:ESPTOOL_EXE }    else { Join-Path $Root 'script\esptool.exe' }
$OtaScript  = Join-Path $Root 'script\ota\Flash-OTA.ps1'
$PackScript = Join-Path $Root 'script\littlefs_gen\00_pack_web_page.ps1'

# ----------------------------------------------------- partition spiffs -------
# Nguon chan ly: script\release\release-config.json (memories[].expectedSpiffs).
# Fallback theo .clinerules/flash-device.md:
#   16MB (app3M_spiffs9M_16MB): spiffs 0x610000 / 0x9D0000 (10.289.152 B)
#    4MB (web485_v2)          : spiffs 0x310000 / 0xF0000  (983.040 B)
$layouts = @{
    '16MB' = @{ Offset = '0x610000'; Size = '0x9D0000' }
    '4MB'  = @{ Offset = '0x310000'; Size = '0xF0000' }
}
$layoutSource = 'hardcode'
$configPath = Join-Path $Root 'script\release\release-config.json'
if (Test-Path -LiteralPath $configPath) {
    try {
        $cfg = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
        $hit = @($cfg.memories | Where-Object { $_.key -eq $Mem })
        if ($hit.Count -gt 0 -and $hit[0].expectedSpiffs) {
            $fromCfg = @{ Offset = $hit[0].expectedSpiffs.offset; Size = $hit[0].expectedSpiffs.size }
            if (($fromCfg.Offset -ne $layouts[$Mem].Offset) -or ($fromCfg.Size -ne $layouts[$Mem].Size)) {
                Write-Warn "$Mem : release-config.json = $($fromCfg.Offset)/$($fromCfg.Size), hardcode = $($layouts[$Mem].Offset)/$($layouts[$Mem].Size) - dung theo release-config.json"
            }
            $layouts[$Mem] = $fromCfg
            $layoutSource = 'release-config.json'
        }
    }
    catch { Write-Warn "Khong doc duoc release-config.json ($($_.Exception.Message)) - dung gia tri hardcode." }
}
$offsetHex  = $layouts[$Mem].Offset
$sizeHex    = $layouts[$Mem].Size
$sizeBytes  = Convert-HexToInt64 -Value $sizeHex
$blockSize  = 4096
$blockCount = [uint32]($sizeBytes / $blockSize)

$dataPath = Resolve-RootPath -Path $DataDir
$outPath  = Resolve-RootPath -Path $OutFile

# ----------------------------------------------------------------- banner -----
Write-Host ''
Write-Host '========================================================' -ForegroundColor DarkCyan
Write-Host ' esp_wifi_template - LittleFS: tao anh + nap (OTA / UART)' -ForegroundColor Cyan
Write-Host '========================================================' -ForegroundColor DarkCyan
Write-Host "    Root      : $Root"
Write-Host "    Memory    : $Mem  (spiffs $offsetHex / $sizeHex = $sizeBytes B, nguon: $layoutSource)"
Write-Host "    Data      : $dataPath"
Write-Host "    Image     : $outPath"
Write-Host "    Mode      : $Mode"
if ($Mode -eq 'ota')  { Write-Host "    Board IP  : $($Ip):$OtaPort (UDP, espota -s)" }
if ($Mode -eq 'uart') { Write-Host "    COM       : $ComPort @ $Baud" }
Write-Host ''

# ------------------------------------------------------------- pack (tuy chon)
if ($Pack.Count -gt 0 -and -not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $PackScript)) { Write-Err "Khong thay $PackScript"; exit 1 }
    Write-Step "Pack trang tu data_org\ : $($Pack -join ', ')"
    try { & $PackScript -Name $Pack }
    catch { Write-Err "Pack trang that bai: $($_.Exception.Message)"; exit 1 }
    Write-Ok 'Pack trang xong.'
}

# ------------------------------------------------------------------ build -----
if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $dataPath)) { Write-Err "Khong thay thu muc nguon: $dataPath"; exit 1 }
    if (-not (Test-Path -LiteralPath $Mklittlefs)) { Write-Err "Khong thay mklittlefs: $Mklittlefs"; exit 1 }

    $dataSize = (Get-ChildItem -LiteralPath $dataPath -Recurse -File -ErrorAction SilentlyContinue |
                 Measure-Object -Property Length -Sum).Sum
    if (-not $dataSize) { $dataSize = 0 }
    Write-Step ("Build anh LittleFS: {0:N0} B du lieu -> {1:N0} B ({2:P1} phan vung)" -f `
                $dataSize, $sizeBytes, ($dataSize / $sizeBytes))
    if ($dataSize -gt $sizeBytes) {
        Write-Err "data\ ($dataSize B) LON HON partition spiffs ($sizeBytes B)."
        exit 1
    }

    $outDir = Split-Path -Parent $outPath
    if (-not (Test-Path -LiteralPath $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    # KHONG them '\' cuoi: PowerShell boc arg co space bang "..." nen "path\" lam backslash
    # thoat luon dau " => mklittlefs tach arg sai (PARSE ERROR: Argument: Project).
    $srcArg = $dataPath.TrimEnd('\')
    $mkArgs = @('-c', $srcArg, '-d', '0', '-b', "$blockSize", '-p', '256', '-s', $sizeHex, $outPath)
    Write-Host "    > `"$Mklittlefs`" $($mkArgs -join ' ')" -ForegroundColor DarkGray
    $mkCode = Invoke-Native -File $Mklittlefs -Arguments $mkArgs
    # mklittlefs 0.2.3 trong repo bi assertion o che do -l/-u => khong ket luan theo exit
    # code; xac nhan bang kich thuoc file ket qua.
    if ($mkCode -ne 0) { Write-Warn "mklittlefs exit $mkCode - kiem tra file ket qua..." }
    if (-not (Test-Path -LiteralPath $outPath)) { Write-Err "Khong tao duoc anh: $outPath"; exit 1 }
    $builtSize = (Get-Item -LiteralPath $outPath).Length
    if ($builtSize -ne $sizeBytes) {
        Write-Err "Anh tao ra $builtSize B, phai la $sizeBytes B ($sizeHex)."
        exit 1
    }
    Write-Ok ("Anh LittleFS: {0:N0} B" -f $builtSize)
}
elseif (-not (Test-Path -LiteralPath $outPath)) {
    Write-Err "Khong thay anh: $outPath  (bo -SkipBuild de tao moi)"
    exit 1
}

$imgSize = (Get-Item -LiteralPath $outPath).Length
$sha256  = (Get-FileHash -LiteralPath $outPath -Algorithm SHA256).Hash

# ----------------------------------------------------------------- verify -----
if (-not $NoVerify) {
    Write-Step 'Verify anh LittleFS'
    $bytes = [System.IO.File]::ReadAllBytes($outPath)

    if ($bytes.Length -ne $sizeBytes) {
        Write-Err "Size sai: $($bytes.Length) B (mong doi $sizeBytes B = $sizeHex)."
        exit 1
    }

    $magicOff = Find-FirstBytePattern -Bytes $bytes -Ascii 'littlefs' -Limit 64
    if ($magicOff -lt 0) {
        Write-Err 'Khong thay superblock "littlefs" trong 64 byte dau => anh khong hop le.'
        exit 1
    }
    # Offset tung field trong superblock phu thuoc phien ban littlefs => khong doc cung mot
    # offset; tim pattern little-endian cua block_size / block_count trong 64 byte dau.
    $hdrHex = (($bytes[0..31] | ForEach-Object { $_.ToString('x2') }) -join ' ')
    $offSize  = Find-LePattern -Bytes $bytes -Value ([uint32]$blockSize) -Limit 64
    $offCount = Find-LePattern -Bytes $bytes -Value $blockCount -Limit 64
    if ($offSize -lt 0 -or $offCount -lt 0) {
        Write-Err "Superblock KHONG dung: khong thay block_size=$blockSize / block_count=$blockCount trong 64 byte dau."
        Write-Host "    header : $hdrHex" -ForegroundColor DarkGray
        exit 1
    }
    Write-Ok ("Superblock: magic@$magicOff, block_size@$offSize, block_count@$offCount (0x{0:X} = {1} block)" -f $blockCount, $blockCount)
    Write-Host "    header : $hdrHex" -ForegroundColor DarkGray

    # ten file web phai nam trong anh (giong cach kiem chung cua script/release)
    $latin1 = [System.Text.Encoding]::GetEncoding(28591)
    $text = $latin1.GetString($bytes)
    $found = @()
    $missing = @()
    foreach ($name in @('home.htm.gz', 'mystyle.css.gz', 'index.htm.gz')) {
        if ($text.IndexOf($name) -ge 0) { $found += $name } else { $missing += $name }
    }
    if ($found.Count -gt 0)   { Write-Ok "Ten file trong anh: $($found -join ', ')" }
    if ($missing.Count -gt 0) { Write-Warn "Khong thay trong anh: $($missing -join ', ')" }
    if ($found.Count -eq 0)   { Write-Warn 'Khong thay ten file web nao trong anh - kiem tra lai data\.' }
}

Write-Host ("    Size    : {0:N0} B" -f $imgSize) -ForegroundColor DarkGray
Write-Host "    SHA-256 : $sha256" -ForegroundColor DarkGray

# ------------------------------------------------------------------ flash -----
if ($Mode -ne 'none') {
    if (-not $Yes) {
        Write-Host ''
        Write-Warn 'Nap FS THAY TOAN BO phan vung LittleFS tren board:'
        Write-Host '     moi file sinh tren thiet bi (log, server1_info.txt, card_http_history_log*.csv ...) se MAT.' -ForegroundColor Yellow
        $ans = Read-Host 'Tiep tuc nap? (y/N)'
        if ($ans -notmatch '^(y|Y)(es|ES)?$') { Write-Warn 'Da huy nap (khong ghi gi len board).'; exit 0 }
    }

    if ($Mode -eq 'ota') {
        if (-not (Test-Path -LiteralPath $OtaScript)) { Write-Err "Khong thay $OtaScript"; exit 1 }
        Write-Step "Nap OTA qua IP $($Ip):$OtaPort (espota -s)"
        # Truyen -Bin TUONG MINH: neu de Flash-OTA.ps1 tu tim, no se chon file
        # littlefs*.bin MOI NHAT theo mtime trong script\littlefs_gen / script\bin
        # (co the la anh sai size 0x170000 hoac anh 0xF0000 cua board 4MB).
        $otaArgs = @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $OtaScript,
            '-Target', 'littlefs', '-Bin', $outPath, '-Ip', $Ip, '-Port', "$OtaPort"
        )
        $code = Invoke-Native -File 'powershell.exe' -Arguments $otaArgs
        if ($code -eq 0) {
            Write-Ok 'OTA THANH CONG - board se tu khoi dong lai.'
        }
        else {
            Write-Err "OTA THAT BAI (exit $code)."
            Write-Host "    Du phong UART (luon nap duoc): .\script\littlefs_gen\Flash-Littlefs.ps1 -SkipBuild -Mode uart -ComPort $ComPort" -ForegroundColor Yellow
            exit $code
        }
    }
    else {
        if (-not (Test-Path -LiteralPath $Esptool)) { Write-Err "Khong thay esptool: $Esptool"; exit 1 }
        Write-Step "Nap UART qua $ComPort @ $Baud -> offset $offsetHex"
        Write-Host '    (chi 1 tien trinh duoc giu cong COM: dong Serial Monitor / Teleplot truoc)' -ForegroundColor DarkYellow
        $espArgs = @(
            '--chip', 'esp32', '--port', $ComPort, '--baud', "$Baud",
            '--before', 'default_reset', '--after', 'hard_reset',
            'write_flash', '-z', '--flash_mode', $FlashMode, '--flash_freq', '80m', '--flash_size', 'detect',
            $offsetHex, $outPath
        )
        Write-Host "    > `"$Esptool`" $($espArgs -join ' ')" -ForegroundColor DarkGray
        $code = Invoke-Native -File $Esptool -Arguments $espArgs
        if ($code -eq 0) { Write-Ok 'UART THANH CONG - board se tu khoi dong lai.' }
        else { Write-Err "UART THAT BAI (esptool exit $code)."; exit $code }
    }
}

# ---------------------------------------------------------------- summary -----
Write-Host ''
Write-Host '--------------------------------------------------------' -ForegroundColor DarkCyan
Write-Ok "Anh LittleFS: $outPath  ($imgSize B)"
Write-Host "    SHA-256 : $sha256" -ForegroundColor DarkGray
if ($Mode -eq 'none') {
    Write-Host '    Nap tiep (khong build lai):' -ForegroundColor DarkGray
    Write-Host "      .\script\littlefs_gen\Flash-Littlefs.ps1 -SkipBuild -Mode ota  -Ip $Ip" -ForegroundColor DarkGray
    Write-Host "      .\script\littlefs_gen\Flash-Littlefs.ps1 -SkipBuild -Mode uart -ComPort $ComPort" -ForegroundColor DarkGray
}
else {
    Write-Host '    Kiem chung sau khi nap: node script/flash/board_status.js' -ForegroundColor DarkGray
    Write-Host '    Mo trang web tren board nho Ctrl+F5 (trinh duyet giu cache ban cu).' -ForegroundColor DarkGray
}
exit 0
