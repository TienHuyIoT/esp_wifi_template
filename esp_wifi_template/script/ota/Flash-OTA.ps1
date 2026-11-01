<#
.SYNOPSIS
    Nap firmware esp_wifi_template len ESP32 qua mang (OTA / ArduinoOTA / espota).

.DESCRIPTION
    Firmware co OTA_ARDUINO_ENABLE = 1 (THIoT_ESPConfig.h) va goi ArduinoOTA.begin()
    trong THIoT_ESPWifiHandle.cpp => board lang nghe OTA tren cong UDP 3232.
    Script nay goi espota.exe (hoac espota.py) cua ESP32 platform de day file .bin
    qua IP, khong can cam cap USB.

.PARAMETER Ip
    IP cua board. Mac dinh 192.168.3.102.

.PARAMETER Port
    Cong OTA (UDP). Mac dinh 3232 (ArduinoOTA default).

.PARAMETER Auth
    Mat khau OTA. Firmware hien tai KHONG dat password => de trong.

.PARAMETER Bin
    Duong dan file .bin muon nap. Bo trong => dung file trong .build
    (hoac script\littlefs_gen voi -Target littlefs).

.PARAMETER Build
    Bien dich lai bang arduino-cli truoc khi OTA.

.PARAMETER Target
    'app' (firmware, mac dinh) hoac 'littlefs' (anh LittleFS, gui bang espota -s).

.PARAMETER Timeout
    Timeout cho moi goi tin OTA (giay). Mac dinh 20.

.PARAMETER OtaDebug
    Bat log debug cua espota.

.PARAMETER SkipPing
    Bo qua buoc kiem tra ping.

.EXAMPLE
    .\Flash-OTA.ps1
    .\Flash-OTA.ps1 -Ip 192.168.3.102 -Build
    .\Flash-OTA.ps1 -Target littlefs
    .\Flash-OTA.ps1 -OtaDebug -Timeout 60
