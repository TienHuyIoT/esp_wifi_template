<#
================================================================================
 TeraTerm Console - esp_wifi_template        (PowerShell + WinForms)            v2.0
--------------------------------------------------------------------------------
 Serial-console manager for the esp_wifi_template board.  Drives a Tera Term session
 through the file-IPC contract implemented by tt_console_watcher.ttl.

 ACTIONS
   Open / Close        open a Tera Term session, or close it (log is flushed)
   Pause / Start       disconnect / reconnect the serial port (TT stays open)
   Start / Stop Log    single logging toggle - the session stays open
   Restart Device      press-and-hold RTS reset, or latch it with "Hold on click"

 KEYBOARD
   Ctrl+O   open / close session          Ctrl+L   start / stop logging
   Ctrl+P   pause / resume the port       Ctrl+R   restart the device (tap)
   F5       rescan COM ports              Ctrl+E   clear the activity list
   Ctrl+M   live debug monitor (follows the log, with timestamps)
   Ctrl+D   open the log folder           Ctrl+K   copy the current log path
   F1       shortcuts / about             Esc      dismiss the message

 RESET (RST) POLICY
   * Opening or reconnecting the port never drives DTR or RTS: the runtime INI
     forces FlowCtrlRTS=0 and FlowCtrlDTR=0, so Tera Term cannot pulse the
     board auto-reset circuit while it connects.
   * DTR is never driven at all - not by Tera Term (FlowCtrlDTR=0) and not by
     always parked in its released state before Tera Term is closed - also
     when the window is closed while Tera Term keeps running.

 UX NOTES
   * Header pill + status card always show the live session state.  The banner
     is a cached bitmap that is only redrawn when the state really changes, so
     the indicator never flickers, and the link state is debounced so it cannot
     flap between Connected and No link.
   * Activity list: colour-coded event feed (newest at the bottom, 400 max).
   * A reserved message bar under the buttons carries all feedback (guidance
     when idle, colour-coded notice after an action, marquee while a command
     runs).  It never covers other content and never shifts the layout, and
     short commands show nothing at all (250 ms delay + minimum display time).
   * Buttons are never disabled or hidden during work; re-entrancy is blocked
     internally, so the UI stays visually stable while commands run.
   * The ttpmacro helper window is hidden by a WinEvent hook the instant it is
     created - opening a session shows no extra popup at all.
   * Field-level validation highlights the offending input instead of a popup.
   * Window geometry, COM port, paths and every toggle are remembered in
     .build\console_ui.json.
   * The UI is DPI aware and the window is resizable (minimum size enforced).
   * Design tokens follow the UI/UX Pro Max rules: 4.5:1 text contrast, 8 px
     spacing rhythm, semantic colours, focus-visible fields, empty states and
     error messages that name the cause and the recovery step.

 FILES (script\tera_console\)
   TeraTermConsole.cmd        double-click launcher (hidden, no console flash)
   TeraTermConsole.vbs        hidden relauncher for the .ps1
   TeraTermConsole.ps1        this UI (single file, ASCII only)
   tt_console_watcher.ttl     TTL macro template (launched with /M=)
   .build\                    runtime: settings, generated watcher, ctrl /
                              status files, logs (git-ignored)

 CONTROL FLOW  (file IPC - do not change without updating the TTL macro)
   UI      -> .build\ctrl_<sid>.txt      <seq>|<CMD>|<arg1>|<arg2>|END
   watcher -> .build\status_<sid>.txt    first line only:
       -1|READY||<link>                  watcher started
       <seq>|LOGGING|<logpath>|<link>    logopen succeeded
       <seq>|ERROR|<logpath>|<link>      logopen failed
       <seq>|IDLE|<logpath>|<link>       logging stopped
       FINAL|QUIT|                       Tera Term closing
   <link> = testlink value (0 = no link, 1 = disconnected, 2 = connected),
   refreshed by the watcher heartbeat (~1 s).

 DEV HELPERS
   powershell -File TeraTermConsole.ps1 -UiPreview shot.png [-Demo]
     renders the UI to a PNG and exits (no session, nothing is saved).
================================================================================
#>

[CmdletBinding()]
param(
    # Dev helper: render the UI into this PNG file and exit immediately.
    [string]$UiPreview = '',
    # With -UiPreview: fill the window with sample data (running + logging).
    [switch]$Demo
)

trap {
    $msg = 'FATAL: ' + $_.Exception.Message + [Environment]::NewLine + $_.ScriptStackTrace
    try {
        Add-Content -LiteralPath (Join-Path $PSScriptRoot '.build\ui_error.log') -Value ('[' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + '] ' + $msg)
    } catch { }
    try { [System.Windows.Forms.MessageBox]::Show($msg, 'TeraTerm Console - Error', 'OK', 'Error') | Out-Null } catch { }
    exit 1
}

$CRLF      = [string][char]13 + [string][char]10
$AppTitle  = 'TeraTerm Console - esp_wifi_template'
$AppId     = 'EspWifiTemplate.TeraTermConsole'
$MutexName = 'Local\TeraTermConsole_esp_wifi_template'

# --------------------------------------------------------------- interop / dpi
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -Namespace Win32Util -Name Native -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
[DllImport("shell32.dll", CharSet = CharSet.Unicode)] public static extern int SetCurrentProcessExplicitAppUserModelID(string appId);
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
// 1 ms timer resolution: without it Start-Sleep/timers quantise to ~15.6 ms,
// which is long enough for the ttpmacro window to be visible for one frame.
[DllImport("winmm.dll", EntryPoint = "timeBeginPeriod")] public static extern uint TimeBeginPeriod(uint ms);
[DllImport("winmm.dll", EntryPoint = "timeEndPeriod")] public static extern uint TimeEndPeriod(uint ms);

private delegate void MacroWinEventHook(IntPtr hHook, uint ev, IntPtr hwnd, int idObject, int idChild, uint thread, uint time);
[DllImport("user32.dll")] private static extern IntPtr SetWinEventHook(uint eventMin, uint eventMax, IntPtr hmod, MacroWinEventHook proc, uint idProcess, uint idThread, uint flags);
[DllImport("user32.dll")] private static extern bool UnhookWinEvent(IntPtr hHook);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetWindowTextW(IntPtr hWnd, System.Text.StringBuilder text, int count);
private delegate bool MacroEnumProc(IntPtr hwnd, IntPtr param);
[DllImport("user32.dll")] private static extern bool EnumWindows(MacroEnumProc proc, IntPtr param);
[DllImport("user32.dll")] private static extern int GetWindowLong(IntPtr hWnd, int index);
[DllImport("user32.dll")] private static extern int SetWindowLong(IntPtr hWnd, int index, int value);
[DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);

private static MacroWinEventHook _macroProc;
private static IntPtr _macroHook = IntPtr.Zero;
private static IntPtr _macroHookName = IntPtr.Zero;
private static string _macroMatch = null;
private static int _macroPid = 0;
private static string _macroLog = null;
private static System.Threading.Thread _macroThread = null;
private static volatile bool _macroRun = false;
private static readonly object _macroLock = new object();

public static bool StartMacroGuard(string titlePart) { return StartMacroGuard(titlePart, 0); }

// Optional diagnostics: every hidden window is appended here so a leftover
// flash can be traced on the machine where it happens.
public static void SetGuardLog(string path) {
    _macroLog = path;
    if (path != null && path.Length > 0) { try { if (System.IO.File.Exists(path)) { System.IO.File.Delete(path); } } catch { } }
}

// A window belongs to the guard when its caption carries our macro file name,
// or - once the pid is known - when it is the macro dialog's template caption
// "MACRO".  Error dialogs keep their own caption and are never hidden.
private static bool IsMacroWindow(IntPtr hwnd, System.Text.StringBuilder sb) {
    if (GetWindowTextW(hwnd, sb, sb.Capacity) <= 0) { return false; }
    string title = sb.ToString();
    if (title.Length == 0) { return false; }
    if (_macroMatch != null && _macroMatch.Length > 0 && title.IndexOf(_macroMatch, System.StringComparison.OrdinalIgnoreCase) >= 0) { return true; }
    if (_macroPid > 0 && title.Trim().Equals("MACRO", System.StringComparison.OrdinalIgnoreCase)) { return true; }
    return false;
}

private static void LogLine(string what, string title) {
    if (_macroLog == null || _macroLog.Length == 0) { return; }
    lock (_macroLock) {
        try { System.IO.File.AppendAllText(_macroLog, System.DateTime.Now.ToString("HH:mm:ss.fff") + "  " + what + " '" + title + "'" + System.Environment.NewLine); } catch { }
    }
}

private static void HideIfMacro(IntPtr hwnd) {
    var sb = new System.Text.StringBuilder(400);
    if (!IsMacroWindow(hwnd, sb)) { return; }
    // Stage 1: hide it FIRST, before anything else, so the window cannot be
    // painted even for a single frame.
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, 0);
        LogLine("hid", sb.ToString());
    }
    // Stage 2: park it far outside the desktop.  Every later ShowWindow by the
    // macro host then renders off-screen, so the helper window stays invisible
    // whatever the timing is - this no longer depends on how fast we react.
    try { SetWindowPos(hwnd, IntPtr.Zero, -32000, -32000, 0, 0, 0x0001 | 0x0004 | 0x0010); } catch { }
    // Stage 3: drop the task bar button (WS_EX_TOOLWINDOW): Windows animates a
    // new task bar button for ~200 ms, which reads as a flash even when the
    // window itself never paints.
    try {
        int ex = GetWindowLong(hwnd, -20);
        if ((ex & 0x00000080) == 0) {
            SetWindowLong(hwnd, -20, ex | 0x00000080);
            LogLine("notaskbar", sb.ToString());
        }
    } catch { }
    // Stage 4: never let it take focus (WS_EX_NOACTIVATE) - an activated window
    // is raised to the top of the z-order and can steal a visible frame.
    try {
        int ex2 = GetWindowLong(hwnd, -20);
        if ((ex2 & 0x08000000) == 0) {
            SetWindowLong(hwnd, -20, ex2 | 0x08000000);
            LogLine("noactivate", sb.ToString());
        }
    } catch { }
    // Belt and braces: if it managed to show itself between the stages, hide it
    // again (no log line, so 'hid' never gets duplicated).
    try { if (IsWindowVisible(hwnd)) { ShowWindow(hwnd, 0); } } catch { }
}

// Background sweep: independent of the UI thread and of any message pump, so
// the helper window is hidden a couple of milliseconds after it is created even
// when the interface is busy.  Fast for the first 12 s, then a cheap safety net.
private static void MacroWatchLoop() {
    var sw = System.Diagnostics.Stopwatch.StartNew();
    while (_macroRun) {
        try {
            EnumWindows(delegate(IntPtr h, IntPtr l) { HideIfMacro(h); return true; }, IntPtr.Zero);
        } catch { }
        long ms = sw.ElapsedMilliseconds;
        if (ms < 3000) { System.Threading.Thread.SpinWait(200); }   // stay on the core: ~microsecond latency
        else if (ms < 15000) { System.Threading.Thread.Sleep(2); }  // still fast while the macro starts
        else {
            // Start-up is over: become a cheap safety net and a good citizen.
            var t = _macroThread;
            if (t != null && t.Priority != System.Threading.ThreadPriority.BelowNormal) {
                try { t.Priority = System.Threading.ThreadPriority.BelowNormal; } catch { }
            }
            System.Threading.Thread.Sleep(100);
        }
    }
}

public static bool StartMacroGuard(string titlePart, int pid) {
    StopMacroGuard();
    if (titlePart == null || titlePart.Length == 0) { return false; }
    _macroMatch = titlePart;
    _macroPid = pid;
    uint filter = (pid > 0) ? (uint)pid : 0u;
    _macroProc = new MacroWinEventHook(OnMacroWindowEvent);
    _macroHook = SetWinEventHook(0x8000, 0x8003, IntPtr.Zero, _macroProc, filter, 0, 0);
    // the caption can be assigned right after the dialog appears
    _macroHookName = SetWinEventHook(0x800C, 0x800C, IntPtr.Zero, _macroProc, filter, 0, 0);
    _macroRun = true;
    _macroThread = new System.Threading.Thread(MacroWatchLoop);
    _macroThread.IsBackground = true;
    // Start-up is the only moment where the helper window can slip through: a
    // slightly raised priority keeps the sweep from being starved while Tera
    // Term and the macro host are starting.  MacroWatchLoop lowers it again
    // once that phase is over.
    try { _macroThread.Priority = System.Threading.ThreadPriority.AboveNormal; } catch { }
    _macroThread.Start();
    return (_macroHook != IntPtr.Zero) || (_macroHookName != IntPtr.Zero) || _macroThread.IsAlive;
}

public static void StopMacroGuard() {
    _macroRun = false;
    _macroThread = null;
    if (_macroHook != IntPtr.Zero) { try { UnhookWinEvent(_macroHook); } catch { } _macroHook = IntPtr.Zero; }
    if (_macroHookName != IntPtr.Zero) { try { UnhookWinEvent(_macroHookName); } catch { } _macroHookName = IntPtr.Zero; }
    _macroProc = null;
    _macroMatch = null;
    _macroPid = 0;
}

private static void OnMacroWindowEvent(IntPtr hHook, uint ev, IntPtr hwnd, int idObject, int idChild, uint thread, uint time) {
    if (hwnd == IntPtr.Zero || idObject != 0) { return; }
    try { HideIfMacro(hwnd); } catch { }
}
'@

# One window per desktop session: a second double-click just raises the first.
$script:AppMutex = $null
if (-not $UiPreview) {
    $acquired = $true
    try {
        $script:AppMutex = New-Object System.Threading.Mutex($false, $MutexName)
        try { $acquired = $script:AppMutex.WaitOne(0, $false) }
        catch [System.Threading.AbandonedMutexException] { $acquired = $true }
    } catch { $acquired = $true }
    if (-not $acquired) {
        # Another window is already open - raise it instead of opening a second one.
        $h = [IntPtr]::Zero
        try { $h = [Win32Util.Native]::FindWindow($null, $AppTitle) } catch { }
        if ($h -eq [IntPtr]::Zero) {
            try {
                $other = Get-Process -Name 'powershell' -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $PID -and $_.MainWindowHandle -ne [IntPtr]::Zero -and $_.MainWindowTitle -eq $AppTitle } | Select-Object -First 1
                if ($other) { $h = $other.MainWindowHandle }
            } catch { }
        }
        if ($h -ne [IntPtr]::Zero) {
            try {
                [void][Win32Util.Native]::ShowWindow($h, 9)
                [void][Win32Util.Native]::SetForegroundWindow($h)
            } catch { }
        }
        exit 0
    }
}


try { [void][Win32Util.Native]::SetProcessDPIAware() } catch { }
[System.Windows.Forms.Application]::EnableVisualStyles()

# A stray WinForms exception must never pop the raw .NET error dialog again:
# it is logged, reported as a toast and the window keeps working.
[System.Windows.Forms.Application]::SetUnhandledExceptionMode([System.Windows.Forms.UnhandledExceptionMode]::CatchException)
[System.Windows.Forms.Application]::add_ThreadException({
    param($sender, $e)
    $detail = ''
    try { $detail = $e.Exception.Message } catch { }
    try {
        # PowerShell knows the exact script line - far more useful than the .NET stack
        $where = ''
        try {
            $ex = $e.Exception
            $inv = $null
            if ($ex.InvocationInfo) { $inv = $ex.InvocationInfo }
            elseif ($ex.ErrorRecord) { $inv = $ex.ErrorRecord.InvocationInfo }
            if ($inv) {
                $where = ' | script line ' + $inv.ScriptLineNumber + ': ' + ([string]$inv.Line).Trim()
                if ($inv.PositionMessage) { $where = $where + ' | ' + (([string]$inv.PositionMessage) -split [char]10)[0] }
            } else { $where = ' @ ' + $ex.StackTrace }
        } catch { }
        Add-Content -LiteralPath (Join-Path $PSScriptRoot '.build\ui_error.log') -Value ('[' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + '] UI EXCEPTION: ' + $detail + $where)
    } catch { }
    try {
        if ($script:pnlNotice) { Show-Notice -Text ('Internal UI error - the window keeps running: ' + $detail) -Kind 'err' }
    } catch { }
})

$script:DpiScale = 1.0
try {
    $probeBmp = New-Object System.Drawing.Bitmap 1, 1
    $probeGfx = [System.Drawing.Graphics]::FromImage($probeBmp)
    $script:DpiScale = [double]$probeGfx.DpiX / 96.0
    $probeGfx.Dispose()
    $probeBmp.Dispose()
} catch { }
if ($script:DpiScale -lt 1.0) { $script:DpiScale = 1.0 }
if ($script:DpiScale -gt 3.0) { $script:DpiScale = 3.0 }

# Scale a 96-dpi design pixel value.  Fonts are point based and must NOT be
# scaled - GDI+ already renders them larger on high-dpi displays.
function Px { param([double]$Value) return [int][Math]::Round($Value * $script:DpiScale) }
function Pt { param([double]$X, [double]$Y) return (New-Object System.Drawing.Point((Px $X), (Px $Y))) }
function Sz { param([double]$W, [double]$H) return (New-Object System.Drawing.Size((Px $W), (Px $H))) }

function C { param([int]$R, [int]$G, [int]$B) return [System.Drawing.Color]::FromArgb($R, $G, $B) }
function Hx {
    param([string]$Value)
    $h = $Value.TrimStart('#')
    return [System.Drawing.Color]::FromArgb(
        [Convert]::ToInt32($h.Substring(0, 2), 16),
        [Convert]::ToInt32($h.Substring(2, 2), 16),
        [Convert]::ToInt32($h.Substring(4, 2), 16))
}

# ------------------------------------------------------------------- theme
$script:Theme = @{
    Bg          = (Hx '#EDF1F7')
    Surface     = (Hx '#FFFFFF')
    Surface2    = (Hx '#F8FAFC')
    Border      = (Hx '#DCE3EC')
    Text        = (Hx '#0F172A')
    TextMuted   = (Hx '#64748B')
    TextFaint   = (Hx '#6B7280')
    Accent      = (Hx '#2563EB')
    AccentSoft  = (Hx '#DBEAFE')
    Header1     = (Hx '#0B1220')
    Header2     = (Hx '#1E3A8A')
    Ok          = (Hx '#15803D')
    OkSoft      = (Hx '#DCFCE7')
    Warn        = (Hx '#B45309')
    WarnSoft    = (Hx '#FEF3C7')
    Err         = (Hx '#B91C1C')
    ErrSoft     = (Hx '#FEE2E2')
    Info        = (Hx '#1D4ED8')
    InfoSoft    = (Hx '#DBEAFE')
    Neutral     = (Hx '#475569')
    NeutralSoft = (Hx '#E2E8F0')
    DisabledBg  = (Hx '#E9EEF5')
    DisabledFg  = (Hx '#A3B0BF')
    GhostBorder = (Hx '#CBD5E1')
    GridLine    = (Hx '#F1F5F9')
}

