' ============================================================================
'  TeraTerm Console - esp_wifi_template    hidden launcher (no console window)
'  Started by TeraTermConsole.cmd: runs TeraTermConsole.ps1 with a hidden
'  PowerShell window and returns immediately.
' ============================================================================
Option Explicit

Dim fso, sh, here, ps1, cmd

Set fso = CreateObject("Scripting.FileSystemObject")
Set sh  = CreateObject("WScript.Shell")

here = fso.GetParentFolderName(WScript.ScriptFullName)
ps1  = fso.BuildPath(here, "TeraTermConsole.ps1")

If Not fso.FileExists(ps1) Then
    MsgBox "TeraTermConsole.ps1 was not found next to this launcher:" & vbCrLf & vbCrLf & ps1, vbCritical, "TeraTerm Console"
    WScript.Quit 1
End If

cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & ps1 & """"

On Error Resume Next
sh.Run cmd, 0, False
If Err.Number <> 0 Then
    MsgBox "Windows PowerShell could not be started:" & vbCrLf & vbCrLf & Err.Description, vbCritical, "TeraTerm Console"
    WScript.Quit 1
End If
On Error GoTo 0
