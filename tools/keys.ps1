param([string]$Keys = "{ENTER}", [int]$WaitMs = 1200)
Add-Type @'
using System;using System.Runtime.InteropServices;
public class K4{
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
}
'@
Add-Type -AssemblyName System.Windows.Forms
$pid0 = [int](Get-Content (Join-Path $PSScriptRoot "..\work\dosbox.pid"))
$p = Get-Process -Id $pid0
$h = [IntPtr]$p.MainWindowHandle
[K4]::ShowWindow($h, 9) | Out-Null
[K4]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
[System.Windows.Forms.SendKeys]::SendWait($Keys)
Start-Sleep -Milliseconds $WaitMs
"sent $Keys"