# Action palette: every entry has the same weight (600 fill / 500 hover, white
# text) so the buttons read as one family instead of a rainbow.
$script:Pal = @{
    Primary = @{ Back = (Hx '#2563EB'); Hover = (Hx '#3B82F6'); Fore = (Hx '#FFFFFF') }
    Danger  = @{ Back = (Hx '#DC2626'); Hover = (Hx '#EF4444'); Fore = (Hx '#FFFFFF') }
    Warn    = @{ Back = (Hx '#D97706'); Hover = (Hx '#F59E0B'); Fore = (Hx '#FFFFFF') }
    Success = @{ Back = (Hx '#059669'); Hover = (Hx '#10B981'); Fore = (Hx '#FFFFFF') }
    Teal    = @{ Back = (Hx '#0D9488'); Hover = (Hx '#14B8A6'); Fore = (Hx '#FFFFFF') }
    Slate   = @{ Back = (Hx '#475569'); Hover = (Hx '#64748B'); Fore = (Hx '#FFFFFF') }
    Violet  = @{ Back = (Hx '#6D28D9'); Hover = (Hx '#8B5CF6'); Fore = (Hx '#FFFFFF') }
    Ghost   = @{ Back = (Hx '#FFFFFF'); Hover = (Hx '#EEF2F7'); Fore = (Hx '#334155') }
}

$script:F = @{
    Title  = (New-Object System.Drawing.Font('Segoe UI', 15, [System.Drawing.FontStyle]::Bold))
    Sub    = (New-Object System.Drawing.Font('Segoe UI', 9))
    Card   = (New-Object System.Drawing.Font('Segoe UI', 10, [System.Drawing.FontStyle]::Bold))
    Body   = (New-Object System.Drawing.Font('Segoe UI', 9))
    BodyB  = (New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold))
    Small  = (New-Object System.Drawing.Font('Segoe UI', 8.75))
    Btn    = (New-Object System.Drawing.Font('Segoe UI', 10.5, [System.Drawing.FontStyle]::Bold))
    BtnSm  = (New-Object System.Drawing.Font('Segoe UI', 8.5, [System.Drawing.FontStyle]::Bold))
    State  = (New-Object System.Drawing.Font('Segoe UI', 11, [System.Drawing.FontStyle]::Bold))
    Pill   = (New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold))
    Log    = (New-Object System.Drawing.Font('Consolas', 8.5))
}

# --------------------------------------------------------------- metrics
$script:M = @{
    HeaderH   = (Px 70)
    AccentH   = (Px 3)
    ConnH     = (Px 112)
    TtH       = (Px 200)
    BtnH      = (Px 46)
    BtnSmH    = (Px 26)
    StatusH   = (Px 70)
    EventsMin = (Px 96)
    NoticeH   = (Px 42)
    ActionH   = (Px 158)
    Pad       = (Px 14)
    Gap       = (Px 14)
    RowGap    = (Px 12)
    Radius    = (Px 8)
    RadiusSm  = (Px 6)
}

# ------------------------------------------------------------ drawing utils
function New-RoundPath {
    param([int]$X, [int]$Y, [int]$W, [int]$H, [int]$R)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    if ($W -le 0 -or $H -le 0) { return $path }
    if ($R -lt 1 -or $W -lt ($R * 2 + 2) -or $H -lt ($R * 2 + 2)) {
        $path.AddRectangle((New-Object System.Drawing.Rectangle($X, $Y, $W, $H)))
        return $path
    }
    $d = $R * 2
    $path.AddArc($X, $Y, $d, $d, 180, 90)
    $path.AddArc(($X + $W - $d - 1), $Y, $d, $d, 270, 90)
    $path.AddArc(($X + $W - $d - 1), ($Y + $H - $d - 1), $d, $d, 0, 90)
    $path.AddArc($X, ($Y + $H - $d - 1), $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

function Set-RoundRegion {
    param($Control, [int]$R)
    if (-not $Control) { return }
    try {
        $w = $Control.Width; $h = $Control.Height
        if ($w -le 0 -or $h -le 0) { return }
        $path = New-RoundPath 0 0 $w $h $R
        $old = $Control.Region
        $Control.Region = New-Object System.Drawing.Region($path)
        $path.Dispose()
        if ($old) { $old.Dispose() }
    } catch { }
}

# ------------------------------------------------------------ control factory
$script:BtnPalette = @{}
$script:FieldBoxes = New-Object System.Collections.ArrayList

function New-Label {
    param([string]$Text, $Font, $Color, [bool]$AutoSize = $true)
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $Text
    $l.Font = $Font
    $l.ForeColor = $Color
    $l.BackColor = [System.Drawing.Color]::Transparent
    $l.AutoSize = $AutoSize
    if (-not $AutoSize) { $l.TextAlign = 'MiddleLeft' }
    return $l
}

function New-Card {
    param([string]$Title, [int]$W, [int]$H)
    $card = New-Object System.Windows.Forms.Panel
    $card.Size = New-Object System.Drawing.Size($W, $H)
    $card.BackColor = $script:Theme.Surface
    $card.Tag = $Title
    $card.Add_Paint({
        param($s, $e)
        $g = $e.Graphics
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $w = $s.Width; $h = $s.Height
        $path = New-RoundPath 0 0 ($w - 1) ($h - 1) (Px 8)
        $fill = New-Object System.Drawing.SolidBrush $script:Theme.Surface
        $pen = New-Object System.Drawing.Pen $script:Theme.Border, 1
        $g.FillPath($fill, $path)
        $g.DrawPath($pen, $path)
        $title = [string]$s.Tag
        if ($title) {
            $bar = New-Object System.Drawing.SolidBrush $script:Theme.Accent
            $g.FillRectangle($bar, (Px 15), (Px 17), (Px 3), (Px 13))
            $bar.Dispose()
            $tb = New-Object System.Drawing.SolidBrush $script:Theme.Text
            $g.DrawString($title, $script:F.Card, $tb, (Px 24), (Px 13))
            $tb.Dispose()
        }
        $fill.Dispose(); $pen.Dispose(); $path.Dispose()
    })
    $card.Add_Resize({
        param($s, $e)
        Set-RoundRegion $s (Px 8)
        $s.Invalidate()
    })
    Set-RoundRegion $card (Px 8)
    return $card
}

function New-FieldBox {
    param([int]$PadLeft = 9, [int]$PadRight = 9)
    $box = New-Object System.Windows.Forms.Panel
    $box.BackColor = $script:Theme.Surface
    $box.Tag = @{ Focus = $false; Invalid = $false; PadLeft = $PadLeft; PadRight = $PadRight }
    [void]$script:FieldBoxes.Add($box)
    $box.Add_Paint({
        param($s, $e)
        $g = $e.Graphics
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $st = $s.Tag
        $bc = $script:Theme.Border
        if ($st.Invalid) { $bc = $script:Theme.Err }
        elseif ($st.Focus) { $bc = $script:Theme.Accent }
        $path = New-RoundPath 0 0 ($s.Width - 1) ($s.Height - 1) (Px 6)
        $fill = New-Object System.Drawing.SolidBrush $s.BackColor
        $pen = New-Object System.Drawing.Pen($bc, 1)
        $g.FillPath($fill, $path)
        $g.DrawPath($pen, $path)
        $fill.Dispose(); $pen.Dispose(); $path.Dispose()
    })
    $box.Add_Resize({
        param($s, $e)
        if ($s.Controls.Count -gt 0) {
            $c = $s.Controls[0]
            $st = $s.Tag
            $c.Location = New-Object System.Drawing.Point((Px $st.PadLeft), [int](($s.Height - $c.Height) / 2))
            $w = $s.Width - (Px $st.PadLeft) - (Px $st.PadRight)
            if ($w -gt (Px 10)) { $c.Width = $w }
        }
        Set-RoundRegion $s (Px 6)
        $s.Invalidate()
    })
    Set-RoundRegion $box (Px 6)
    return $box
}

function New-TextField {
    param([bool]$ReadOnly = $false)
    $box = New-FieldBox 9 9
    $txt = New-Object System.Windows.Forms.TextBox
    $txt.BorderStyle = 'None'
    $txt.Font = $script:F.Body
    $txt.BackColor = $script:Theme.Surface
    $txt.ForeColor = $script:Theme.Text
    $txt.ReadOnly = $ReadOnly
    $box.Controls.Add($txt)
    $txt.Add_GotFocus({ param($s, $e) if ($s.Parent -and $s.Parent.Tag) { $s.Parent.Tag.Focus = $true; $s.Parent.Invalidate() } })
    $txt.Add_LostFocus({ param($s, $e) if ($s.Parent -and $s.Parent.Tag) { $s.Parent.Tag.Focus = $false; $s.Parent.Invalidate() } })
    return @{ Box = $box; Txt = $txt }
}

function New-ComboField {
    param([bool]$Editable = $false, [int]$DropWidth = 0)
    # A flat combo is inset by one pixel inside the rounded wrapper, so the
    # wrapper border stays visible and the arrow keeps its room.
    $box = New-FieldBox 5 7
    $cbo = New-Object System.Windows.Forms.ComboBox
    $cbo.FlatStyle = 'Flat'
    $cbo.Font = $script:F.Body
    $cbo.BackColor = $script:Theme.Surface
    $cbo.ForeColor = $script:Theme.Text
    $cbo.IntegralHeight = $false
    if ($Editable) { $cbo.DropDownStyle = 'DropDown' } else { $cbo.DropDownStyle = 'DropDownList' }
    if ($DropWidth -gt 0) { $cbo.DropDownWidth = (Px $DropWidth) }
    $box.Controls.Add($cbo)
    $cbo.Add_GotFocus({ param($s, $e) if ($s.Parent -and $s.Parent.Tag) { $s.Parent.Tag.Focus = $true; $s.Parent.Invalidate() } })
    $cbo.Add_LostFocus({ param($s, $e) if ($s.Parent -and $s.Parent.Tag) { $s.Parent.Tag.Focus = $false; $s.Parent.Invalidate() } })
    return @{ Box = $box; Cbo = $cbo }
}

function New-FlatButton {
    param([string]$Text, [hashtable]$Palette, [bool]$Ghost = $false)
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $Text
    $b.FlatStyle = 'Flat'
    $b.BackColor = $Palette.Back
    $b.ForeColor = $Palette.Fore
    $b.Font = $script:F.Btn
    $b.UseVisualStyleBackColor = $false
    $b.Cursor = [System.Windows.Forms.Cursors]::Hand
    $b.FlatAppearance.BorderSize = 0
    $b.FlatAppearance.MouseOverBackColor = $Palette.Hover
    $b.FlatAppearance.MouseDownBackColor = $Palette.Hover
    if ($Ghost) {
        $b.FlatAppearance.BorderSize = 1
        $b.FlatAppearance.BorderColor = $script:Theme.GhostBorder
        $b.Font = $script:F.BtnSm
    }
    $script:BtnPalette[$b] = $Palette
    return $b
}

function New-FlatCheck {
    param([string]$Text)
    $c = New-Object System.Windows.Forms.CheckBox
    $c.Text = $Text
    $c.AutoSize = $true
    $c.FlatStyle = 'Flat'
    $c.Font = $script:F.Body
    $c.ForeColor = $script:Theme.Text
    $c.BackColor = [System.Drawing.Color]::Transparent
    $c.Cursor = [System.Windows.Forms.Cursors]::Hand
    return $c
}

function Set-ButtonPalette {
    # Idempotent: re-applying the same palette object is a no-op, so the 500 ms
    # refresh never repaints buttons that did not actually change.
    param($Button, [hashtable]$Palette)
    if (-not $Button) { return }
    if ($script:BtnPalette[$Button] -eq $Palette) { return }
    $script:BtnPalette[$Button] = $Palette
    $Button.ForeColor = $Palette.Fore
    if ($Button.Enabled) {
        $Button.BackColor = $Palette.Back
        $Button.FlatAppearance.MouseOverBackColor = $Palette.Hover
        $Button.FlatAppearance.MouseDownBackColor = $Palette.Hover
    }
}

function Set-ButtonState {
    param($Button, [bool]$On)
    if (-not $Button) { return }
    if ($Button.Enabled -eq $On) { return }
    $Button.Enabled = $On
    $pal = $null
    if ($script:BtnPalette.ContainsKey($Button)) { $pal = $script:BtnPalette[$Button] }
    if ($null -eq $pal) { return }
    if ($On) {
        $Button.BackColor = $pal.Back
        $Button.ForeColor = $pal.Fore
        $Button.FlatAppearance.MouseOverBackColor = $pal.Hover
        $Button.FlatAppearance.MouseDownBackColor = $pal.Hover
    } else {
        $Button.BackColor = $script:Theme.DisabledBg
        $Button.ForeColor = $script:Theme.DisabledFg
        $Button.FlatAppearance.MouseOverBackColor = $script:Theme.DisabledBg
        $Button.FlatAppearance.MouseDownBackColor = $script:Theme.DisabledBg
    }
}

function Set-FieldInvalid {
    param($Field, [bool]$Invalid)
    if (-not $Field) { return }
    $Field.Tag.Invalid = $Invalid
    $Field.Invalidate()
}

function Clear-FieldInvalid {
    foreach ($b in $script:FieldBoxes) {
        if ($b.Tag -and $b.Tag.Invalid) { $b.Tag.Invalid = $false; $b.Invalidate() }
    }
}

# --------------------------------------------------------------- file helpers
function Write-UiLog {
    param([string]$Message)
    try {
        Add-Content -LiteralPath $script:ErrorLogFile -Value ('[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message)
    } catch { }
}

function Show-Alert {
    # Modal - only for cases that need a decision or an unmissable warning.
    param([string]$Message, [string]$Kind = 'Warning')
    [void][System.Windows.Forms.MessageBox]::Show($Message, $AppTitle, 'OK', $Kind)
}

function Write-SharedText {
    param([string]$Path, [string]$Text)
    $enc = New-Object System.Text.ASCIIEncoding
    $bytes = $enc.GetBytes($Text)
    $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
    try { $fs.Write($bytes, 0, $bytes.Length); $fs.Flush() } finally { $fs.Close() }
}

function Write-AsciiText {
    param([string]$Path, [string]$Text)
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.ASCIIEncoding))
}

function Write-IniText {
    param([string]$Path, [string]$Text)
    if ($Text -match '[^\x00-\x7F]') {
        [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
    } else {
        [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.ASCIIEncoding))
    }
}

function Read-FirstLineShared {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $len = [int][Math]::Min([long]$fs.Length, 4096)
            if ($len -le 0) { return '' }
            $buf = [byte[]]::new($len)
            [void]$fs.Read($buf, 0, $len)
            $txt = [System.Text.Encoding]::ASCII.GetString($buf, 0, $len)
            $nl = $txt.IndexOf([char]10)
            if ($nl -ge 0) { $txt = $txt.Substring(0, $nl) }
            return $txt.TrimEnd([char]13)
        } finally { $fs.Close() }
    } catch { return $null }
}

function Test-AsciiPath {
    param([string]$Path)
    return (-not ($Path -match '[^\x00-\x7F]'))
}

function Wait-UiPump {
    # Wait in tiny slices while keeping the message pump alive.  The WinEvent
    # callback and the timers that hide the ttpmacro helper window only run
    # while messages are pumped, so a plain Start-Sleep would let that window
    # stay visible for the whole sleep.
    param([int]$Ms = 10)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $Ms) {
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 1
    }
}

function New-SettingRow {
    # one column rhythm for every settings dialog: label left, value at x=176,
    # muted hint directly under the value.
    param($Card, [string]$Label, [int]$LabelY, [string]$Hint = '')
    $lb = New-Label $Label $script:F.Small $script:Theme.TextMuted
    $lb.Location = (New-Object System.Drawing.Point((Px 20), (Px $LabelY)))
    $Card.Controls.Add($lb)
    if ($Hint -ne '') {
        $hn = New-Label $Hint $script:F.Small $script:Theme.TextFaint
        $hn.Location = (New-Object System.Drawing.Point((Px 176), (Px ($LabelY + 32))))
        $Card.Controls.Add($hn)
    }
}

function Get-ResetHoldMs {
    # How long the RESET line is held low for a short tap (0.5 s default).
    $v = 500
    try { if ($script:Settings.ContainsKey('ResetHoldMs')) { $v = [int]$script:Settings.ResetHoldMs } } catch { }
    if ($v -lt 50) { $v = 50 }
    if ($v -gt 5000) { $v = 5000 }
    return $v
}

function Get-AckTimeoutMs {
    $v = 4000
    try { if ($script:Settings.ContainsKey('AckTimeoutMs')) { $v = [int]$script:Settings.AckTimeoutMs } } catch { }
    if ($v -lt 500) { $v = 500 }
    if ($v -gt 60000) { $v = 60000 }
    return $v
}

function Set-IniValue {
    # Replace "Key=..." in place, or insert it right after an anchor key so the
    # new entry stays inside the same INI [section].  Line based on purpose:
    # regex "$" handling with CRLF text is easy to get wrong.
    param([string]$Text, [string]$Key, [string]$Value, [string]$AfterKey = '')
    $lines = @($Text -split '
|
')
    $keyRe = '^[ \t]*' + [regex]::Escape($Key) + '='
    $anchorRe = '^[ \t]*' + [regex]::Escape($AfterKey) + '='
    $hit = -1
    $anchor = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($hit -lt 0 -and [regex]::IsMatch($lines[$i], $keyRe)) { $hit = $i }
        if ($AfterKey -ne '' -and $anchor -lt 0 -and [regex]::IsMatch($lines[$i], $anchorRe)) { $anchor = $i }
    }
    $newLine = $Key + '=' + $Value
    if ($hit -ge 0) {
        $lines[$hit] = $newLine
    } else {
        if ($anchor -lt 0) {
            for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i].Trim() -eq '[Tera Term]') { $anchor = $i; break } }
        }
        $out = New-Object System.Collections.ArrayList
        for ($i = 0; $i -lt $lines.Count; $i++) {
            [void]$out.Add($lines[$i])
            if ($i -eq $anchor) { [void]$out.Add($newLine) }
        }
        if ($anchor -lt 0) { [void]$out.Add($newLine) }
        $lines = $out.ToArray()
    }
    return ($lines -join $CRLF)
}

function Format-Kb {
    param([double]$Bytes)
    if ($Bytes -lt 1024) { return ('{0:N0} B' -f $Bytes) }
    if ($Bytes -lt 1048576) { return ('{0:N1} KB' -f ($Bytes / 1KB)) }
    return ('{0:N1} MB' -f ($Bytes / 1MB))
}

# --------------------------------------------------------------------- paths
$ScriptRoot         = $PSScriptRoot
$BuildDir           = Join-Path $ScriptRoot '.build'
$SettingsFile       = Join-Path $BuildDir 'console_ui.json'
$script:ErrorLogFile = Join-Path $BuildDir 'ui_error.log'
$WatcherTemplate    = Join-Path $ScriptRoot 'tt_console_watcher.ttl'
$IcoPath            = Join-Path $ScriptRoot 'TeraTermConsole.ico'
$DefaultTeraTermExe = 'C:\Program Files (x86)\teraterm\ttermpro.exe'
$DefaultIniTemplate = Join-Path $ScriptRoot 'geen.INI'
$DefaultLogDir      = Join-Path $BuildDir 'logs'

if (-not (Test-Path -LiteralPath $BuildDir))      { New-Item -ItemType Directory -Path $BuildDir -Force      | Out-Null }
if (-not (Test-Path -LiteralPath $DefaultLogDir)) { New-Item -ItemType Directory -Path $DefaultLogDir -Force | Out-Null }

# ------------------------------------------------------------------ settings
$script:Settings = @{
    ComPort        = 'COM10'
    BaudRate       = '115200'
    TeraTermExe    = $DefaultTeraTermExe
    IniTemplate    = $DefaultIniTemplate
    LogDir         = $DefaultLogDir
    LogNamePattern = 'esp_wifi_%Y%m%d_%H%M%S.log'
    LogTimestamp   = $false
    AutoLogOnOpen  = $false
    ResetHold      = $false
    ResetHoldMs    = 500
    AckTimeoutMs   = 4000
    LogTsFormat    = '%Y-%m-%d %H:%M:%S.%N'
    LogTsType      = 'Local'
    ExtraTtArgs    = ''
    OnCloseAction  = 'ask'
    SkipTtWarning  = $false
    ShowMacroWindow = $false
    WinW           = 0
    WinH           = 0
    WinX           = -1
    WinY           = -1
}