#>
[CmdletBinding()]
param(
    [string] $Ip      = '192.168.3.102',
    [int]    $Port    = 3232,
    [string] $Auth    = '',
    [string] $Bin     = '',
    [switch] $Build,
    [ValidateSet('app', 'littlefs')]
    [string] $Target  = 'app',
    [int]    $Timeout = 20,
    [switch] $OtaDebug,
    [switch] $SkipPing
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- helpers ----
function Write-Step { param([string]$m) Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok   { param([string]$m) Write-Host "OK  $m" -ForegroundColor Green }
function Write-Warn { param([string]$m) Write-Host "!!  $m" -ForegroundColor Yellow }
function Write-Err  { param([string]$m) Write-Host "XX  $m" -ForegroundColor Red }

# sketch root: file nay nam o <root>\script\ota\
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$ArduinoCli = if ($env:ARDUINO_CLI) { $env:ARDUINO_CLI } else { 'C:\ArduinoCLI\arduino-cli.exe' }

# ------------------------------------------------------- read arduino.yaml ----
function Get-SketchConfig {
    param([string]$Root)
    $cfg = @{
        board         = 'espressif:esp32:esp32'
        configuration = 'JTAGAdapter=default,PSRAM=disabled,PartitionScheme=web485_v2,CPUFreq=240,FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,LoopCore=1,EventsCore=1,DebugLevel=none,EraseFlash=none'
        sketch        = 'esp_wifi_template.ino'
        output        = '.build'
    }
    $yaml = Join-Path $Root '.vscode\arduino.yaml'
    if (Test-Path $yaml) {
        foreach ($line in Get-Content -LiteralPath $yaml) {
            if     ($line -match '^\s*board:\s*(.+?)\s*$')         { $cfg.board         = $Matches[1] }
            elseif ($line -match '^\s*configuration:\s*(.+?)\s*$') { $cfg.configuration = $Matches[1] }
            elseif ($line -match '^\s*sketch:\s*(.+?)\s*$')        { $cfg.sketch        = $Matches[1] }
            elseif ($line -match '^\s*output:\s*(.+?)\s*$')        { $cfg.output        = $Matches[1] }
        }
        Write-Host "    (config lay tu .vscode\arduino.yaml)" -ForegroundColor DarkGray
    }
    else {
        Write-Warn "Khong thay .vscode\arduino.yaml - dung config mac dinh."
    }
    return $cfg
}

# ----------------------------------------------------------- find espota -----
function Find-Espota {
    param([string]$Root)

    $candidates = @()
    if ($env:ESPOTA_EXE) { $candidates += $env:ESPOTA_EXE }
    $candidates += (Join-Path $Root 'script\bin\espota.exe')
    $candidates += (Join-Path $env:USERPROFILE 'Documents\Arduino\hardware\espressif\esp32\tools\espota.exe')

    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return (Resolve-Path -LiteralPath $c).Path }
    }

    # Arduino15 (core cai qua Boards Manager)
    $a15 = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32'
    if (Test-Path -LiteralPath $a15) {
        $hit = Get-ChildItem -LiteralPath $a15 -Filter 'espota.exe' -Recurse -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

function Find-EspotaPy {
    param([string]$Root)
    $py = Join-Path $env:USERPROFILE 'Documents\Arduino\hardware\espressif\esp32\tools\espota.py'
    if (Test-Path -LiteralPath $py) { return $py }
    $a15 = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32'
    if (Test-Path -LiteralPath $a15) {
        $hit = Get-ChildItem -LiteralPath $a15 -Filter 'espota.py' -Recurse -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

function Get-PythonCmd {
    foreach ($c in 'python', 'python3', 'py') {
        $cmd = Get-Command $c -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

# ----------------------------------------------------------------- banner ----
Write-Host ''
Write-Host '========================================================' -ForegroundColor DarkCyan
Write-Host ' esp_wifi_template - OTA (ArduinoOTA / espota) qua IP' -ForegroundColor Cyan
Write-Host '========================================================' -ForegroundColor DarkCyan
Write-Host "    Root      : $Root"
Write-Host "    Board IP  : $($Ip):$Port"
Write-Host "    Target    : $Target"
if ($Auth) { Write-Host "    Auth      : **** (co password)" } else { Write-Host "    Auth      : (khong password)" }
Write-Host ''

# ------------------------------------------------------- locate espota tool --
$espotaExe = Find-Espota -Root $Root
if ($espotaExe) {
    Write-Ok "espota.exe: $espotaExe"
    $espotaCmd  = $espotaExe
    $espotaBase = @()
}
else {
    $espotaPy = Find-EspotaPy -Root $Root
    if (-not $espotaPy) {
        Write-Err 'Khong tim thay espota.exe / espota.py.'
        Write-Host '  Cai ESP32 core (Boards Manager) hoac dat bien moi truong ESPOTA_EXE.' -ForegroundColor Yellow
        exit 1
    }
    $python = Get-PythonCmd
    if (-not $python) {
        Write-Err "Khong tim thay Python de chay $espotaPy"
        exit 1
    }
    Write-Ok "espota.py : $espotaPy  (qua $python)"
    $espotaCmd  = $python
    $espotaBase = @($espotaPy)
}

# -------------------------------------------------------------- build bin ----
$cfg       = Get-SketchConfig -Root $Root
$fqbn      = if ($cfg.configuration) { "$($cfg.board):$($cfg.configuration)" } else { $cfg.board }
$buildPath = [IO.Path]::GetFullPath((Join-Path $Root $cfg.output))
$sketch    = Join-Path $Root $cfg.sketch

$appBin = Join-Path $buildPath ([IO.Path]::GetFileNameWithoutExtension($cfg.sketch) + '.ino.bin')

if (-not $Bin) {
    if ($Target -eq 'app') {
        $Bin = $appBin
    }
    else {
        # tim anh littlefs moi nhat trong vai noi quen thuoc
        $searchDirs = @(
            (Join-Path $Root 'script\littlefs_gen'),
            (Join-Path $Root 'script\bin')
        )
        $Bin = $searchDirs |
               Where-Object { Test-Path -LiteralPath $_ } |
               ForEach-Object {
                   Get-ChildItem -LiteralPath $_ -Filter '*.bin' -ErrorAction SilentlyContinue |
                   Where-Object { $_.Name -match 'littlefs|littlfs|little_fs|spiffs' }
               } |
               Sort-Object LastWriteTime -Descending |
               Select-Object -First 1 -ExpandProperty FullName
        if (-not $Bin) {
            Write-Err 'Khong thay anh littlefs (script\littlefs_gen hoac script\bin). Chay 01_littlefs_gen.bat truoc.'
            exit 1
        }
    }
}

if ($Build) {
    if (-not (Test-Path -LiteralPath $ArduinoCli)) {
        Write-Err "Khong thay arduino-cli: $ArduinoCli  (dat bien ARDUINO_CLI de doi duong dan)"
        exit 1
    }
    Write-Step "Bien dich: $($cfg.sketch)"
    Write-Host "    FQBN: $fqbn" -ForegroundColor DarkGray
    # KHONG dung --clean (rebuild toan bo rat cham)
    & $ArduinoCli compile -b $fqbn --build-path $buildPath $sketch
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Compile that bai (exit $LASTEXITCODE)."
        exit 1
    }
    Write-Ok 'Compile xong.'
}

if (-not (Test-Path -LiteralPath $Bin)) {
    Write-Warn "Chua co file .bin: $Bin"
    Write-Host '  Chay lai voi -Build de bien dich truoc, hoac build bang Arduino: Verify.' -ForegroundColor Yellow
    exit 1
}
$binSize = [math]::Round((Get-Item -LiteralPath $Bin).Length / 1KB, 1)
Write-Ok "Image: $Bin  ($binSize KB)"

# --------------------------------------------------------------- preflight ---
if (-not $SkipPing) {
    Write-Step "Kiem tra ket noi toi $Ip ..."
    $alive = Test-Connection -ComputerName $Ip -Count 2 -Quiet
    if (-not $alive) {
        Write-Warn "Khong ping duoc $Ip. Van thu OTA (mot so AP chan ICMP)."
    }
    else {
        Write-Ok "$Ip phan hoi ping."
    }
}

# --------------------------------------------------------------- run espota --
$espotaArgs = @()
$espotaArgs += $espotaBase
$espotaArgs += @('-i', $Ip, '-p', "$Port", '--auth=' + $Auth, '-f', $Bin, '-t', "$Timeout")
if ($Target -eq 'littlefs') { $espotaArgs += '-s' }
if ($OtaDebug)             { $espotaArgs += '-d' }
else                       { $espotaArgs += '-r' }

Write-Step 'Bat dau nap OTA...'
Write-Host ("    > `"$espotaCmd`" " + ($espotaArgs -join ' ')) -ForegroundColor DarkGray
Write-Host ''
Write-Host '    (neu Windows Firewall hoi, phai cho phep espota.exe)' -ForegroundColor DarkYellow
Write-Host ''

# espota writes lines such as "Sending invitation ..." to STDERR. While
# $ErrorActionPreference = 'Stop' (set at the top of this script) PowerShell turns
# the first of those into a TERMINATING NativeCommandError and the script aborts
# before it can even read espota's exit code. Relax the preference for the native
# call and turn the merged streams into plain text; $LASTEXITCODE still carries
# espota's exit code.
$eapSaved = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $espotaCmd @espotaArgs 2>&1 | ForEach-Object {
    if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.Exception.Message } else { "$_" }
}
$code = $LASTEXITCODE
$ErrorActionPreference = $eapSaved
Write-Host ''

if ($code -eq 0) {
    Write-Ok 'OTA THANH CONG - board se tu khoi dong lai.'
    Write-Host '    Kiem tra log boot tren serial: "==== Firmware version ... ===="' -ForegroundColor DarkGray
    exit 0
}
else {
    Write-Err "OTA THAT BAI (espota exit $code)."
    Write-Host '    Goi y: kiem tra IP/port 3232, board dang chay firmware co OTA, tat firewall,' -ForegroundColor Yellow
    Write-Host '           va dam bao chi 1 board dung IP do.' -ForegroundColor Yellow
    exit $code
}
