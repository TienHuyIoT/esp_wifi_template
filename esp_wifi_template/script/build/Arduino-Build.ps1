<#
.SYNOPSIS
    Build / upload esp_wifi_template bang arduino-cli; FQBN lay tu .vscode\arduino.yaml.

.DESCRIPTION
    .vscode\tasks.json goi script nay thay vi hardcode FQBN: board, configuration,
    sketch, output va port deu doc tu .vscode\arduino.yaml (mot nguon chan ly, giong
    script\ota\Flash-OTA.ps1). Doi board option chi can sua arduino.yaml.

.PARAMETER Action
    verify    = compile (khong clean)                      -> .build\verify_log.txt
    rebuild   = compile --clean                            -> .build\verify_log.txt
    upload    = compile --upload (build lai neu can)       -> .build\upload_log.txt
    cliupload = upload (khong build lai, dung .bin co san) -> .build\upload_log.txt

.PARAMETER Port
    COM port. Bo qua -> lay tu .vscode\arduino.yaml (dong 'port:').

.PARAMETER ArduinoVerbose
    Them --verbose cho arduino-cli (soi lenh gcc chi tiet).

.EXAMPLE
    .\Arduino-Build.ps1 -Action verify
    .\Arduino-Build.ps1 -Action upload
    .\Arduino-Build.ps1 -Action cliupload
#>
[CmdletBinding()]
param(
    [ValidateSet('verify', 'rebuild', 'upload', 'cliupload')]
    [string] $Action = 'verify',
    [string] $Port = '',
    [switch] $ArduinoVerbose
)

$ErrorActionPreference = 'Stop'

# sketch root: file nay nam o <root>\script\build\
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$ArduinoCli = if ($env:ARDUINO_CLI) { $env:ARDUINO_CLI } else { 'C:\ArduinoCLI\arduino-cli.exe' }
if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    $cmd = Get-Command $ArduinoCli -ErrorAction SilentlyContinue
    if ($cmd) { $ArduinoCli = $cmd.Source }
    else { throw "Khong tim thay arduino-cli: $ArduinoCli (dat bien moi truong ARDUINO_CLI)" }
}

# ------------------------------------------------------- read arduino.yaml ----
function Get-SketchConfig {
    param([string]$Root)
    $cfg = @{
        board         = 'espressif:esp32:esp32'
        configuration = ''
        sketch        = 'esp_wifi_template.ino'
        output        = '.build'
        port          = 'COM10'
    }
    $yaml = Join-Path $Root '.vscode\arduino.yaml'
    if (Test-Path -LiteralPath $yaml) {
        foreach ($line in Get-Content -LiteralPath $yaml) {
            if     ($line -match '^\s*board:\s*(.+?)\s*$')         { $cfg.board         = $Matches[1] }
            elseif ($line -match '^\s*configuration:\s*(.+?)\s*$') { $cfg.configuration = $Matches[1] }
            elseif ($line -match '^\s*sketch:\s*(.+?)\s*$')        { $cfg.sketch        = $Matches[1] }
            elseif ($line -match '^\s*output:\s*(.+?)\s*$')        { $cfg.output        = $Matches[1] }
            elseif ($line -match '^\s*port:\s*(.+?)\s*$')          { $cfg.port          = $Matches[1] }
        }
        Write-Host '    (config lay tu .vscode\arduino.yaml)' -ForegroundColor DarkGray
    }
    else {
        Write-Warning 'Khong thay .vscode\arduino.yaml - dung gia tri mac dinh.'
    }
    return $cfg
}

$cfg  = Get-SketchConfig -Root $Root
$fqbn = if ($cfg.configuration) { "$($cfg.board):$($cfg.configuration)" } else { $cfg.board }
if (-not $Port) { $Port = $cfg.port }

$buildPath  = [IO.Path]::GetFullPath((Join-Path $Root $cfg.output))
$sketchPath = Join-Path $Root $cfg.sketch
if (-not (Test-Path -LiteralPath $sketchPath)) { throw "Khong thay sketch: $sketchPath" }
if (-not (Test-Path -LiteralPath $buildPath)) { New-Item -ItemType Directory -Force -Path $buildPath | Out-Null }

$logName = if ($Action -in @('verify', 'rebuild')) { 'verify_log.txt' } else { 'upload_log.txt' }
$logPath = Join-Path $buildPath $logName

$cliArgs = @()
switch ($Action) {
    'verify'    { $cliArgs = @('compile', '-b', $fqbn, '--build-path', $buildPath, $sketchPath) }
    'rebuild'   { $cliArgs = @('compile', '-b', $fqbn, '--clean', '--build-path', $buildPath, $sketchPath) }
    'upload'    { $cliArgs = @('compile', '-b', $fqbn, '--upload', '--port', $Port, '--build-path', $buildPath, $sketchPath) }
    'cliupload' { $cliArgs = @('upload',  '-b', $fqbn, '--port', $Port, '--build-path', $buildPath, $sketchPath) }
}
if ($ArduinoVerbose) { $cliArgs += '--verbose' }

Write-Host ('==> arduino-cli ' + ($cliArgs -join ' ')) -ForegroundColor Cyan

# arduino-cli ghi tien do ra STDERR; voi $ErrorActionPreference = 'Stop' dong dau tien
# tro thanh NativeCommandError lam script dung truoc khi doc duoc exit code. Ha muc do
# trong luc goi lenh native va chuyen ErrorRecord thanh dong text sach (nhu Flash-Littlefs.ps1).
$eap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $ArduinoCli @cliArgs 2>&1 | ForEach-Object {
    if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.Exception.Message } else { "$_" }
} | Tee-Object -FilePath $logPath
$code = $LASTEXITCODE
$ErrorActionPreference = $eap

if ($code -eq 0) { Write-Host "OK  $Action thanh cong  (log: $logPath)" -ForegroundColor Green }
else             { Write-Host "XX  $Action that bai (exit $code, log: $logPath)" -ForegroundColor Red }
exit $code