function Load-Settings {
    if (-not (Test-Path -LiteralPath $SettingsFile)) { return }
    try {
        $j = Get-Content -LiteralPath $SettingsFile -Raw | ConvertFrom-Json
        foreach ($k in @($script:Settings.Keys)) {
            $v = $j.$k
            # $null = key absent in the file.  An empty string is a real value
            # (e.g. "Extra TT args" cleared by the user) and must be kept,
            # otherwise such a setting can never be reset to empty.
            if ($null -ne $v) { $script:Settings[$k] = $v }
        }
        # Self-heal a stale INI path (project moved / another machine): the
        # template ships next to this script, so fall back to it when the
        # stored path no longer exists.
        $ini = [string]$script:Settings.IniTemplate
        if ($ini -eq '' -or -not (Test-Path -LiteralPath $ini)) {
            if (Test-Path -LiteralPath $DefaultIniTemplate) { $script:Settings.IniTemplate = $DefaultIniTemplate }
        }
    } catch { Write-UiLog ('Load-Settings failed: ' + $_.Exception.Message) }
}

function Save-Settings {
    try {
        $script:Settings | ConvertTo-Json | Set-Content -LiteralPath $SettingsFile -Encoding ASCII
    } catch { Write-UiLog ('Save-Settings failed: ' + $_.Exception.Message) }
}

# ------------------------------------------------------------ COM / log names
function Get-ComPortList {
    $ports = @()
    try { $ports = @([System.IO.Ports.SerialPort]::GetPortNames()) } catch { }
    $names = @{}
    try {
        $pnp = Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop | Where-Object { $_.Name -match '\(COM\d+\)' }
        foreach ($d in $pnp) {
            if ($d.Name -match '\((COM\d+)\)') { $names[$Matches[1]] = ($d.Name -replace '\s*\(COM\d+\)\s*$', '').Trim() }
        }
    } catch { }
    $list = New-Object System.Collections.ArrayList
    foreach ($p in $ports) {
        $disp = $p
        if ($names.ContainsKey($p) -and $names[$p].Length -gt 0) { $disp = $p + '  -  ' + $names[$p] }
        [void]$list.Add([pscustomobject]@{ Port = $p; Display = $disp; Number = [int]($p -replace '\D', '') })
    }
    return @($list | Sort-Object Number)
}

function Resolve-LogFileName {
    param([string]$Pattern)
    $now = Get-Date
    $s = '' + $Pattern
    $s = $s.Replace('%Y', $now.ToString('yyyy')).Replace('%y', $now.ToString('yy'))
    $s = $s.Replace('%m', $now.ToString('MM')).Replace('%d', $now.ToString('dd'))
    $s = $s.Replace('%H', $now.ToString('HH')).Replace('%M', $now.ToString('mm')).Replace('%S', $now.ToString('ss'))
    $s = $s -replace '[\\/:*?"<>|]', '_'
    if (-not $s.ToLowerInvariant().EndsWith('.log')) { $s = $s + '.log' }
    return $s
}

# ------------------------------------------------------------------- session
$script:Session = @{
    Id      = $null
    Proc    = $null
    Ctrl    = $null
    Status  = $null
    Watcher = $null
    Seq     = 0
    State   = 'Stopped'
    Com     = ''
    Baud    = ''
    LogPath = $null
    Logging = $false
    Link    = -1
    HideTries = 0
    LinkStale = 0
    StartedAt = $null
    ResetMode = $null
    ResetPressing = $false
    ResetPendingRelease = $false
    ResetPort = $null
    ResetTime = $null
    ResetLatched = $false
}

function Send-TtlCommand {
    param([string]$Cmd, [string]$Arg1 = '', [string]$Arg2 = '')
    $s = $script:Session
    $s.Seq = [int]$s.Seq + 1
    $line = ('{0}|{1}|{2}|{3}|END' -f $s.Seq, $Cmd, $Arg1, $Arg2) + $CRLF
    $lastErr = $null
    for ($i = 0; $i -lt 12; $i++) {
        try { Write-SharedText -Path $s.Ctrl -Text $line; return $s.Seq }
        catch { $lastErr = $_; Start-Sleep -Milliseconds 50 }
    }
    throw ('Cannot write control file: ' + $lastErr)
}

function Wait-TtlAck {
    param([int]$Seq, [int]$TimeoutMs = 0)
    if ($TimeoutMs -le 0) { $TimeoutMs = (Get-AckTimeoutMs) }
    $s = $script:Session
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        [System.Windows.Forms.Application]::DoEvents()
        if ((-not $s.Proc) -or $s.Proc.HasExited) { return $null }
        $parts = Read-SessionStatus
        if ($parts) {
            if ($parts.Seq -ge $Seq) { return $parts.Raw }
        }
        Wait-UiPump 20
    }
    return $null
}

function Read-SessionStatus {
    # -> @{ Raw = <string[]>; Seq = <int>; State = <string>; Log = <string>; Link = <int> } or $null
    $s = $script:Session
    if (-not $s.Status) { return $null }
    $line = Read-FirstLineShared -Path $s.Status
    if (-not $line) { return $null }
    $parts = $line.Split('|')
    $seq = -1
    if ($parts.Length -ge 1) {
        $n = 0
        if ([int]::TryParse($parts[0], [ref]$n)) { $seq = $n }
    }
    $state = ''
    if ($parts.Length -ge 2) { $state = $parts[1] }
    $log = ''
    if ($parts.Length -ge 3) { $log = $parts[2] }
    $link = -1
    if ($parts.Length -ge 4) {
        $n = 0
        if ([int]::TryParse($parts[3], [ref]$n)) { $link = $n }
    }
    return @{ Raw = $parts; Seq = $seq; State = $state; Log = $log; Link = $link }
}

# ======================================================================== UI
$script:form = New-Object System.Windows.Forms.Form
$script:form.Text = $AppTitle
$script:form.Font = $script:F.Body
$script:form.BackColor = $script:Theme.Bg
$script:form.ForeColor = $script:Theme.Text
$script:form.AutoScaleMode = 'None'
$script:form.FormBorderStyle = 'Sizable'
$script:form.StartPosition = 'CenterScreen'
$script:form.KeyPreview = $true
$script:form.ClientSize = (Sz 800 872)
try { $script:form.DoubleBuffered = $true } catch { }
try { [void][Win32Util.Native]::SetCurrentProcessExplicitAppUserModelID($AppId) } catch { }
$script:FormReady = $false

# ------------------------------------------------------------- header banner
# The banner (gradient, title and the live state pill) is rendered into a
# cached bitmap: it is redrawn only when its size or the pill state changes,
# so the status indicator can never flicker while the UI refreshes.
$script:HeaderArt = $null
$script:pnlHeader = New-Object System.Windows.Forms.Panel
$script:pnlHeader.BackColor = $script:Theme.Header1
$script:pnlHeader.BackgroundImageLayout = 'None'
$script:form.Controls.Add($script:pnlHeader)

$script:Pill = @{ Text = 'Stopped'; Bg = $script:Theme.NeutralSoft; Fg = $script:Theme.Neutral }

function Update-HeaderArt {
    if (-not $script:pnlHeader) { return }
    $w = $script:pnlHeader.Width
    $h = $script:pnlHeader.Height
    if ($w -lt 8 -or $h -lt 8) { return }
    if ($null -eq $script:HeaderArt -or $script:HeaderArt.Width -ne $w -or $script:HeaderArt.Height -ne $h) {
        $old = $script:HeaderArt
        $script:HeaderArt = New-Object System.Drawing.Bitmap($w, $h)
        $script:pnlHeader.BackgroundImage = $script:HeaderArt
        if ($old) { $old.Dispose() }
    }
    $g = [System.Drawing.Graphics]::FromImage($script:HeaderArt)
    try {
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $r = New-Object System.Drawing.Rectangle 0, 0, $w, $h
        $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($r, $script:Theme.Header1, $script:Theme.Header2, 16.0)
        $g.FillRectangle($grad, $r)
        $grad.Dispose()
        $pen = New-Object System.Drawing.Pen($script:Theme.Accent, 1)
        $g.DrawLine($pen, 0, ($h - 1), $w, ($h - 1))
        $pen.Dispose()
        $tb = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
        $g.DrawString('TeraTerm Console', $script:F.Title, $tb, (Px 18), (Px 9))
        $tb.Dispose()
        $sb = New-Object System.Drawing.SolidBrush (Hx '#9EC1E8')
        $g.DrawString('esp_wifi_template   |   serial console manager', $script:F.Sub, $sb, (Px 20), (Px 41))
        $sb.Dispose()
        $st = $script:Pill
        $pw = Px 176
        $ph = Px 32
        $px = $w - (Px 14) - $pw
        $py = [int](($h - $ph) / 2)
        if ($px -gt (Px 260)) {
            $path = New-RoundPath $px $py $pw $ph ([int]($ph / 2))
            $fb = New-Object System.Drawing.SolidBrush $st.Bg
            $g.FillPath($fb, $path)
            $fb.Dispose()
            $d = Px 9
            $db = New-Object System.Drawing.SolidBrush $st.Fg
            $g.FillEllipse($db, ($px + (Px 13)), ($py + [int](($ph - $d) / 2)), $d, $d)
            $db.Dispose()
            $tt = New-Object System.Drawing.SolidBrush $st.Fg
            $g.DrawString($st.Text, $script:F.Pill, $tt, ($px + (Px 29)), ($py + [int](($ph - $script:F.Pill.Height) / 2)))
            $tt.Dispose()
            $path.Dispose()
        }
    } finally { $g.Dispose() }
    $script:pnlHeader.Invalidate()
}

$script:pnlAccent = New-Object System.Windows.Forms.Panel
$script:pnlAccent.BackColor = $script:Theme.Accent
$script:form.Controls.Add($script:pnlAccent)

# ------------------------------------------------------------ connection card
$script:cardConn = New-Card 'Connection' (Px 760) ($script:M.ConnH)
$script:form.Controls.Add($script:cardConn)

$script:lblComCap = New-Label 'COM port' $script:F.Small $script:Theme.TextMuted
$script:cardConn.Controls.Add($script:lblComCap)

$script:ComField = New-ComboField $false 460
$script:boxCom = $script:ComField.Box
$script:cboCom = $script:ComField.Cbo
$script:cboCom.TabIndex = 0
$script:cardConn.Controls.Add($script:boxCom)

$script:btnRefresh = New-FlatButton 'Refresh' $script:Pal.Ghost $true
$script:btnRefresh.TabIndex = 1
$script:cardConn.Controls.Add($script:btnRefresh)

$script:lblBaudCap = New-Label 'Baud rate' $script:F.Small $script:Theme.TextMuted
$script:cardConn.Controls.Add($script:lblBaudCap)

$script:BaudField = New-ComboField $true 160
$script:boxBaud = $script:BaudField.Box
$script:cboBaud = $script:BaudField.Cbo
$script:cboBaud.TabIndex = 2
$script:cardConn.Controls.Add($script:boxBaud)

$script:chkAutoLog = New-FlatCheck 'Auto log on Open'
$script:chkAutoLog.TabIndex = 3
$script:cardConn.Controls.Add($script:chkAutoLog)

# ------------------------------------------------------- Tera Term card
$script:cardTT = New-Card 'Tera Term and logging' (Px 760) ($script:M.TtH)
$script:form.Controls.Add($script:cardTT)

$script:btnAdvanced = New-FlatButton 'Advanced...' $script:Pal.Ghost $true
$script:btnAdvanced.TabIndex = 8
$script:cardTT.Controls.Add($script:btnAdvanced)

$script:lblExeCap = New-Label 'Tera Term exe' $script:F.Small $script:Theme.TextMuted
$script:cardTT.Controls.Add($script:lblExeCap)
$script:ExeField = New-TextField
$script:boxExe = $script:ExeField.Box
$script:txtExe = $script:ExeField.Txt
$script:txtExe.TabIndex = 0
$script:cardTT.Controls.Add($script:boxExe)
$script:btnExe = New-FlatButton 'Browse' $script:Pal.Ghost $true
$script:btnExe.TabIndex = 1
$script:cardTT.Controls.Add($script:btnExe)

$script:lblIniCap = New-Label 'INI template' $script:F.Small $script:Theme.TextMuted
$script:cardTT.Controls.Add($script:lblIniCap)
$script:IniField = New-TextField
$script:boxIni = $script:IniField.Box
$script:txtIni = $script:IniField.Txt
$script:txtIni.TabIndex = 2
$script:cardTT.Controls.Add($script:boxIni)
$script:btnIni = New-FlatButton 'Browse' $script:Pal.Ghost $true
$script:btnIni.TabIndex = 3
$script:cardTT.Controls.Add($script:btnIni)

$script:lblLogDirCap = New-Label 'Log folder' $script:F.Small $script:Theme.TextMuted
$script:cardTT.Controls.Add($script:lblLogDirCap)
$script:LogDirField = New-TextField
$script:boxLogDir = $script:LogDirField.Box
$script:txtLogDir = $script:LogDirField.Txt
$script:txtLogDir.TabIndex = 4
$script:cardTT.Controls.Add($script:boxLogDir)
$script:btnLogDir = New-FlatButton 'Browse' $script:Pal.Ghost $true
$script:btnLogDir.TabIndex = 5
$script:cardTT.Controls.Add($script:btnLogDir)

$script:lblPatternCap = New-Label 'File pattern' $script:F.Small $script:Theme.TextMuted
$script:cardTT.Controls.Add($script:lblPatternCap)
$script:PatternField = New-TextField
$script:boxPattern = $script:PatternField.Box
$script:txtPattern = $script:PatternField.Txt
$script:txtPattern.TabIndex = 6
$script:cardTT.Controls.Add($script:boxPattern)

$script:chkTimestamp = New-FlatCheck 'Timestamp each line'
$script:chkTimestamp.TabIndex = 7
$script:cardTT.Controls.Add($script:chkTimestamp)

$script:lblTokens = New-Label 'Tokens  %Y %m %d %H %M %S      example:  esp_wifi_%Y%m%d_%H%M%S.log' $script:F.Small $script:Theme.TextFaint
$script:cardTT.Controls.Add($script:lblTokens)

# ------------------------------------------------------------ session actions
# Two action cards with two equally wide buttons each, so the columns line up
# with the cards above and the whole window keeps one rhythm.
$script:HoldMode = $false

$script:cardSession = New-Card 'Session' (Px 380) ($script:M.ActionH)
$script:cardSession.TabIndex = 2
$script:form.Controls.Add($script:cardSession)

$script:btnPower = New-FlatButton '&Open Session' $script:Pal.Primary
$script:btnPower.TabIndex = 0
$script:cardSession.Controls.Add($script:btnPower)

$script:btnPauseResume = New-FlatButton '&Pause Port' $script:Pal.Warn
$script:btnPauseResume.TabIndex = 1
$script:cardSession.Controls.Add($script:btnPauseResume)

$script:cardDevice = New-Card 'Logging and device' (Px 380) ($script:M.ActionH)
$script:cardDevice.TabIndex = 3
$script:form.Controls.Add($script:cardDevice)

$script:btnLog = New-FlatButton 'Start &Logging' $script:Pal.Teal
$script:btnLog.TabIndex = 0
$script:cardDevice.Controls.Add($script:btnLog)

$script:btnReset = New-FlatButton '&Restart Device' $script:Pal.Violet
$script:btnReset.TabIndex = 1
$script:cardDevice.Controls.Add($script:btnReset)

# segmented latch toggle, replacing the old floating check box
$script:btnHold = New-FlatButton 'Hold off' $script:Pal.Ghost $true
$script:btnHold.TabIndex = 2
$script:cardDevice.Controls.Add($script:btnHold)

$script:btnMonitor = New-FlatButton 'Monitor' $script:Pal.Ghost $true
$script:btnMonitor.TabIndex = 3
$script:cardDevice.Controls.Add($script:btnMonitor)

# ---------------------------------------------------------------- activity
$script:cardEvents = New-Card 'Activity' (Px 760) (Px 150)
$script:cardEvents.TabIndex = 7
$script:form.Controls.Add($script:cardEvents)

$script:EventKinds = New-Object System.Collections.ArrayList
$script:lstEvents = New-Object System.Windows.Forms.ListBox
$script:lstEvents.BorderStyle = 'None'
$script:lstEvents.DrawMode = 'OwnerDrawFixed'
$script:lstEvents.ItemHeight = (Px 19)
$script:lstEvents.Font = $script:F.Log
$script:lstEvents.BackColor = $script:Theme.Surface
$script:lstEvents.ForeColor = $script:Theme.Text
$script:lstEvents.SelectionMode = 'None'
$script:lstEvents.IntegralHeight = $false
$script:lstEvents.HorizontalScrollbar = $true
$script:lstEvents.TabIndex = 8
$script:lstEvents.Add_DrawItem({
    param($s, $e)
    if ($e.Index -lt 0 -or $e.Index -ge $s.Items.Count) { return }
    $g = $e.Graphics
    $bb = New-Object System.Drawing.SolidBrush $s.BackColor
    $g.FillRectangle($bb, $e.Bounds)
    $bb.Dispose()
    $col = $script:Theme.Text
    if ($e.Index -lt $script:EventKinds.Count) { $col = $script:EventKinds[$e.Index] }
    $fb = New-Object System.Drawing.SolidBrush $col
    $txt = [string]$s.Items[$e.Index]
    $g.DrawString($txt, $s.Font, $fb, ($e.Bounds.X + (Px 4)), ($e.Bounds.Y + [int](($e.Bounds.Height - $s.Font.Height) / 2)))
    $fb.Dispose()
})
$script:cardEvents.Controls.Add($script:lstEvents)

$script:btnClearEvents = New-FlatButton 'Clear' $script:Pal.Ghost $true
$script:btnClearEvents.TabIndex = 9
$script:cardEvents.Controls.Add($script:btnClearEvents)

$script:lblEventsEmpty = New-Label 'No activity yet.  Open a session to see the events here.' $script:F.Small $script:Theme.TextFaint $false
$script:cardEvents.Controls.Add($script:lblEventsEmpty)

# ------------------------------------------------------------------ status bar
$script:cardStatus = New-Card '' (Px 760) ($script:M.StatusH)
$script:cardStatus.TabIndex = 10
$script:form.Controls.Add($script:cardStatus)

$script:lblState = New-Label 'Stopped' $script:F.State $script:Theme.TextMuted $false
$script:lblState.AutoEllipsis = $true
$script:cardStatus.Controls.Add($script:lblState)

$script:lblDetail = New-Label '' $script:F.Body $script:Theme.TextMuted $false
$script:lblDetail.AutoEllipsis = $true
$script:cardStatus.Controls.Add($script:lblDetail)

$script:btnOpenLog = New-FlatButton 'Open Log' $script:Pal.Ghost $true
$script:btnOpenLog.TabIndex = 11
$script:cardStatus.Controls.Add($script:btnOpenLog)

$script:btnLogFolder = New-FlatButton 'Log Folder' $script:Pal.Ghost $true
$script:btnLogFolder.TabIndex = 12
$script:cardStatus.Controls.Add($script:btnLogFolder)

# ------------------------------------------------------------- message bar
# Reserved strip under the action buttons.  It shows guidance when idle,
# coloured feedback after an action and a marquee while a command runs - it
# never covers other content and never changes layout bounds (no jitter).
$script:Notice = @{ Text = ''; Kind = '' }
$script:IdleHint = ''
$script:BusyShown = $false
$script:BusyPhase = 0
$script:pnlNotice = New-Object System.Windows.Forms.Panel
$script:pnlNotice.BackColor = $script:Theme.Surface2
$script:pnlNotice.Cursor = [System.Windows.Forms.Cursors]::Hand
$script:pnlNotice.Add_Paint({
    param($s, $e)
    if ($s.Width -lt 8 -or $s.Height -lt 8) { return }
    $g = $e.Graphics
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $st = $script:Notice
    $kind = $st.Kind
    $text = [string]$st.Text
    if (-not $kind -and $script:BusyShown) {
        $kind = 'info'
        $text = ([string]$script:BusyText) + ' ...'
    }
    if (-not $text) { $text = [string]$script:IdleHint }
    $accent = $script:Theme.Border
    $bg = $script:Theme.Surface2
    $fg = $script:Theme.TextMuted
    $glyph = ''
    switch ($kind) {
        'ok'   { $accent = $script:Theme.Ok;   $bg = $script:Theme.OkSoft;   $fg = $script:Theme.Ok;   $glyph = '+' }
        'warn' { $accent = $script:Theme.Warn; $bg = $script:Theme.WarnSoft; $fg = $script:Theme.Warn; $glyph = '!' }
        'err'  { $accent = $script:Theme.Err;  $bg = $script:Theme.ErrSoft;  $fg = $script:Theme.Err;  $glyph = '!' }
        'info' { $accent = $script:Theme.Info; $bg = $script:Theme.InfoSoft; $fg = $script:Theme.Info; $glyph = 'i' }
    }
    $path = New-RoundPath 0 0 ($s.Width - 1) ($s.Height - 1) (Px 8)
    $fb = New-Object System.Drawing.SolidBrush $bg
    $pb = New-Object System.Drawing.Pen($accent, 1)
    $g.FillPath($fb, $path)
    $g.DrawPath($pb, $path)
    $fb.Dispose()
    $pb.Dispose()
    $path.Dispose()
    $tx = Px 16
    if ($kind) {
        $bar = New-Object System.Drawing.SolidBrush $accent
        $g.FillRectangle($bar, 1, 1, (Px 5), ($s.Height - 2))
        $bar.Dispose()
        $ib = New-Object System.Drawing.SolidBrush $accent
        $g.DrawString($glyph, $script:F.Pill, $ib, (Px 15), [int](($s.Height - $script:F.Pill.Height) / 2))
        $ib.Dispose()
        $tx = Px 32
    }
    $txb = New-Object System.Drawing.SolidBrush $fg
    $g.DrawString($text, $script:F.Body, $txb, $tx, [int](($s.Height - $script:F.Body.Height) / 2))
    $txb.Dispose()
    if ($script:BusyShown) {
        $track = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(45, $accent))
        $g.FillRectangle($track, 1, ($s.Height - (Px 4)), ($s.Width - 2), (Px 3))
        $track.Dispose()
        $segW = [int][Math]::Max((Px 60), ($s.Width * 0.18))
        $span = [int][Math]::Max(1, ($s.Width + $segW))
        $segX = [int]($script:BusyPhase % $span) - $segW
        $x0 = [Math]::Max(1, $segX)
        $x1 = [Math]::Min(($s.Width - 2), ($segX + $segW))
        if ($x1 -gt $x0) {
            $seg = New-Object System.Drawing.SolidBrush $accent
            $g.FillRectangle($seg, $x0, ($s.Height - (Px 4)), ($x1 - $x0), (Px 3))
            $seg.Dispose()
        }
    }
})
$script:form.Controls.Add($script:pnlNotice)

# ------------------------------------------------------------------ tooltips
$script:tip = New-Object System.Windows.Forms.ToolTip
$script:tip.AutoPopDelay = 12000
$script:tip.InitialDelay = 350
$script:tip.ReshowDelay = 100
$script:tip.SetToolTip($script:btnPower, 'Open / close the Tera Term session        Ctrl+O')
$script:tip.SetToolTip($script:btnPauseResume, 'Pause = disconnect the serial port (Tera Term stays open)' + [Environment]::NewLine + 'Start = reconnect it                              Ctrl+P')
$script:tip.SetToolTip($script:btnLog, 'Start / stop logging to a timestamped file (session stays open)   Ctrl+L')
$script:tip.SetToolTip($script:btnReset, 'Press and hold = device RESET held LOW (chip stopped)' + [Environment]::NewLine + 'Release = chip runs          Ctrl+R for a short tap')
$script:tip.SetToolTip($script:btnHold, 'Latch mode for Restart Device: ON = one click holds RESET low until clicked again.' + [Environment]::NewLine + 'OFF = classic press-and-hold.')
$script:tip.SetToolTip($script:chkAutoLog, 'Start logging automatically as soon as the session opens.')
$script:tip.SetToolTip($script:chkTimestamp, 'Prefix every logged line with a local timestamp.')
$script:tip.SetToolTip($script:btnRefresh, 'Rescan the serial ports (F5)')
$script:tip.SetToolTip($script:btnOpenLog, 'Open the current log file')
$script:tip.SetToolTip($script:btnLogFolder, 'Open the log folder in Explorer (Ctrl+D)')
$script:tip.SetToolTip($script:btnClearEvents, 'Clear the activity list (Ctrl+E)')
$script:tip.SetToolTip($script:lstEvents, 'Recent session events, newest at the bottom.')
$script:tip.SetToolTip($script:btnMonitor, 'Open the live debug monitor - it follows the log file with its timestamps (Ctrl+M).')
$script:tip.SetToolTip($script:btnAdvanced, 'Advanced settings: reset hold, command timeout, extra Tera Term options, close behaviour.')

# -------------------------------------------------------------------- layout
function Invoke-Layout {
    if (-not $script:FormReady) { return }
    $f = $script:form
    $cw = $f.ClientSize.Width
    $ch = $f.ClientSize.Height
    if ($cw -lt (Px 620) -or $ch -lt (Px 480)) { return }
    $pad = $script:M.Pad
    $gap = $script:M.Gap
    $contentW = $cw - ($pad * 2)
    $f.SuspendLayout()

    # header
    $script:pnlHeader.SetBounds(0, 0, $cw, $script:M.HeaderH)
    $script:pnlAccent.SetBounds(0, $script:M.HeaderH, $cw, $script:M.AccentH)
    Update-HeaderArt

    # cards + action rows
    $y = $script:M.HeaderH + $script:M.AccentH + $script:M.RowGap
    $script:cardConn.SetBounds($pad, $y, $contentW, $script:M.ConnH)
    $y += $script:M.ConnH + $script:M.RowGap
    $script:cardTT.SetBounds($pad, $y, $contentW, $script:M.TtH)
    $y += $script:M.TtH + $script:M.RowGap
    # two balanced action cards, filled edge to edge with equal buttons
    $colW = [int](($contentW - $gap) / 2)
    $colW2 = $contentW - $colW - $gap
    $script:cardSession.SetBounds($pad, $y, $colW, $script:M.ActionH)
    $script:cardDevice.SetBounds(($pad + $colW + $gap), $y, $colW2, $script:M.ActionH)
    $inW = $colW - ($script:M.Pad * 2)
    $inW2 = $colW2 - ($script:M.Pad * 2)
    $script:btnPower.SetBounds($script:M.Pad, (Px 42), $inW, $script:M.BtnH)
    $script:btnPauseResume.SetBounds($script:M.Pad, (Px 96), $inW, $script:M.BtnH)
    Set-RoundRegion $script:btnPower (Px 7)
    Set-RoundRegion $script:btnPauseResume (Px 7)
    $holdW = [int](Px 74)
    $script:btnLog.SetBounds($script:M.Pad, (Px 42), $inW2, $script:M.BtnH)
    $script:btnReset.SetBounds($script:M.Pad, (Px 96), ($inW2 - $holdW - (Px 8)), $script:M.BtnH)
    $script:btnHold.SetBounds(($script:M.Pad + $inW2 - $holdW), (Px 96), $holdW, $script:M.BtnH)
    Set-RoundRegion $script:btnLog (Px 7)
    Set-RoundRegion $script:btnReset (Px 7)
    Set-RoundRegion $script:btnHold (Px 7)
    $script:btnMonitor.SetBounds(($script:M.Pad + $colW2 - (Px 96)), (Px 10), (Px 96), (Px 24))
    Set-RoundRegion $script:btnMonitor (Px 6)
    $y += $script:M.ActionH + $script:M.RowGap

    # message bar: reserved slot, so feedback never covers real content
    $script:pnlNotice.SetBounds($pad, $y, $contentW, $script:M.NoticeH)
    Set-RoundRegion $script:pnlNotice (Px 8)
    $y += $script:M.NoticeH + $script:M.RowGap

    # bottom anchored blocks
    $statusY = $ch - $pad - $script:M.StatusH
    $eventsH = $statusY - $script:M.RowGap - $y
    if ($eventsH -lt $script:M.EventsMin) { $eventsH = $script:M.EventsMin }
    $script:cardEvents.SetBounds($pad, $y, $contentW, $eventsH)
    $script:cardStatus.SetBounds($pad, $statusY, $contentW, $script:M.StatusH)

    # connection card internals
    $w = $script:cardConn.Width
    $script:lblComCap.Location = (Pt 17 47)
    $browseW = [int](Px 100)
    $script:btnRefresh.SetBounds(($w - $pad - $browseW), (Px 40), $browseW, (Px 28))
    Set-RoundRegion $script:btnRefresh (Px 6)
    $script:boxCom.SetBounds((Px 104), (Px 40), ($w - $pad - $browseW - (Px 104) - (Px 12)), (Px 28))
    $script:lblBaudCap.Location = (Pt 17 81)
    $script:boxBaud.SetBounds((Px 104), (Px 74), (Px 140), (Px 28))
    $script:chkAutoLog.Location = (Pt 268 81)

    # tera term card internals
    $w = $script:cardTT.Width
    $script:lblExeCap.Location = (Pt 17 47)
    $script:lblIniCap.Location = (Pt 17 81)
    $script:lblLogDirCap.Location = (Pt 17 115)
    $script:lblPatternCap.Location = (Pt 17 149)
    $script:btnExe.SetBounds(($w - $pad - $browseW), (Px 40), $browseW, (Px 28))
    $script:btnIni.SetBounds(($w - $pad - $browseW), (Px 74), $browseW, (Px 28))
    $script:btnLogDir.SetBounds(($w - $pad - $browseW), (Px 108), $browseW, (Px 28))
    Set-RoundRegion $script:btnExe (Px 6)
    Set-RoundRegion $script:btnIni (Px 6)
    Set-RoundRegion $script:btnLogDir (Px 6)
    $longW = $w - $pad - $browseW - (Px 104) - (Px 12)
    $script:boxExe.SetBounds((Px 104), (Px 40), $longW, (Px 28))
    $script:boxIni.SetBounds((Px 104), (Px 74), $longW, (Px 28))
    $script:boxLogDir.SetBounds((Px 104), (Px 108), $longW, (Px 28))
    $script:boxPattern.SetBounds((Px 104), (Px 142), (Px 240), (Px 28))
    $script:chkTimestamp.Location = (Pt 364 149)
    $script:lblTokens.Location = (Pt 106 176)
    $script:btnAdvanced.SetBounds(($w - $pad - (Px 108)), (Px 10), (Px 108), (Px 24))
    Set-RoundRegion $script:btnAdvanced (Px 6)

    # activity card internals
    $w = $script:cardEvents.Width
    $h = $script:cardEvents.Height
    $script:btnClearEvents.SetBounds(($w - $pad - (Px 76)), (Px 10), (Px 76), (Px 24))
    Set-RoundRegion $script:btnClearEvents (Px 6)
    $listH = $h - (Px 56)
    if ($listH -lt (Px 40)) { $listH = Px 40 }
    $script:lstEvents.SetBounds((Px 14), (Px 44), ($w - (Px 28)), $listH)
    $script:lblEventsEmpty.SetBounds((Px 20), (Px 52), ($w - (Px 40)), (Px 20))

    # status card internals
    $w = $script:cardStatus.Width
    $btnW2 = [int](Px 104)
    $textW = $w - (Px 16) - ($btnW2 * 2) - (Px 30)
    if ($textW -lt (Px 120)) { $textW = Px 120 }
    $script:lblState.SetBounds((Px 17), (Px 13), $textW, (Px 22))
    $script:lblDetail.SetBounds((Px 17), (Px 39), $textW, (Px 20))
    $script:btnLogFolder.SetBounds(($w - $pad - $btnW2), (Px 23), $btnW2, (Px 24))
    $script:btnOpenLog.SetBounds(($w - $pad - $btnW2 - (Px 8) - $btnW2), (Px 23), $btnW2, (Px 24))
    Set-RoundRegion $script:btnLogFolder (Px 6)
    Set-RoundRegion $script:btnOpenLog (Px 6)

    $f.ResumeLayout()
    $f.Invalidate()
}

# ============================================================ notices / events
function Show-Notice {
    param([string]$Text, [string]$Kind = 'info')
    if (-not $script:pnlNotice) { return }
    $script:Notice = @{ Text = $Text; Kind = $Kind }
    $script:pnlNotice.Invalidate()
    $script:NoticeTimer.Stop()
    if ($Kind -ne 'err') {
        # auto-dismiss (errors stay until they are clicked away)
        $script:NoticeTimer.Interval = 5000
        $script:NoticeTimer.Start()
    }
    Add-Event -Text $Text -Kind $Kind
}

function Hide-Notice {
    try { $script:NoticeTimer.Stop() } catch { }
    $script:Notice = @{ Text = ''; Kind = '' }
    if ($script:pnlNotice) { $script:pnlNotice.Invalidate() }
}

function Update-EventsEmpty {
    if (-not $script:lblEventsEmpty) { return }
    $show = ($script:lstEvents.Items.Count -eq 0)
    $script:lblEventsEmpty.Visible = $show
    if ($show) { $script:lblEventsEmpty.BringToFront() }
}

function Add-Event {
    param([string]$Text, [string]$Kind = 'info')
    if (-not $script:lstEvents) { return }
    $col = $script:Theme.Text
    switch ($Kind) {
        'ok'   { $col = $script:Theme.Ok }
        'warn' { $col = $script:Theme.Warn }
        'err'  { $col = $script:Theme.Err }
        'info' { $col = $script:Theme.Info }
        'dim'  { $col = $script:Theme.TextFaint }
    }
    [void]$script:lstEvents.Items.Add(('[' + (Get-Date -Format 'HH:mm:ss') + ']  ' + $Text))
    [void]$script:EventKinds.Add($col)
    while ($script:lstEvents.Items.Count -gt 400) {
        $script:lstEvents.Items.RemoveAt(0)
        $script:EventKinds.RemoveAt(0)
    }
    try { $script:lstEvents.TopIndex = [Math]::Max(0, ($script:lstEvents.Items.Count - 1)) } catch { }
    $script:lstEvents.Invalidate()
    Update-EventsEmpty
}

function Clear-Events {
    $script:lstEvents.Items.Clear()
    $script:EventKinds.Clear()
    $script:lstEvents.Invalidate()
    Update-EventsEmpty
}

$script:NoticeTimer = New-Object System.Windows.Forms.Timer
$script:NoticeTimer.Interval = 5000
$script:NoticeTimer.Add_Tick({ Hide-Notice })

# ---------------------------------------------------------------- visuals
function Set-Pill {
    # No-op when nothing changed: this is what stops the indicator blinking.
    param([string]$Text, $Bg, $Fg)
    $cur = $script:Pill
    if ($cur -and $cur.Text -eq $Text -and $cur.Bg -eq $Bg -and $cur.Fg -eq $Fg) { return }
    $script:Pill = @{ Text = $Text; Bg = $Bg; Fg = $Fg }
    Update-HeaderArt
}

function Update-PowerToggleVisual {
    param([bool]$Running)
    if ($Running) {
        $script:btnPower.Text = '&Close Session'
        Set-ButtonPalette $script:btnPower $script:Pal.Danger
    } else {
        $script:btnPower.Text = '&Open Session'
        Set-ButtonPalette $script:btnPower $script:Pal.Primary
    }
}

function Update-PauseToggleVisual {
    param([bool]$Running)
    if ($Running -and $script:Session.Link -eq 1) {
        $script:btnPauseResume.Text = '&Start Port'
        Set-ButtonPalette $script:btnPauseResume $script:Pal.Success
    } else {
        $script:btnPauseResume.Text = '&Pause Port'
        Set-ButtonPalette $script:btnPauseResume $script:Pal.Warn
    }
}

function Update-LogToggleVisual {
    if ($script:Session.Logging) {
        $script:btnLog.Text = 'Stop &Logging'
        Set-ButtonPalette $script:btnLog $script:Pal.Slate
    } else {
        $script:btnLog.Text = 'Start &Logging'
        Set-ButtonPalette $script:btnLog $script:Pal.Teal
    }
}

function Update-HoldVisual {
    # filled violet = one click latches the reset, outline = press and hold
    if ($script:HoldMode) {
        $script:btnHold.Text = 'Hold on'
        Set-ButtonPalette $script:btnHold $script:Pal.Violet
    } else {
        $script:btnHold.Text = 'Hold off'
        Set-ButtonPalette $script:btnHold $script:Pal.Ghost
    }
}

function Set-ResetVisual {
    param([bool]$Active, [bool]$Latched = $false, [string]$HoldText = '')
    if (-not $script:btnReset) { return }
    if ($Active) {
        if ($HoldText) { $script:btnReset.Text = $HoldText }
        else { $script:btnReset.Text = 'Release &Reset' }
        Set-ButtonPalette $script:btnReset $script:Pal.Danger
    } else {
        $script:btnReset.Text = '&Restart Device'
        Set-ButtonPalette $script:btnReset $script:Pal.Violet
    }
}

# -------------------------------------------------------------- state -> ui
# Busy feedback (delayed so short commands never flash a spinner; minimum
# display time so it cannot blink either).  It never disables or hides any
# control - re-entrancy is prevented with the $script:Busy guard instead.
$script:BusyAt = Get-Date
$script:BusyEndedAt = Get-Date
$script:BusyTimer = New-Object System.Windows.Forms.Timer
$script:BusyTimer.Interval = 50
$script:BusyTimer.Add_Tick({
    $now = Get-Date
    if ($script:Busy) {
        if (-not $script:BusyShown) {
            if (($now - $script:BusyAt).TotalMilliseconds -ge 250) {
                $script:BusyShown = $true
                $script:BusyPhase = 0
                if ($script:pnlNotice) { $script:pnlNotice.Invalidate() }
            }
        } else {
            $script:BusyPhase = $script:BusyPhase + 10
            if ($script:pnlNotice) { $script:pnlNotice.Invalidate() }
        }
    } else {
        if ($script:BusyShown) {
            if (($now - $script:BusyEndedAt).TotalMilliseconds -ge 300) {
                $script:BusyShown = $false
                $script:BusyTimer.Stop()
                if ($script:pnlNotice) { $script:pnlNotice.Invalidate() }
            }
        } else {
            $script:BusyTimer.Stop()
        }
    }
})

function Set-Busy {
    param([bool]$On, [string]$Text = '')
    $script:Busy = $On
    if ($Text) { $script:BusyText = $Text }
    try {
        if ($script:form) {
            if ($On) { $script:form.Cursor = [System.Windows.Forms.Cursors]::WaitCursor }
            else { $script:form.Cursor = [System.Windows.Forms.Cursors]::Default }
        }
    } catch { }
    if ($On) {
        $script:BusyAt = Get-Date
        if (-not $script:BusyTimer.Enabled) { $script:BusyTimer.Start() }
    } else {
        $script:BusyEndedAt = Get-Date
        if (-not $script:BusyShown) { $script:BusyTimer.Stop() }
    }
    Update-Ui
    if ($script:pnlNotice) { $script:pnlNotice.Invalidate() }
}

function Update-Ui {
    $s = $script:Session
    $running = $false
    if ($script:Demo) {
        $running = $true
    } elseif ($s.Proc) {
        try { $s.Proc.Refresh(); $running = -not $s.Proc.HasExited } catch { $running = $false }
    }

    # Enabled state and button colours are touched only when the session state
    # really changed - the 500 ms refresh must never make the UI blink.
    $sig = ('{0}|{1}|{2}|{3}' -f $running, $s.Link, $s.Logging, [bool]($s.LogPath))
    if ($sig -ne $script:UiSig) {
        $script:UiSig = $sig
        $idle = (-not $running)
        Update-PowerToggleVisual $running
        Update-PauseToggleVisual $running
        Update-LogToggleVisual
        Set-ButtonState $script:btnPower $true
        Set-ButtonState $script:btnPauseResume ($running -and (($s.Link -eq 1) -or ($s.Link -eq 2)))
        Set-ButtonState $script:btnLog $running
        Set-ButtonState $script:btnReset $true
        $script:btnRefresh.Enabled = $idle
        $script:cboCom.Enabled = $idle
        $script:cboBaud.Enabled = $idle
        $script:chkAutoLog.Enabled = $idle
        $script:txtExe.Enabled = $idle
        $script:btnExe.Enabled = $idle
        $script:txtIni.Enabled = $idle
        $script:btnIni.Enabled = $idle
        $script:txtLogDir.Enabled = $idle
        $script:btnLogDir.Enabled = $idle
        $script:txtPattern.Enabled = $idle
        $script:chkTimestamp.Enabled = $idle
        Set-ButtonState $script:btnHold $true
        $script:btnOpenLog.Enabled = [bool]($s.LogPath)
        $script:btnLogFolder.Enabled = $true
    }

    $stateTxt = ''
    $stateCol = $script:Theme.TextMuted
    $pillTxt = 'Stopped'
    $pillBg = $script:Theme.NeutralSoft
    $pillFg = $script:Theme.Neutral

    if (-not $running) {
        $stateTxt = 'Session stopped'
        $script:IdleHint = 'Ready.  Pick a COM port, then press Open Session  (Ctrl+O)'
        if ($s.LogPath) { $script:lblDetail.Text = 'Last log:  ' + $s.LogPath }
        else { $script:lblDetail.Text = 'No session running.' }
    } else {
        $connTxt = 'link unknown'
        if ($s.Link -eq 2) { $connTxt = 'Connected' }
        elseif ($s.Link -eq 1) { $connTxt = 'Port disconnected' }
        elseif ($s.Link -eq 0) { $connTxt = 'No link to Tera Term' }
        $bits = New-Object System.Collections.ArrayList
        if ($s.Logging) { [void]$bits.Add('LOGGING') } else { [void]$bits.Add('Session open') }
        [void]$bits.Add((([string]$s.Com).Trim() + ' @ ' + ([string]$s.Baud).Trim()))
        [void]$bits.Add($connTxt)
        if ($s.Proc) { try { [void]$bits.Add('PID ' + $s.Proc.Id) } catch { } }
        if ($s.StartedAt) {
            $up = (Get-Date) - $s.StartedAt
            if ($up.TotalHours -ge 1) { [void]$bits.Add(('up {0:d2}:{1:d2}:{2:d2}' -f [int]$up.TotalHours, $up.Minutes, $up.Seconds)) }
            else { [void]$bits.Add(('up {0:d2}:{1:d2}' -f $up.Minutes, $up.Seconds)) }
        }
        $stateTxt = ($bits.ToArray() -join '    |    ')

        if ($s.Logging -and $s.LogPath) {
            $stateCol = $script:Theme.Ok
            $pillTxt = 'Logging'
            $pillBg = $script:Theme.OkSoft
            $pillFg = $script:Theme.Ok
            $script:IdleHint = 'Recording.  Press Stop Logging to flush and close the file  (Ctrl+L)'
            $sizeTxt = ''
            try {
                if (Test-Path -LiteralPath $s.LogPath) { $sizeTxt = '      ' + (Format-Kb ((Get-Item -LiteralPath $s.LogPath).Length)) }
            } catch { }
            $script:lblDetail.Text = 'Log:  ' + $s.LogPath + $sizeTxt
        } elseif ($s.Link -eq 1) {
            $stateCol = $script:Theme.Warn
            $pillTxt = 'Disconnected'
            $pillBg = $script:Theme.WarnSoft
            $pillFg = $script:Theme.Warn
            $script:IdleHint = 'Serial port disconnected.  Press Start Port to reconnect  (Ctrl+P)'
            $script:lblDetail.Text = 'Serial port disconnected - the session stays open.'
        } elseif ($s.Link -eq 0) {
            $stateCol = $script:Theme.Warn
            $pillTxt = 'No link'
            $pillBg = $script:Theme.WarnSoft
            $pillFg = $script:Theme.Warn
            $script:IdleHint = 'The watcher macro is not answering.  Close the session and open it again.'
            $script:lblDetail.Text = 'Tera Term is running but the watcher macro is not responding.'
        } else {
            $stateCol = $script:Theme.Info
            $pillTxt = 'Connected'
            $pillBg = $script:Theme.InfoSoft
            $pillFg = $script:Theme.Info
            $script:IdleHint = 'Session open.  Press Start Logging to capture the console output  (Ctrl+L)'
            $script:lblDetail.Text = 'Logging is off.'
        }
    }

    $script:lblState.Text = $stateTxt
    $script:lblState.ForeColor = $stateCol
    Set-Pill $pillTxt $pillBg $pillFg
}


function Timer-Tick {
    $s = $script:Session
    if ($s.Proc -and -not $script:Closing) {
        $exited = $false
        try { $s.Proc.Refresh(); $exited = $s.Proc.HasExited } catch { $exited = $true }
        if ($exited) {
            $wasLogging = $s.Logging
            $s.Proc = $null
            $s.State = 'Stopped'
            $s.Logging = $false
            $s.Link = -1
            $s.StartedAt = $null
            if ($wasLogging) { Add-Event 'Tera Term exited - the log was flushed' 'warn' }
            else { Add-Event 'Tera Term exited' 'dim' }
        } else {
            $st = Read-SessionStatus
            if ($st) {
                if ($st.Link -ge 0) {
                    $s.Link = $st.Link
                    $s.LinkStale = 0
                }
                try {
                    $age = (Get-Date) - (Get-Item -LiteralPath $s.Status).LastWriteTime
                    if ($age.TotalSeconds -gt 3) {
                        # The macro can be busy for a moment - require ~2 s of
                        # silence before dropping the link, otherwise the status
                        # pill would flap between Connected and No link.
                        $s.LinkStale = [int]$s.LinkStale + 1
                        if ($s.LinkStale -ge 4) { $s.Link = -1 }
                    } else {
                        $s.LinkStale = 0
                    }
                } catch { }
            }
        }
    }
    if ($s.ResetMode -and (-not $s.ResetLatched) -and $s.ResetTime) {
        $secs = ((Get-Date) - $s.ResetTime).TotalSeconds
        Set-ResetVisual -Active $true -Latched $false -HoldText ('RESET held  {0:N1}s' -f $secs)
    }
    Update-Ui
}

# ------------------------------------------------------------------- actions
function Refresh-ComPorts {
    if ($script:Busy -or $script:Demo) { return }
    $cur = ''
    if ($script:cboCom.SelectedItem) { $cur = [regex]::Match([string]$script:cboCom.SelectedItem, '^COM\d+').Value }
    if ($cur -eq '') { $cur = [string]$script:Settings.ComPort }
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    Set-Busy $true 'Scanning serial ports'
    try {
        $list = @(Get-ComPortList)
        $script:cboCom.Items.Clear()
        foreach ($p in $list) { [void]$script:cboCom.Items.Add($p.Display) }
        $selIdx = -1
        for ($i = 0; $i -lt $list.Count; $i++) { if ($list[$i].Port -eq $cur) { $selIdx = $i; break } }
        if ($selIdx -ge 0) { $script:cboCom.SelectedIndex = $selIdx }
        elseif ($script:cboCom.Items.Count -gt 0) { $script:cboCom.SelectedIndex = 0 }
    } finally { Set-Busy $false }
    if ($script:cboCom.Items.Count -eq 0) {
        Show-Notice -Text 'No serial port found. Plug the board in and press Refresh (F5).' -Kind 'warn'
    } else {
        Add-Event -Text ('Port scan: ' + $script:cboCom.Items.Count + ' port(s) in ' + [int]$sw.ElapsedMilliseconds + ' ms') -Kind 'dim'
    }
}

function Start-Session {
    if ($script:Busy) { return }
    Clear-FieldInvalid
    $s = $script:Session
    $comPort = ''
    if ($script:cboCom.SelectedItem) { $comPort = [regex]::Match([string]$script:cboCom.SelectedItem, '^COM\d+').Value }
    $baud = ($script:cboBaud.Text -replace '\s', '')
    $exe = $script:txtExe.Text.Trim()
    $ini = $script:txtIni.Text.Trim()
    $logDir = $script:txtLogDir.Text.Trim()
    $pattern = $script:txtPattern.Text.Trim()
    $useTimestamp = $script:chkTimestamp.Checked
    $autoLog = $script:chkAutoLog.Checked

    if ($comPort -eq '') {
        Set-FieldInvalid $script:boxCom $true
        Show-Notice -Text 'Select a COM port first - press Refresh (F5) if the list is empty.' -Kind 'warn'
        return
    }
    if ($baud -notmatch '^\d+$') {
        Set-FieldInvalid $script:boxBaud $true
        Show-Notice -Text 'Baud rate must be a number, for example 115200.' -Kind 'warn'
        return
    }
    if ($comPort -eq '' -or -not (Test-Path -LiteralPath $exe)) {
        Set-FieldInvalid $script:boxExe $true
        Show-Notice -Text ('Tera Term not found: ' + $exe) -Kind 'err'
        return
    }
    if (-not (Test-Path -LiteralPath $ini)) {
        Set-FieldInvalid $script:boxIni $true
        Show-Notice -Text ('INI template not found: ' + $ini) -Kind 'err'
        return
    }
    if (-not (Test-Path -LiteralPath $WatcherTemplate)) {
        Show-Notice -Text ('Watcher template not found: ' + $WatcherTemplate) -Kind 'err'
        return
    }
    if (-not (Test-Path -LiteralPath $logDir)) {
        try { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
        catch {
            Set-FieldInvalid $script:boxLogDir $true
            Show-Notice -Text ('Cannot create log folder: ' + $logDir) -Kind 'err'
            return
        }
    }
    if (-not (Test-AsciiPath $logDir)) {
        Set-FieldInvalid $script:boxLogDir $true
        Show-Notice -Text 'The log folder path must be ASCII-only (Tera Term macro limitation).' -Kind 'err'
        return
    }
    if ($pattern -eq '') {
        $pattern = 'esp_wifi_%Y%m%d_%H%M%S.log'
        $script:txtPattern.Text = $pattern
    }
    if ($s.Proc -and (-not $s.Proc.HasExited)) {
        Show-Notice -Text 'A session started by this window is already running.' -Kind 'warn'
        return
    }
    if (-not [bool]$script:Settings.SkipTtWarning) {
        if (Get-Process -Name 'ttermpro' -ErrorAction SilentlyContinue) {
            $r = [System.Windows.Forms.MessageBox]::Show('Another Tera Term instance is already running (not managed by this window). Open an additional session anyway?', $AppTitle, 'YesNo', 'Warning')
            if ($r -ne 'Yes') {
                Add-Event -Text 'Open cancelled: another Tera Term is running' -Kind 'warn'
                return
            }
        }
    }

    $sid = [guid]::NewGuid().ToString('N').Substring(0, 8)
    $s.Id = $sid
    $s.Ctrl = Join-Path $BuildDir ('ctrl_' + $sid + '.txt')
    $s.Status = Join-Path $BuildDir ('status_' + $sid + '.txt')
    $s.Watcher = Join-Path $BuildDir ('watcher_' + $sid + '.ttl')
    $runtimeIni = Join-Path $BuildDir 'runtime_geen.ini'

    Set-Busy $true 'Preparing the session'
    try {
        $iniText = [System.IO.File]::ReadAllText($ini)
        $comNum = ($comPort -replace '\D', '')
        $iniText = [regex]::Replace($iniText, '(?m)^\s*ComPort=.*$', ('ComPort=' + $comNum))
        $iniText = [regex]::Replace($iniText, '(?m)^\s*BaudRate=.*$', ('BaudRate=' + $baud))
        $iniText = [regex]::Replace($iniText, '(?m)^\s*LogAutoStart=.*$', 'LogAutoStart=off')
        # Opening the port must not touch the RESET line.  Tera Term asserts
        # RTS/DTR on open unless these are disabled, and on an auto-reset board
        # that pulse reboots the chip - which is exactly what we must avoid.
        $iniText = Set-IniValue -Text $iniText -Key 'FlowCtrlRTS' -Value '0' -AfterKey 'FlowCtrl'
        $iniText = Set-IniValue -Text $iniText -Key 'FlowCtrlDTR' -Value '0' -AfterKey 'FlowCtrlRTS'
        # Per-line log timestamps: Tera Term supports a strftime format and a
        # choice of clock (Local/UTC/LoggingElapsed/ConnectionElapsed).
        $tsFormat = ([string]$script:Settings.LogTsFormat).Trim()
        if ($tsFormat -ne '') { $iniText = Set-IniValue -Text $iniText -Key 'LogTimestampFormat' -Value $tsFormat -AfterKey 'LogTimestamp' }
        $tsType = ([string]$script:Settings.LogTsType).Trim()
        if ($tsType -notin @('Local', 'UTC', 'LoggingElapsed', 'ConnectionElapsed')) { $tsType = 'Local' }
        $iniText = Set-IniValue -Text $iniText -Key 'LogTimestampType' -Value $tsType -AfterKey 'LogTimestampFormat'
        Write-IniText -Path $runtimeIni -Text $iniText

        $tsFlag = '0'
        if ($useTimestamp) { $tsFlag = '1' }
        $ttl = [System.IO.File]::ReadAllText($WatcherTemplate)
        $tsType = '0'
        switch ([string]$script:Settings.LogTsType) {
            'UTC' { $tsType = '1' }
            'LoggingElapsed' { $tsType = '2' }
            'ConnectionElapsed' { $tsType = '3' }
        }
        $ttl = $ttl.Replace('@@CTRL@@', $s.Ctrl).Replace('@@STATUS@@', $s.Status).Replace('@@TSFLAG@@', $tsFlag).Replace('@@TSTYPE@@', $tsType)
        if ($ttl.Contains('@@')) {
            Show-Notice -Text 'The watcher template still contains placeholders - fix tt_console_watcher.ttl.' -Kind 'err'
            return
        }
        Write-AsciiText -Path $s.Watcher -Text $ttl

        Write-SharedText -Path $s.Ctrl -Text ('0|NOP|||END' + $CRLF)
        Write-SharedText -Path $s.Status -Text ('0|INIT||' + $CRLF)

        $logPath = $null
        if ($autoLog) { $logPath = Join-Path $logDir (Resolve-LogFileName -Pattern $pattern) }

        $argStr = ('/C={0} /SPEED={1} /F="{2}" /M="{3}"' -f $comNum, $baud, $runtimeIni, $s.Watcher)
        if ($autoLog) { $argStr = $argStr + (' /L="{0}"' -f $logPath) }
        $extraArgs = ([string]$script:Settings.ExtraTtArgs).Trim()
        if ($extraArgs -ne '') { $argStr = $argStr + ' ' + $extraArgs }

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $exe
        $psi.Arguments = $argStr
        $psi.WorkingDirectory = $ScriptRoot
        $psi.UseShellExecute = $false

        $script:TtpmacroBefore = @(Get-Process ttpmacro -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
        try { [void][Win32Util.Native]::TimeBeginPeriod(1) } catch { }
        if (-not [bool]$script:Settings.ShowMacroWindow) {
            try { [void][Win32Util.Native]::SetGuardLog((Join-Path $BuildDir 'macro_guard.log')) } catch { }
            try { [void][Win32Util.Native]::StartMacroGuard((Split-Path -Leaf $s.Watcher)) } catch { }
        }
        Write-UiLog ('Start: "' + $exe + '" ' + $argStr)
        $proc = $null
        try { $proc = [System.Diagnostics.Process]::Start($psi) }
        catch {
            Show-Notice -Text ('Cannot start Tera Term: ' + $_.Exception.Message) -Kind 'err'
            return
        }

        $s.Proc = $proc
        $s.Seq = 0
        $s.State = 'Running'
        $s.Com = $comPort
        $s.Baud = $baud
        $s.LogPath = $logPath
        $s.Logging = [bool]$autoLog
        $s.Link = -1
        $s.HideTries = 0
        $s.StartedAt = Get-Date
        if (-not [bool]$script:Settings.ShowMacroWindow) {
            $script:HideTicks = 0
            $script:MacroPid = 0
            $script:HideTimer.Interval = 10
            $script:HideTimer.Start()
        }
        # Find the macro process straight away: the WinEvent hook is then
        # re-armed filtered to that pid *before* its window is ever shown.  An
        # unfiltered hook waits behind every window event on the desktop, which
        # is what used to let the macro window become visible for a moment.
        $swFind = [System.Diagnostics.Stopwatch]::StartNew()
        while ((-not [bool]$script:Settings.ShowMacroWindow) -and $script:MacroPid -eq 0 -and $swFind.ElapsedMilliseconds -lt 1500) {
            foreach ($pr in @(Get-Process ttpmacro -ErrorAction SilentlyContinue)) {
                if ($script:TtpmacroBefore -contains $pr.Id) { continue }
                $script:MacroPid = $pr.Id
                try { [void][Win32Util.Native]::StartMacroGuard((Split-Path -Leaf $s.Watcher), $pr.Id) } catch { }
                break
            }
            if ($script:MacroPid -ne 0) { break }
            [System.Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 1
        }
        Add-Event -Text ('Opening Tera Term on ' + $comPort + ' @ ' + $baud) -Kind 'info'

        Set-Busy $true 'Waiting for the watcher macro'
        $ready = $false
        for ($i = 0; $i -lt 600; $i++) {
            # ~10 ms slices: both the WinEvent hook that hides the macro window
            # and the hide timer only run while messages are being pumped, so a
            # 100 ms sleep would leave that window visible for the whole sleep.
            Wait-UiPump 10
            if (($i % 5) -eq 0) { Hide-WatcherWindows }
            if ($proc.HasExited) { break }
            if (($i % 10) -eq 0) {
                $st = Read-SessionStatus
                if ($st -and ($st.State -eq 'READY' -or $st.State -eq 'IDLE' -or $st.State -eq 'LOGGING' -or $st.State -eq 'ERROR')) { $ready = $true; break }
            }
        }
        if ($proc.HasExited) {
            $s.Proc = $null
            $s.State = 'Stopped'
            $s.Logging = $false
            $s.Link = -1
            $s.StartedAt = $null
            Show-Notice -Text ('Tera Term exited immediately. Check ' + $comPort + ' (busy or missing?) and the settings.') -Kind 'err'
        } elseif (-not $ready) {
            Show-Notice -Text 'Tera Term is running but the watcher macro did not report READY. The log buttons may not work.' -Kind 'warn'
        } else {
            $st = Read-SessionStatus
            if ($st -and $st.Link -ge 0) { $s.Link = $st.Link }
            Add-Event -Text 'Watcher ready' -Kind 'ok'
            Show-Notice -Text ('Session open on ' + $comPort + ' @ ' + $baud) -Kind 'ok'
        }
    } finally {
        try { [void][Win32Util.Native]::TimeEndPeriod(1) } catch { }
        Set-Busy $false
    }
    Update-Ui
}

function Start-Logging {
    if ($script:Busy) { return }
    $s = $script:Session
    if (-not $s.Proc -or $s.Proc.HasExited) { return }
    $logDir = $script:txtLogDir.Text.Trim()
    $pattern = $script:txtPattern.Text.Trim()
    if ($pattern -eq '') { $pattern = 'esp_wifi_%Y%m%d_%H%M%S.log' }
    if (-not (Test-Path -LiteralPath $logDir)) {
        try { New-Item -ItemType Directory -Path $logDir -Force | Out-Null }
        catch {
            Set-FieldInvalid $script:boxLogDir $true
            Show-Notice -Text ('Cannot create log folder: ' + $logDir) -Kind 'err'
            return
        }
    }
    $logPath = Join-Path $logDir (Resolve-LogFileName -Pattern $pattern)
    $p = $null
    Set-Busy $true 'Starting the log'
    try {
        try { $seq = Send-TtlCommand -Cmd 'LOGSTART' -Arg1 $logPath }
        catch {
            Show-Notice -Text ('Cannot send the command: ' + $_.Exception.Message) -Kind 'err'
            return
        }
        $p = Wait-TtlAck -Seq $seq
    } finally { Set-Busy $false }
    if ($null -eq $p) {
        Show-Notice -Text 'The watcher did not answer LOGSTART (timeout).' -Kind 'err'
        return
    }
    if ($p.Length -ge 2 -and $p[1] -eq 'LOGGING') {
        $s.Logging = $true
        $s.LogPath = $logPath
        Show-Notice -Text ('Logging to ' + (Split-Path -Leaf $logPath)) -Kind 'ok'
    } else {
        Show-Notice -Text ('Tera Term could not open the log file: ' + $logPath) -Kind 'err'
    }
    Update-Ui
}

function Stop-Logging {
    if ($script:Busy) { return }
    $s = $script:Session
    if (-not $s.Proc -or $s.Proc.HasExited) { return }
    $p = $null
    Set-Busy $true 'Stopping the log'
    try {
        try { $seq = Send-TtlCommand -Cmd 'LOGSTOP' }
        catch {
            Show-Notice -Text ('Cannot send the command: ' + $_.Exception.Message) -Kind 'err'
            return
        }
        $p = Wait-TtlAck -Seq $seq
    } finally { Set-Busy $false }
    if ($null -eq $p) {
        Show-Notice -Text 'The watcher did not answer LOGSTOP (timeout).' -Kind 'err'
        return
    }
    $s.Logging = $false
    Show-Notice -Text 'Log closed and flushed.' -Kind 'ok'
    Update-Ui
}

function Pause-Session {
    if ($script:Busy) { return }
    $s = $script:Session
    if (-not $s.Proc -or $s.Proc.HasExited) { return }
    $p = $null
    Set-Busy $true 'Disconnecting the port'
    try {
        try { $seq = Send-TtlCommand -Cmd 'DISCONNECT' }
        catch {
            Show-Notice -Text ('Cannot send the command: ' + $_.Exception.Message) -Kind 'err'
            return
        }
        $p = Wait-TtlAck -Seq $seq
    } finally { Set-Busy $false }
    if ($null -eq $p) {
        Show-Notice -Text 'The watcher did not answer DISCONNECT (timeout).' -Kind 'err'
        return
    }
    if ($p.Length -ge 4) {
        $n = 0
        if ([int]::TryParse($p[3], [ref]$n)) { $s.Link = $n }
    }
    Show-Notice -Text 'Serial port disconnected - the session stays open.' -Kind 'warn'
    Update-Ui
}

function Resume-Session {
    if ($script:Busy) { return }
    $s = $script:Session
    if (-not $s.Proc -or $s.Proc.HasExited) { return }
    $params = ('/C={0} /SPEED={1}' -f ($s.Com -replace '\D', ''), $s.Baud)
    $p = $null
    Set-Busy $true 'Reconnecting the port'
    try {
        try { $seq = Send-TtlCommand -Cmd 'CONNECT' -Arg1 $params }
        catch {
            Show-Notice -Text ('Cannot send the command: ' + $_.Exception.Message) -Kind 'err'
            return
        }
        $p = Wait-TtlAck -Seq $seq
    } finally { Set-Busy $false }
    if ($null -eq $p) {
        Show-Notice -Text 'The watcher did not answer CONNECT (timeout).' -Kind 'err'
        return
    }
    if ($p.Length -ge 4) {
        $n = 0
        if ([int]::TryParse($p[3], [ref]$n)) { $s.Link = $n }
    }
    if ($s.Link -ne 2) {
        Show-Notice -Text ('Reconnect failed (link=' + $s.Link + '). Is the COM port busy?') -Kind 'err'
    } else {
        Show-Notice -Text ('Reconnected on ' + $s.Com + ' @ ' + $s.Baud) -Kind 'ok'
    }
    Update-Ui
}

# Tera Term flashes the ttpmacro helper window when the watcher starts.  Its
# buttons are unused, so it is hidden as soon as it exists: a 35 ms poll keeps
# the flash to a single frame instead of the previous ~500 ms.
$script:TtpmacroBefore = @()
function Hide-WatcherWindows {
    $s = $script:Session
    $name = ''
    if ($s.Watcher) { $name = Split-Path -Leaf $s.Watcher }
    $before = $script:TtpmacroBefore
    try {
        foreach ($proc in @(Get-Process ttpmacro -ErrorAction SilentlyContinue)) {
            $mine = $false
            if ($before -and -not $before.Contains($proc.Id)) { $mine = $true }
            if (-not $mine -and $name) {
                $title = ''
                try { $title = [string]$proc.MainWindowTitle } catch { }
                if ($title -and $title.Contains($name)) { $mine = $true }
            }
            if (-not $mine) { continue }
            $h = $proc.MainWindowHandle
            if ($h -eq [IntPtr]::Zero -and $name) {
                # Process.MainWindowHandle becomes 0 once the helper window is
                # hidden (and while its caption is not set yet), so fall back to
                # FindWindow, which also locates hidden top-level windows.
                # Two details matter here:
                #   * the caption is 'MACRO - <file>', not the full macro path;
                #   * the class name must be a real NULL - passing $null makes
                #     PowerShell bind an empty string, which never matches.
                try { $h = [Win32Util.Native]::FindWindow([NullString]::Value, ('MACRO - ' + $name)) } catch { }
            }
            if ($h -ne [IntPtr]::Zero) {
                try { [void][Win32Util.Native]::ShowWindow($h, 0) } catch { }
            }
        }
    } catch { }
}

$script:HideTicks = 0
$script:HideTimer = New-Object System.Windows.Forms.Timer
$script:HideTimer.Interval = 10
$script:HideTimer.Add_Tick({
    $s = $script:Session
    Hide-WatcherWindows
    if ($script:MacroPid -eq 0 -and $s.Watcher) {
        foreach ($pr in @(Get-Process ttpmacro -ErrorAction SilentlyContinue)) {
            if ($script:TtpmacroBefore -contains $pr.Id) { continue }
            $script:MacroPid = $pr.Id
            try { [void][Win32Util.Native]::StartMacroGuard((Split-Path -Leaf $s.Watcher), $pr.Id) } catch { }
            $script:HideTimer.Interval = 60
            break
        }
    }
    $script:HideTicks = [int]$script:HideTicks + 1
    if ($script:HideTicks -gt 600 -or -not $s.Proc -or $s.Proc.HasExited) { $script:HideTimer.Stop() }
})

function Assert-DeviceReset {
    param([bool]$Latched = $false)
    # Hold the device RESET line LOW via RTS only (release = the chip runs).
    # DTR is never driven by this UI.
    #   connected    -> through the Tera Term watcher (setrts 1)
    #   disconnected -> open the COM port directly and assert RTS
    $s = $script:Session
    if ($s.ResetPressing -or $s.ResetMode) { return }
    $s.ResetPressing = $true
    $s.ResetPendingRelease = $false
    $com = ''
    try {
        if ($s.Proc -and (-not $s.Proc.HasExited) -and $s.Link -eq 2) {
            try {
                [void](Send-TtlCommand -Cmd 'SETRTS' -Arg1 '1')
                $s.ResetMode = 'ttl'
            } catch { $s.ResetMode = $null }
        }
        if (-not $s.ResetMode) {
            if ($s.Proc -and (-not $s.Proc.HasExited) -and $s.Com) { $com = $s.Com }
            if (-not $com -and $script:cboCom.SelectedItem) { $com = [regex]::Match([string]$script:cboCom.SelectedItem, '^COM\d+').Value }
            if ($com -eq '') {
                Show-Notice -Text 'Select a COM port before restarting the device.' -Kind 'warn'
                return
            }
            $sp = New-Object System.IO.Ports.SerialPort $com
            $sp.BaudRate = 115200
            $sp.Handshake = 'None'
            $sp.Open()
            Start-Sleep -Milliseconds 120
            $sp.RtsEnable = $true
            $s.ResetPort = $sp
            $s.ResetMode = 'port'
        }
        $s.ResetLatched = $Latched
        $s.ResetTime = Get-Date
        Set-ResetVisual -Active $true -Latched $Latched -HoldText 'RESET held  0.0s'
        $comTag = $com
        if (-not $comTag) { $comTag = $s.Com }
        Write-UiLog ('Reset asserted via ' + $s.ResetMode + ' (' + $comTag + ')')
        if ($Latched) { Add-Event -Text 'Device reset latched (RESET held LOW)' -Kind 'warn' }
        else { Add-Event -Text 'Device reset asserted (RESET held LOW)' -Kind 'warn' }
    } catch {
        Show-Notice -Text ('Cannot assert RESET on ' + $com + ': ' + $_.Exception.Message + ' (port busy?)') -Kind 'err'
        if ($s.ResetPort) { try { $s.ResetPort.Close() } catch { } }
        $s.ResetPort = $null
        $s.ResetMode = $null
    } finally {
        $s.ResetPressing = $false
        if ($s.ResetPendingRelease) {
            $s.ResetPendingRelease = $false
            if ($s.ResetMode) { Release-DeviceReset }
        }
    }
}

function Release-DeviceReset {
    $s = $script:Session
    if ($s.ResetPressing -and (-not $s.ResetMode)) { $s.ResetPendingRelease = $true; return }
    if (-not $s.ResetMode) { return }
    # Minimum hold: a short tap must still produce a clean reset pulse (0.5 s).
    if ($s.ResetTime) {
        $held = ((Get-Date) - $s.ResetTime).TotalMilliseconds
        $remain = [int]((Get-ResetHoldMs) - $held)
        if ($remain -gt 0) {
            $swm = [System.Diagnostics.Stopwatch]::StartNew()
            while ($swm.ElapsedMilliseconds -lt $remain) {
                [System.Windows.Forms.Application]::DoEvents()
                Start-Sleep -Milliseconds 20
            }
        }
    }
    if ($s.ResetMode -eq 'ttl') {
        if ($s.Proc -and (-not $s.Proc.HasExited)) {
            try { [void](Send-TtlCommand -Cmd 'SETRTS' -Arg1 '0') } catch { }
        }
    } elseif ($s.ResetMode -eq 'port') {
        try { if ($s.ResetPort) { $s.ResetPort.RtsEnable = $false; $s.ResetPort.Close() } } catch { }
        $s.ResetPort = $null
    }
    $s.ResetMode = $null
    $s.ResetTime = $null
    $s.ResetLatched = $false
    Set-ResetVisual -Active $false
    Add-Event -Text 'Device reset released (chip running)' -Kind 'ok'
}

function Close-Session {
    if ($script:Busy) { return }
    $s = $script:Session
    try { Release-DeviceReset } catch { }
    if (-not $s.Proc) { return }
    # Cache the process: Timer-Tick may clear $s.Proc while we pump messages,
    # and touching a null process is what used to throw here.
    $proc = $s.Proc
    $script:Closing = $true
    Set-Busy $true 'Closing Tera Term'
    try {
        if ($proc -and -not $proc.HasExited) {
            try {
                [void](Send-TtlCommand -Cmd 'SETRTS' -Arg1 '0')
                Wait-UiPump 350
            } catch { }
            try { [void](Send-TtlCommand -Cmd 'QUIT') } catch { }
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            while ($sw.ElapsedMilliseconds -lt 4000) {
                [System.Windows.Forms.Application]::DoEvents()
                try { $proc.Refresh(); if ($proc.HasExited) { break } } catch { break }
                Start-Sleep -Milliseconds 100
            }
            try { $proc.Refresh() } catch { }
            if ($proc -and -not $proc.HasExited) {
                try { [void]$proc.CloseMainWindow() } catch { }
                $sw.Restart()
                while ($sw.ElapsedMilliseconds -lt 3000) {
                    [System.Windows.Forms.Application]::DoEvents()
                    try { $proc.Refresh(); if ($proc.HasExited) { break } } catch { break }
                    Start-Sleep -Milliseconds 100
                }
                try { $proc.Refresh() } catch { }
                if ($proc -and -not $proc.HasExited) {
                    try { Stop-Process -Id $proc.Id -Force -ErrorAction Stop } catch { }
                }
            }
        }
        Show-Notice -Text 'Session closed.' -Kind 'ok'
    } finally {
        $s.Proc = $null
        $s.State = 'Stopped'
        $s.Logging = $false
        $s.Link = -1
        $s.StartedAt = $null
        $script:Closing = $false
        try { $script:HideTimer.Stop() } catch { }
        try { [Win32Util.Native]::StopMacroGuard() } catch { }
        Set-Busy $false
    }
    Update-Ui
}
function Copy-LogPath {
    $p = $script:Session.LogPath
    if (-not $p) { $p = $script:txtLogDir.Text.Trim() }
    if (-not $p) { Show-Notice -Text 'No log path to copy yet.' -Kind 'warn'; return }
    try {
        [System.Windows.Forms.Clipboard]::SetText([string]$p)
        Show-Notice -Text ('Copied: ' + $p) -Kind 'ok'
    } catch { Show-Notice -Text 'Clipboard is busy - try again.' -Kind 'warn' }
}

function Open-LogFolder {
    $d = $script:txtLogDir.Text.Trim()
    if ($d -and (Test-Path -LiteralPath $d)) { Start-Process -FilePath 'explorer.exe' -ArgumentList ('"' + $d + '"') | Out-Null }
    else { Show-Notice -Text ('Folder not found: ' + $d) -Kind 'warn' }
}

function Open-LogFile {
    $p = $script:Session.LogPath
    if ($p -and (Test-Path -LiteralPath $p)) { Start-Process -FilePath $p | Out-Null }
    else { Show-Notice -Text 'No log file for this session yet.' -Kind 'warn' }
}

# ------------------------------------------------------------ debug monitor
# Tera Term itself has no live on-screen timestamp - timestamps only exist in
# the log file.  The monitor therefore tails that file, so the same timestamped
# lines are visible live while the session runs.
$script:MonitorForm = $null
$script:MonList = $null
$script:MonPos = 0
$script:MonPath = ''
$script:MonFollow = $true

function Add-MonitorLine {
    param([string]$Line)
    $lst = $script:MonList
    if (-not $lst) { return }
    [void]$lst.Items.Add($Line)
    while ($lst.Items.Count -gt 5000) { $lst.Items.RemoveAt(0) }
    if ($script:MonFollow) {
        try { $lst.TopIndex = [Math]::Max(0, ($lst.Items.Count - 1)) } catch { }
    }
}

function Clear-Monitor {
    # A plain call (also used by Update-Monitor when the log path changes or the
    # file was rotated) empties the view and re-reads that file from byte 0.
    # -FromEnd is what the UI "Clear" button uses: empty the view and keep
    # following from the CURRENT END of the file.  Resetting MonPos to 0 there
    # would make the very next update re-read the whole log and refill the list,
    # which is why the button used to look like it did nothing at all.
    param([switch]$FromEnd)
    if ($script:MonList) { $script:MonList.Items.Clear() }
    $script:MonPos = 0
    if ($FromEnd) {
        $p = $script:MonPath
        if (-not $p) { $p = $script:Session.LogPath }
        if ($p -and (Test-Path -LiteralPath $p)) {
            try { $script:MonPos = [long](Get-Item -LiteralPath $p).Length } catch { $script:MonPos = 0 }
        }
    }
}

function Update-Monitor {
    $frm = $script:MonitorForm
    if (-not $frm -or $frm.IsDisposed) { return }
    $path = $script:Session.LogPath
    if (-not $path) {
        $script:lblMonInfo.Text = 'Logging is off - press Start Logging so the monitor has a stream to follow.'
        return
    }
    if ($path -ne $script:MonPath) {
        $script:MonPath = $path
        Clear-Monitor
    }
    if (-not (Test-Path -LiteralPath $path)) {
        $script:lblMonInfo.Text = 'Waiting for the log file: ' + $path
        return
    }
    try {
        $fi = Get-Item -LiteralPath $path
        if ($fi.Length -lt $script:MonPos) { Clear-Monitor }
        if ($fi.Length -gt $script:MonPos) {
            $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            try {
                [void]$fs.Seek($script:MonPos, [System.IO.SeekOrigin]::Begin)
                $len = [int]($fs.Length - $fs.Position)
                if ($len -gt 0) {
                    $buf = New-Object byte[] $len
                    $read = $fs.Read($buf, 0, $len)
                    $lastNl = -1
                    for ($i = $read - 1; $i -ge 0; $i--) { if ($buf[$i] -eq 10) { $lastNl = $i; break } }
                    if ($lastNl -ge 0) {
                        $script:MonPos = $script:MonPos + $lastNl + 1
                        $text = [System.Text.Encoding]::ASCII.GetString($buf, 0, $lastNl + 1)
                        foreach ($line in ($text -split "
")) {
                            $clean = $line.TrimEnd([char]13)
                            if ($clean.Trim().Length -gt 0) { Add-MonitorLine $clean }
                        }
                    }
                }
            } finally { $fs.Close() }
        }
        $state = 'auto-scroll on'
        if (-not $script:MonFollow) { $state = 'auto-scroll off' }
        $script:lblMonInfo.Text = ('Following ' + $path + '   (' + (Format-Kb $fi.Length) + ' , ' + $state + ')')
    } catch {
        $script:lblMonInfo.Text = 'Cannot read the log file: ' + $_.Exception.Message
    }
}

function Show-Monitor {
    if ($script:MonitorForm -and -not $script:MonitorForm.IsDisposed) {
        [void]$script:MonitorForm.Activate()
        return
    }
    $frm = New-Object System.Windows.Forms.Form
    $frm.Text = 'Debug monitor - esp_wifi_template'
    $frm.ClientSize = (Sz 940 540)
    $frm.MinimumSize = (Sz 700 360)
    $frm.StartPosition = 'CenterParent'
    $frm.BackColor = $script:Theme.Bg
    $frm.Font = $script:F.Body
    $frm.AutoScaleMode = 'None'
    $frm.KeyPreview = $true
    $frm.Padding = (New-Object System.Windows.Forms.Padding((Px 12), (Px 6), (Px 12), (Px 12)))
    try { if ($script:form.Icon) { $frm.Icon = $script:form.Icon } } catch { }

    # --- top bar: status label on the left, controls right aligned
    # A TableLayoutPanel + FlowLayoutPanel do the maths, so resizing and DPI
    # scaling can never clip or overlap the buttons again.
    $lbl = New-Label 'Waiting for a session...' $script:F.Small $script:Theme.TextMuted $false
    $lbl.AutoEllipsis = $true
    $lbl.TextAlign = 'MiddleLeft'
    $lbl.Dock = 'Fill'
    $lbl.Margin = (New-Object System.Windows.Forms.Padding(0, 0, (Px 8), 0))
    $script:lblMonInfo = $lbl

    $chkAuto = New-FlatCheck 'Auto-scroll'
    $chkAuto.Checked = $true
    $chkAuto.Margin = (New-Object System.Windows.Forms.Padding((Px 6), (Px 7), (Px 8), (Px 4)))
    $script:chkMonAuto = $chkAuto

    $btnJump = New-FlatButton 'Jump to end' $script:Pal.Ghost $true
    $btnClear = New-FlatButton 'Clear' $script:Pal.Ghost $true
    $btnOpen = New-FlatButton 'Open log' $script:Pal.Ghost $true
    $btnClose = New-FlatButton 'Close' $script:Pal.Slate
    $script:btnMonJump = $btnJump
    $script:btnMonClear = $btnClear
    $script:btnMonOpen = $btnOpen
    $script:btnMonClose = $btnClose

    $flow = New-Object System.Windows.Forms.FlowLayoutPanel
    $flow.AutoSize = $true
    $flow.AutoSizeMode = 'GrowAndShrink'
    $flow.FlowDirection = 'RightToLeft'
    $flow.WrapContents = $false
    $flow.Dock = 'Fill'
    $flow.Margin = (New-Object System.Windows.Forms.Padding(0))
    foreach ($b in @($btnClose, $btnOpen, $btnClear, $btnJump)) {
        # let the flow panel size each button to its caption (DPI safe)
        $b.AutoSize = $true
        $b.AutoSizeMode = 'GrowAndShrink'
        $b.MinimumSize = (New-Object System.Drawing.Size((Px 78), (Px 26)))
        $b.Margin = (New-Object System.Windows.Forms.Padding((Px 4), (Px 6), (Px 4), (Px 6)))
        [void]$flow.Controls.Add($b)
    }
    [void]$flow.Controls.Add($chkAuto)

    $tbl = New-Object System.Windows.Forms.TableLayoutPanel
    $tbl.Dock = 'Top'
    $tbl.Height = (Px 44)
    $tbl.ColumnCount = 2
    $tbl.RowCount = 1
    $tbl.BackColor = $script:Theme.Bg
    [void]$tbl.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 100)))
    [void]$tbl.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::AutoSize)))
    [void]$tbl.Controls.Add($lbl, 0, 0)
    [void]$tbl.Controls.Add($flow, 1, 0)

    $lst = New-Object System.Windows.Forms.ListBox
    $lst.DrawMode = 'OwnerDrawFixed'
    $lst.ItemHeight = (Px 19)
    $lst.Font = (New-Object System.Drawing.Font('Consolas', 9))
    $lst.BackColor = (Hx '#0B1220')
    $lst.ForeColor = (Hx '#E2E8F0')
    $lst.BorderStyle = 'None'
    $lst.SelectionMode = 'None'
    $lst.IntegralHeight = $false
    $lst.HorizontalScrollbar = $true
    $lst.Dock = 'Fill'
    $lst.Margin = (New-Object System.Windows.Forms.Padding(0, (Px 4), 0, 0))
    $lst.Add_DrawItem({
        param($s, $e)
        if ($e.Index -lt 0) { return }
        $g = $e.Graphics
        $bb = New-Object System.Drawing.SolidBrush $s.BackColor
        $g.FillRectangle($bb, $e.Bounds)
        $bb.Dispose()
        $line = [string]$s.Items[$e.Index]
        $y = $e.Bounds.Y + [int](($e.Bounds.Height - $s.Font.Height) / 2)
        $stamp = [regex]::Match($line, '^(\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:\.\d+)?|\d{2}:\d{2}:\d{2}(?:\.\d+)?)')
        if ($stamp.Success) {
            $tb = New-Object System.Drawing.SolidBrush (Hx '#5EEAD4')
            $g.DrawString($stamp.Value, $s.Font, $tb, ($e.Bounds.X + (Px 6)), $y)
            $tb.Dispose()
            $off = $g.MeasureString(($stamp.Value + ' '), $s.Font).Width
            $sb = New-Object System.Drawing.SolidBrush $s.ForeColor
            $g.DrawString($line.Substring($stamp.Value.Length).TrimStart(), $s.Font, $sb, ($e.Bounds.X + (Px 6) + $off), $y)
            $sb.Dispose()
        } else {
            $sb = New-Object System.Drawing.SolidBrush $s.ForeColor
            $g.DrawString($line, $s.Font, $sb, ($e.Bounds.X + (Px 6)), $y)
            $sb.Dispose()
        }
    })

    $frm.Controls.Add($lst)
    $frm.Controls.Add($tbl)
    $script:MonList = $lst

    $chkAuto.Add_CheckedChanged({ $script:MonFollow = [bool]$script:chkMonAuto.Checked; Update-Monitor })
    $btnJump.Add_Click({
        $script:chkMonAuto.Checked = $true
        $script:MonFollow = $true
        try { $script:MonList.TopIndex = [Math]::Max(0, ($script:MonList.Items.Count - 1)) } catch { }
    })
    $btnClear.Add_Click({
        Clear-Monitor -FromEnd
        Update-Monitor
        try { $script:lblMonInfo.Text = 'View cleared - new lines only.   ' + $script:lblMonInfo.Text } catch { }
    })
    $btnOpen.Add_Click({
        $path = $script:Session.LogPath
        if ($path -and (Test-Path -LiteralPath $path)) { Start-Process -FilePath $path | Out-Null }
        else { Show-Notice -Text 'No log file for this session yet.' -Kind 'warn' }
    })
    $btnClose.Add_Click({ $script:MonitorForm.Close() })
    $frm.Add_KeyDown({
        param($s, $e)
        if ($e.KeyCode -eq [System.Windows.Forms.Keys]::Escape) { $script:MonitorForm.Close(); $e.Handled = $true }
    })

    $script:MonitorTimer = New-Object System.Windows.Forms.Timer
    $script:MonitorTimer.Interval = 300
    $script:MonitorTimer.Add_Tick({ Update-Monitor })
    $script:MonitorTimer.Start()
    $frm.Add_FormClosed({
        try { $script:MonitorTimer.Stop() } catch { }
        $script:MonitorForm = $null
        $script:MonPath = ''
        $script:MonPos = 0
    })

    $script:MonitorForm = $frm
    $script:MonPath = ''
    $script:MonPos = 0
    $script:MonFollow = $true
    [void]$frm.Show($script:form)
    Update-Monitor
}
# ------------------------------------------------------- advanced settings
function Show-AdvancedDialog {
    $dlg = New-Object System.Windows.Forms.Form
    $dlg.Text = 'Advanced settings'
    $dlg.ClientSize = (Sz 640 612)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $dlg.ShowInTaskbar = $false
    $dlg.AutoScaleMode = 'None'
    $dlg.BackColor = $script:Theme.Bg
    $dlg.Font = $script:F.Body
    try { if ($script:form.Icon) { $dlg.Icon = $script:form.Icon } } catch { }

    # ---- card 1: timing and logging
    $card1 = New-Card 'Timing and logging' (Px 612) (Px 300)
    $card1.Location = (Pt 14 14)
    $dlg.Controls.Add($card1)

    New-SettingRow $card1 'Reset hold (ms)' 50 'RESET stays low at least this long for a short tap (50-5000).'
    $script:AdvHold = New-TextField
    $card1.Controls.Add($script:AdvHold.Box)
    $script:AdvHold.Box.SetBounds((Px 176), (Px 44), (Px 140), (Px 28))
    $script:AdvHold.Txt.Text = [string](Get-ResetHoldMs)

    New-SettingRow $card1 'Command timeout (ms)' 110 'How long to wait for an answer from the watcher macro (500-60000).'
    $script:AdvAck = New-TextField
    $card1.Controls.Add($script:AdvAck.Box)
    $script:AdvAck.Box.SetBounds((Px 176), (Px 104), (Px 140), (Px 28))
    $script:AdvAck.Txt.Text = [string](Get-AckTimeoutMs)

    New-SettingRow $card1 'Log timestamp format' 170 'strftime format used when "Timestamp each line" is on, e.g. %H:%M:%S.%N'
    $script:AdvTs = New-TextField
    $card1.Controls.Add($script:AdvTs.Box)
    $script:AdvTs.Box.SetBounds((Px 176), (Px 164), (Px 300), (Px 28))
    $script:AdvTs.Txt.Text = [string]$script:Settings.LogTsFormat

    New-SettingRow $card1 'Log timestamp source' 230 'Which clock Tera Term stamps each log line with.'
    $script:AdvTsType = New-ComboField $false 200
    $card1.Controls.Add($script:AdvTsType.Box)
    $script:AdvTsType.Box.SetBounds((Px 176), (Px 224), (Px 240), (Px 28))
    [void]$script:AdvTsType.Cbo.Items.Add('Local time')
    [void]$script:AdvTsType.Cbo.Items.Add('UTC')
    [void]$script:AdvTsType.Cbo.Items.Add('Elapsed since logging started')
    [void]$script:AdvTsType.Cbo.Items.Add('Elapsed since the connection')
    $script:AdvTsType.Cbo.SelectedIndex = 0
    switch ([string]$script:Settings.LogTsType) {
        'UTC' { $script:AdvTsType.Cbo.SelectedIndex = 1 }
        'LoggingElapsed' { $script:AdvTsType.Cbo.SelectedIndex = 2 }
        'ConnectionElapsed' { $script:AdvTsType.Cbo.SelectedIndex = 3 }
    }

    # ---- card 2: behaviour
    $card2 = New-Card 'Behaviour' (Px 612) (Px 200)
    $card2.Location = (Pt 14 326)
    $dlg.Controls.Add($card2)

    New-SettingRow $card2 'On close with a session' 44 ''
    $script:AdvClose = New-ComboField $false 260
    $card2.Controls.Add($script:AdvClose.Box)
    $script:AdvClose.Box.SetBounds((Px 176), (Px 38), (Px 260), (Px 28))
    [void]$script:AdvClose.Cbo.Items.Add('Ask me every time')
    [void]$script:AdvClose.Cbo.Items.Add('Close Tera Term too')
    [void]$script:AdvClose.Cbo.Items.Add('Keep Tera Term running')
    $script:AdvClose.Cbo.SelectedIndex = 0
    switch ([string]$script:Settings.OnCloseAction) {
        'close' { $script:AdvClose.Cbo.SelectedIndex = 1 }
        'keep' { $script:AdvClose.Cbo.SelectedIndex = 2 }
    }

    New-SettingRow $card2 'Extra Tera Term options' 100 'Appended to the Tera Term command line, e.g. /W=120 /H=30'
    $script:AdvArgs = New-TextField
    $card2.Controls.Add($script:AdvArgs.Box)
    $script:AdvArgs.Box.SetBounds((Px 176), (Px 94), (Px 300), (Px 28))
    $script:AdvArgs.Txt.Text = [string]$script:Settings.ExtraTtArgs

    $script:AdvWarn = New-FlatCheck 'Skip the "another Tera Term is running" prompt'
    $card2.Controls.Add($script:AdvWarn)
    $script:AdvWarn.Location = (Pt 22 152)
    $script:AdvWarn.Checked = [bool]$script:Settings.SkipTtWarning

    $script:AdvMacro = New-FlatCheck 'Keep the macro helper window visible (troubleshooting)'
    $card2.Controls.Add($script:AdvMacro)
    $script:AdvMacro.Location = (Pt 22 178)
    $script:AdvMacro.Checked = [bool]$script:Settings.ShowMacroWindow

    $note = New-Label 'Stored in .buildconsole_ui.json - applied from the next session start.' $script:F.Small $script:Theme.TextMuted
    $note.Location = (Pt 18 538)
    $dlg.Controls.Add($note)

    $btnDefaults = New-FlatButton 'Restore defaults' $script:Pal.Ghost $true
    $btnDefaults.SetBounds((Px 14), (Px 564), (Px 150), (Px 34))
    Set-RoundRegion $btnDefaults (Px 7)
    $dlg.Controls.Add($btnDefaults)
    $btnCancel = New-FlatButton 'Cancel' $script:Pal.Ghost $true
    $btnCancel.SetBounds((Px 356), (Px 564), (Px 126), (Px 34))
    Set-RoundRegion $btnCancel (Px 7)
    $dlg.Controls.Add($btnCancel)
    $btnSave = New-FlatButton 'Save' $script:Pal.Primary
    $btnSave.SetBounds((Px 490), (Px 564), (Px 136), (Px 34))
    Set-RoundRegion $btnSave (Px 7)
    $dlg.Controls.Add($btnSave)
    $dlg.AcceptButton = $btnSave
    $dlg.CancelButton = $btnCancel

    $btnDefaults.Add_Click({
        $script:AdvHold.Txt.Text = '500'
        $script:AdvAck.Txt.Text = '4000'
        $script:AdvTs.Txt.Text = '%Y-%m-%d %H:%M:%S.%N'
        $script:AdvTsType.Cbo.SelectedIndex = 0
        $script:AdvClose.Cbo.SelectedIndex = 0
        $script:AdvArgs.Txt.Text = ''
        $script:AdvWarn.Checked = $false
        $script:AdvMacro.Checked = $false
    })
    $btnCancel.Add_Click({ param($s, $e) $s.FindForm().Close() })
    $btnSave.Add_Click({
        $hold = 0; $ack = 0
        if (-not [int]::TryParse(($script:AdvHold.Txt.Text -replace '\s', ''), [ref]$hold) -or $hold -lt 50 -or $hold -gt 5000) {
            Set-FieldInvalid $script:AdvHold.Box $true
            Show-Notice -Text 'Reset hold must be a number between 50 and 5000 ms.' -Kind 'warn'
            return
        }
        if (-not [int]::TryParse(($script:AdvAck.Txt.Text -replace '\s', ''), [ref]$ack) -or $ack -lt 500 -or $ack -gt 60000) {
            Set-FieldInvalid $script:AdvAck.Box $true
            Show-Notice -Text 'Command timeout must be a number between 500 and 60000 ms.' -Kind 'warn'
            return
        }
        $fmt = $script:AdvTs.Txt.Text.Trim()
        if ($fmt -eq '') { $fmt = '%Y-%m-%d %H:%M:%S.%N' }
        $script:Settings.ResetHoldMs = $hold
        $script:Settings.AckTimeoutMs = $ack
        $script:Settings.LogTsFormat = $fmt
        $script:Settings.LogTsType = @('Local', 'UTC', 'LoggingElapsed', 'ConnectionElapsed')[$script:AdvTsType.Cbo.SelectedIndex]
        if ($script:AdvClose.Cbo.SelectedIndex -eq 1) { $script:Settings.OnCloseAction = 'close' }
        elseif ($script:AdvClose.Cbo.SelectedIndex -eq 2) { $script:Settings.OnCloseAction = 'keep' }
        else { $script:Settings.OnCloseAction = 'ask' }
        $script:Settings.SkipTtWarning = [bool]$script:AdvWarn.Checked
        $script:Settings.ShowMacroWindow = [bool]$script:AdvMacro.Checked
        $script:Settings.ExtraTtArgs = $script:AdvArgs.Txt.Text.Trim()
        Save-Settings
        Add-Event -Text ('Advanced saved: reset hold ' + $hold + ' ms, timeout ' + $ack + ' ms, stamp ' + $fmt) -Kind 'dim'
        Show-Notice -Text 'Advanced settings saved.' -Kind 'ok'
        $script:AdvHold.FindForm().Close()
    })

    [void]$dlg.ShowDialog($script:form)
    $dlg.Dispose()
}

# ---------------------------------------------------------------- help dialog
function Show-HelpDialog {
    $dlg = New-Object System.Windows.Forms.Form
    $dlg.Text = 'Shortcuts and about'
    $dlg.ClientSize = (Sz 560 452)
    $dlg.StartPosition = 'CenterParent'
    $dlg.FormBorderStyle = 'FixedDialog'
    $dlg.MaximizeBox = $false
    $dlg.MinimizeBox = $false
    $dlg.ShowInTaskbar = $false
    $dlg.AutoScaleMode = 'None'
    $dlg.BackColor = $script:Theme.Bg
    $dlg.Font = $script:F.Body
    try { if ($script:form.Icon) { $dlg.Icon = $script:form.Icon } } catch { }

    $card = New-Card 'Keyboard shortcuts' ($dlg.ClientSize.Width - (Px 28)) (Px 300)
    $card.Location = (Pt 14 14)
    $dlg.Controls.Add($card)

    $keys = @(
        'Ctrl+O', 'Ctrl+P', 'Ctrl+L', 'Ctrl+R', 'Ctrl+D', 'Ctrl+K', 'Ctrl+E', 'Ctrl+M', 'F5', 'F1', 'Esc'
    ) -join [Environment]::NewLine
    $dsc = @(
        'open / close the Tera Term session',
        'pause (disconnect) / start (reconnect) the serial port',
        'start / stop logging',
        'restart the device (short RESET pulse)',
        'open the log folder',
        'copy the current log path',
        'clear the activity list',
        'open the live debug monitor',
        'rescan the COM ports',
        'this shortcuts window',
        'dismiss the current message'
    ) -join [Environment]::NewLine

    $lk = New-Label $keys $script:F.BodyB $script:Theme.Text $false
    $lk.SetBounds((Px 22), (Px 52), (Px 90), (Px 230))
    $card.Controls.Add($lk)
    $ld = New-Label $dsc $script:F.Body $script:Theme.TextMuted $false
    $ld.SetBounds((Px 118), (Px 52), ($card.Width - (Px 140)), (Px 230))
    $card.Controls.Add($ld)

    $card2 = New-Card 'About' ($dlg.ClientSize.Width - (Px 28)) (Px 96)
    $card2.Location = (Pt 14 326)
    $dlg.Controls.Add($card2)
    $about = New-Label ('TeraTerm Console v2.0  for esp_wifi_template.' + [Environment]::NewLine + 'Settings, session files and logs live in script\tera_console\.build (git-ignored).') $script:F.Small $script:Theme.TextMuted $false
    $about.SetBounds((Px 22), (Px 46), ($card2.Width - (Px 44)), (Px 42))
    $card2.Controls.Add($about)

    $ok = New-FlatButton 'Close' $script:Pal.Primary
    $ok.SetBounds(($dlg.ClientSize.Width - (Px 14) - (Px 120)), ($dlg.ClientSize.Height - (Px 14) - (Px 34)), (Px 120), (Px 34))
    Set-RoundRegion $ok (Px 7)
    $dlg.Controls.Add($ok)
    $dlg.AcceptButton = $ok
    $dlg.CancelButton = $ok
    $ok.Add_Click({ param($s, $e) $s.FindForm().Close() })
    [void]$dlg.ShowDialog($script:form)
    $dlg.Dispose()
}

# ------------------------------------------------------------ window geometry
function Restore-WindowGeometry {
    try {
        $w = [int]$script:Settings.WinW
        $h = [int]$script:Settings.WinH
        if ($w -ge (Px 700) -and $h -ge (Px 840)) {
            $script:form.ClientSize = (New-Object System.Drawing.Size($w, $h))
        }
        $x = [int]$script:Settings.WinX
        $y = [int]$script:Settings.WinY
        if ($x -ge 0 -and $y -ge 0) {
            $wa = [System.Windows.Forms.Screen]::FromPoint((New-Object System.Drawing.Point($x, $y))).WorkingArea
            if ($wa.Width -gt 0) {
                if ($x -gt ($wa.Right - (Px 160))) { $x = $wa.Right - (Px 160) }
                if ($y -gt ($wa.Bottom - (Px 100))) { $y = $wa.Bottom - (Px 100) }
                $script:form.StartPosition = 'Manual'
                $script:form.Location = (New-Object System.Drawing.Point($x, $y))
            }
        }
    } catch { }
}

function Save-WindowGeometry {
    try {
        if ($script:form.WindowState -eq 'Normal') {
            $script:Settings.WinW = [int]$script:form.ClientSize.Width
            $script:Settings.WinH = [int]$script:form.ClientSize.Height
            $script:Settings.WinX = [int]$script:form.Location.X
            $script:Settings.WinY = [int]$script:form.Location.Y
        }
    } catch { }
}

# ================================================================ event wiring
$script:Busy = $false
$script:BusyText = ''
$script:UiSig = ''
$script:Closing = $false
$script:Demo = [bool]$Demo

$script:btnPower.Add_Click({
    if ($script:Session.Proc -and (-not $script:Session.Proc.HasExited)) { Close-Session } else { Start-Session }
})

$script:btnPauseResume.Add_Click({
    $s = $script:Session
    if (-not $s.Proc -or $s.Proc.HasExited) { return }
    if ($s.Link -eq 2) { Pause-Session }
    elseif ($s.Link -eq 1) { Resume-Session }
    else { Show-Notice -Text 'The Tera Term link is not ready yet - wait a second and try again.' -Kind 'warn' }
})

$script:btnLog.Add_Click({
    if ($script:Session.Logging) { Stop-Logging } else { Start-Logging }
})

$script:btnReset.Add_MouseDown({ if (-not $script:HoldMode) { Assert-DeviceReset } })
$script:btnReset.Add_MouseUp({ if (-not $script:HoldMode) { Release-DeviceReset } })
$script:btnReset.Add_Click({
    if ($script:HoldMode) {
        if ($script:Session.ResetMode) { Release-DeviceReset } else { Assert-DeviceReset -Latched $true }
    }
})
$script:btnHold.Add_Click({
    # latch mode: one click holds the reset until it is clicked again
    $script:HoldMode = (-not $script:HoldMode)
    Update-HoldVisual
    if ((-not $script:HoldMode) -and $script:Session.ResetLatched) { Release-DeviceReset }
    if ($script:HoldMode) { Show-Notice -Text 'Hold mode on: one click latches RESET until clicked again.' -Kind 'info' }
    else { Show-Notice -Text 'Hold mode off: classic press-and-hold on Restart Device.' -Kind 'info' }
})

$script:btnRefresh.Add_Click({ Refresh-ComPorts })
$script:btnAdvanced.Add_Click({ if (-not $script:Busy) { Show-AdvancedDialog } })
$script:btnMonitor.Add_Click({ Show-Monitor })
$script:btnOpenLog.Add_Click({ Open-LogFile })
$script:btnLogFolder.Add_Click({ Open-LogFolder })
$script:btnClearEvents.Add_Click({ Clear-Events })
$script:pnlNotice.Add_Click({ if ($script:Notice.Kind) { Hide-Notice } })

$script:btnExe.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Title = 'Select the Tera Term executable'
    $dlg.Filter = 'ttermpro.exe|ttermpro.exe|Executable (*.exe)|*.exe'
    try {
        $dir = Split-Path -Parent $script:txtExe.Text
        if ($dir -and (Test-Path -LiteralPath $dir)) { $dlg.InitialDirectory = $dir }
    } catch { }
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $script:txtExe.Text = $dlg.FileName
        Set-FieldInvalid $script:boxExe $false
    }
    $dlg.Dispose()
})

$script:btnIni.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Title = 'Select the Tera Term INI template'
    $dlg.Filter = 'INI files (*.ini)|*.ini|All files (*.*)|*.*'
    try {
        $dir = Split-Path -Parent $script:txtIni.Text
        if ($dir -and (Test-Path -LiteralPath $dir)) { $dlg.InitialDirectory = $dir }
    } catch { }
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $script:txtIni.Text = $dlg.FileName
        Set-FieldInvalid $script:boxIni $false
    }
    $dlg.Dispose()
})

$script:btnLogDir.Add_Click({
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = 'Select the folder for Tera Term log files'
    if ($script:txtLogDir.Text -and (Test-Path -LiteralPath $script:txtLogDir.Text)) { $dlg.SelectedPath = $script:txtLogDir.Text }
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $script:txtLogDir.Text = $dlg.SelectedPath
        Set-FieldInvalid $script:boxLogDir $false
    }
})

$script:form.Add_Resize({ Invoke-Layout })

$script:form.Add_Shown({
    Invoke-Layout
    Add-Event -Text 'TeraTerm Console ready' -Kind 'info'
    if (-not $script:Demo) { Refresh-ComPorts }
    # neutral initial focus: no highlighted combo text on the very first paint
    try { $script:form.ActiveControl = $script:lstEvents } catch { }
})

$script:form.Add_KeyDown({
    param($s, $e)
    if ($e.Control) {
        switch ([string]$e.KeyCode) {
            'O' { $script:btnPower.PerformClick(); $e.Handled = $true; $e.SuppressKeyPress = $true }
            'P' { if ($script:btnPauseResume.Enabled) { $script:btnPauseResume.PerformClick() }; $e.Handled = $true; $e.SuppressKeyPress = $true }
            'L' { if ($script:btnLog.Enabled) { $script:btnLog.PerformClick() }; $e.Handled = $true; $e.SuppressKeyPress = $true }
            'R' {
                # PerformClick() raises the Click event only: with Hold off (the
                # default) the Click handler does nothing, so the key would be a
                # dead shortcut.  Produce a clean tap instead (assert + release).
                $e.Handled = $true; $e.SuppressKeyPress = $true
                if (-not $script:Busy) {
                    if ($script:HoldMode) { $script:btnReset.PerformClick() }
                    elseif (-not $script:Session.ResetMode) { Assert-DeviceReset; Release-DeviceReset }
                }
            }
            'D' { Open-LogFolder; $e.Handled = $true; $e.SuppressKeyPress = $true }
            'K' { Copy-LogPath; $e.Handled = $true; $e.SuppressKeyPress = $true }
            'E' { Clear-Events; $e.Handled = $true; $e.SuppressKeyPress = $true }
            'M' { Show-Monitor; $e.Handled = $true; $e.SuppressKeyPress = $true }
        }
        return
    }
    if ($e.KeyCode -eq [System.Windows.Forms.Keys]::F5) { Refresh-ComPorts; $e.Handled = $true; $e.SuppressKeyPress = $true }
    elseif ($e.KeyCode -eq [System.Windows.Forms.Keys]::F1) { Show-HelpDialog; $e.Handled = $true; $e.SuppressKeyPress = $true }
    elseif ($e.KeyCode -eq [System.Windows.Forms.Keys]::Escape) {
        if ($script:Notice.Kind) { Hide-Notice; $e.Handled = $true; $e.SuppressKeyPress = $true }
    }
})

$script:form.Add_FormClosing({
    param($sender, $e)
    if ($script:UiPreview -ne '') { return }
    try { Release-DeviceReset } catch { }
    if ($script:cboCom.SelectedItem) { $script:Settings.ComPort = [regex]::Match([string]$script:cboCom.SelectedItem, '^COM\d+').Value }
    $script:Settings.BaudRate       = ($script:cboBaud.Text -replace '\s', '')
    $script:Settings.TeraTermExe    = $script:txtExe.Text.Trim()
    $script:Settings.IniTemplate    = $script:txtIni.Text.Trim()
    $script:Settings.LogDir         = $script:txtLogDir.Text.Trim()
    $script:Settings.LogNamePattern = $script:txtPattern.Text.Trim()
    $script:Settings.LogTimestamp   = [bool]$script:chkTimestamp.Checked
    $script:Settings.AutoLogOnOpen  = [bool]$script:chkAutoLog.Checked
    $script:Settings.ResetHold      = [bool]$script:HoldMode
    Save-WindowGeometry
    Save-Settings
    $s = $script:Session
    if ($s.Proc -and (-not $s.Proc.HasExited)) {
        $action = [string]$script:Settings.OnCloseAction
        if ($action -eq 'ask') {
            $r = [System.Windows.Forms.MessageBox]::Show('Tera Term is still running. Close it too?', $AppTitle, 'YesNoCancel', 'Question')
            if ($r -eq [System.Windows.Forms.DialogResult]::Cancel) { $e.Cancel = $true; return }
            if ($r -eq [System.Windows.Forms.DialogResult]::Yes) { $action = 'close' } else { $action = 'keep' }
        }
        if ($action -eq 'close') { Close-Session }
        else {
            # Tera Term stays open, but the RESET line must not stay asserted.
            try { [void](Send-TtlCommand -Cmd 'SETRTS' -Arg1 '0') } catch { }
        }
    }
})

$script:UpdateTimer = New-Object System.Windows.Forms.Timer
$script:UpdateTimer.Interval = 500
$script:UpdateTimer.Add_Tick({ Timer-Tick })

# ------------------------------------------------------- app icon / shortcut
try {
    if (-not (Test-Path -LiteralPath $IcoPath)) {
        $icoBmp = New-Object System.Drawing.Bitmap 32, 32
        $gIco = [System.Drawing.Graphics]::FromImage($icoBmp)
        $gIco.SmoothingMode = 'AntiAlias'
        $gIco.Clear([System.Drawing.Color]::FromArgb(15, 23, 42))
        $gIco.FillRectangle((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(59, 130, 246))), 4, 5, 24, 4)
        $gIco.DrawString('>_', (New-Object System.Drawing.Font('Consolas', 11, [System.Drawing.FontStyle]::Bold)), (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(74, 222, 128))), 1, 9)
        $gIco.Dispose()
        $ico = [System.Drawing.Icon]::FromHandle($icoBmp.GetHicon())
        $fsIco = [System.IO.File]::Create($IcoPath)
        $ico.Save($fsIco)
        $fsIco.Close()
    }
    $script:form.Icon = [System.Drawing.Icon]::new($IcoPath)
} catch { }

try {
    $lnkPath = Join-Path $ScriptRoot 'TeraTerm Console.lnk'
    if (-not (Test-Path -LiteralPath $lnkPath)) {
        $wshell = New-Object -ComObject WScript.Shell
        $lnk = $wshell.CreateShortcut($lnkPath)
        $lnk.TargetPath = 'wscript.exe'
        $lnk.Arguments = '"' + (Join-Path $ScriptRoot 'TeraTermConsole.vbs') + '"'
        $lnk.WorkingDirectory = $ScriptRoot
        $lnk.IconLocation = ($IcoPath + ',0')
        $lnk.Description = 'TeraTerm Console - esp_wifi_template'
        $lnk.Save()
    }
} catch { }

# -------------------------------------------------------------- preview mode
function Set-DemoState {
    $script:Demo = $true
    $script:cboCom.Items.Clear()
    [void]$script:cboCom.Items.Add('COM10  -  USB-SERIAL CH340 (COM10)')
    [void]$script:cboCom.Items.Add('COM7  -  Silicon Labs CP210x (COM7)')
    $script:cboCom.SelectedIndex = 0
    $script:cboBaud.Text = '115200'
    $script:chkAutoLog.Checked = $true
    $script:chkTimestamp.Checked = $true
    $script:txtExe.Text = 'C:\Program Files (x86)\teraterm\ttermpro.exe'
    $script:txtIni.Text = $DefaultIniTemplate
    $script:txtLogDir.Text = (Join-Path $BuildDir 'logs')
    $script:txtPattern.Text = 'esp_wifi_%Y%m%d_%H%M%S.log'

    $s = $script:Session
    $s.Com = 'COM10'
    $s.Baud = '115200'
    $s.Logging = $true
    $s.Link = 2
    $demoLog = Join-Path $BuildDir 'logs\esp_wifi_20260214_093512.log'
    try {
        $demoLines = New-Object System.Collections.ArrayList
        $t0 = Get-Date
        for ($i = 0; $i -lt 60; $i++) {
            $stamp = $t0.AddSeconds(-180 + ($i * 3)).ToString('yyyy-MM-dd HH:mm:ss.fff')
            [void]$demoLines.Add($stamp + ' [INFO ] esp_wifi_template  heartbeat ' + ($i + 1).ToString('000') + '   rssi=-' + (48 + ($i % 14)) + 'dBm  heap=' + (118400 - ($i * 211)) + 'B  uptime=' + ($i * 3) + 's')
        }
        [void]$demoLines.Add($t0.ToString('yyyy-MM-dd HH:mm:ss.fff') + ' [DEBUG] modbus: read holding 0x0000..0x0009 ok (9 regs)')
        [System.IO.File]::WriteAllLines($demoLog, $demoLines.ToArray())
    } catch { }
    $s.LogPath = $demoLog
    $s.StartedAt = (Get-Date).AddSeconds(-754)

    $script:lstEvents.Items.Clear()
    $script:EventKinds.Clear()
    Add-Event -Text 'TeraTerm Console ready' -Kind 'info'
    Add-Event -Text 'Port scan: 2 port(s) in 412 ms' -Kind 'dim'
    Add-Event -Text 'Opening Tera Term on COM10 @ 115200' -Kind 'info'
    Add-Event -Text 'Watcher ready' -Kind 'ok'
    Add-Event -Text 'Logging to esp_wifi_20260214_093512.log' -Kind 'ok'
    Add-Event -Text 'Device reset asserted (RESET held LOW)' -Kind 'warn'
    Add-Event -Text 'Device reset released (chip running)' -Kind 'ok'
    Show-Notice -Text 'Logging to esp_wifi_20260214_093512.log' -Kind 'ok'
}

function Invoke-Preview {
    param([string]$Path)
    if ($script:Demo) {
        Set-DemoState
        foreach ($ctl in @($script:txtExe, $script:txtIni, $script:txtLogDir, $script:txtPattern, $script:cboCom, $script:cboBaud)) {
            try { [void]$ctl.Focus(); [System.Windows.Forms.Application]::DoEvents() } catch { Write-Host ('focus test failed: ' + $_.Exception.Message) }
        }
        try { [void]$script:lstEvents.Focus() } catch { }
    }
    $script:form.TopMost = $true
    $script:form.Show()
    $script:form.Activate()
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 1400
    [System.Windows.Forms.Application]::DoEvents()
    $script:form.Refresh()
    [System.Windows.Forms.Application]::DoEvents()
    $w = $script:form.Width
    $h = $script:form.Height
    if ($script:Demo) {
        Set-Busy $true 'Waiting for the watcher macro'
        $sw2 = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw2.ElapsedMilliseconds -lt 700) { [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 40 }
        try {
            $bmpB = New-Object System.Drawing.Bitmap($w, $h)
            $script:form.DrawToBitmap($bmpB, (New-Object System.Drawing.Rectangle(0, 0, $w, $h)))
            $pathB = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($Path), [System.IO.Path]::GetFileNameWithoutExtension($Path) + '.busy.png')
            $bmpB.Save($pathB, [System.Drawing.Imaging.ImageFormat]::Png)
            $bmpB.Dispose()
            Write-Host ('busy capture  : ' + $pathB)
        } catch { Write-Host ('busy capture failed: ' + $_.Exception.Message) }
        Set-Busy $false
        $sw2 = [System.Diagnostics.Stopwatch]::StartNew()
        while ($sw2.ElapsedMilliseconds -lt 500) { [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 40 }
    }
    try {
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $script:form.DrawToBitmap($bmp, (New-Object System.Drawing.Rectangle(0, 0, $w, $h)))
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host ('preview drawn : ' + $Path)
    } catch { Write-Host ('preview draw failed: ' + $_.Exception.Message) }
    try {
        $bmp2 = New-Object System.Drawing.Bitmap($w, $h)
        $g2 = [System.Drawing.Graphics]::FromImage($bmp2)
        $src = $script:form.PointToScreen((New-Object System.Drawing.Point(0, 0)))
        $g2.CopyFromScreen($src, (New-Object System.Drawing.Point(0, 0)), (New-Object System.Drawing.Size($w, $h)))
        $g2.Dispose()
        $alt = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($Path), [System.IO.Path]::GetFileNameWithoutExtension($Path) + '.screen.png')
        $bmp2.Save($alt, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp2.Dispose()
        Write-Host ('screen capture: ' + $alt)
    } catch { Write-Host ('screen capture failed: ' + $_.Exception.Message) }
    if ($script:Demo) {
        $script:form.ClientSize = (New-Object System.Drawing.Size(1000, 820))
        Invoke-Layout
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 500
        [System.Windows.Forms.Application]::DoEvents()
        try {
            $w2 = $script:form.Width; $h2 = $script:form.Height
            $bmp3 = New-Object System.Drawing.Bitmap($w2, $h2)
            $script:form.DrawToBitmap($bmp3, (New-Object System.Drawing.Rectangle(0, 0, $w2, $h2)))
            $alt2 = [System.IO.Path]::Combine([System.IO.Path]::GetDirectoryName($Path), [System.IO.Path]::GetFileNameWithoutExtension($Path) + '.wide.png')
            $bmp3.Save($alt2, [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp3.Dispose()
            Write-Host ('wide capture  : ' + $alt2)
        } catch { Write-Host ('wide capture failed: ' + $_.Exception.Message) }
    }
    $script:form.Close()
}

# ======================================================================== main
Load-Settings

foreach ($b in @('9600', '19200', '38400', '57600', '115200', '230400', '460800', '921600')) { [void]$script:cboBaud.Items.Add($b) }
$script:cboBaud.Text         = [string]$script:Settings.BaudRate
$script:txtExe.Text          = [string]$script:Settings.TeraTermExe
$script:txtIni.Text          = [string]$script:Settings.IniTemplate
$script:txtLogDir.Text       = [string]$script:Settings.LogDir
$script:txtPattern.Text      = [string]$script:Settings.LogNamePattern
$script:chkTimestamp.Checked = [bool]$script:Settings.LogTimestamp
$script:chkAutoLog.Checked   = [bool]$script:Settings.AutoLogOnOpen
$script:HoldMode = [bool]$script:Settings.ResetHold
Update-HoldVisual

Restore-WindowGeometry

$minH = $script:M.HeaderH + $script:M.AccentH + $script:M.RowGap + $script:M.ConnH + $script:M.RowGap + $script:M.TtH + $script:M.RowGap + $script:M.ActionH + $script:M.RowGap + $script:M.NoticeH + $script:M.RowGap + $script:M.EventsMin + $script:M.RowGap + $script:M.StatusH + $script:M.Pad
$script:form.MinimumSize = (New-Object System.Drawing.Size((Px 716), ($minH + (Px 48))))

$script:FormReady = $true
Invoke-Layout
$script:UpdateTimer.Start()
Update-Ui

if ($UiPreview -ne '') {
    Invoke-Preview $UiPreview
    exit 0
}

[void]$script:form.ShowDialog()
$script:UpdateTimer.Stop()
Write-UiLog 'UI closed.'
