param([string]$ProcessName = "buraco", [string]$Keys = "{ENTER}", [int]$WaitMs = 600)
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class K6{
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,UIntPtr e);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int L,T,R,B;}
}
'@
Add-Type -AssemblyName System.Windows.Forms
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "no window for $ProcessName" }
$h = [IntPtr]$p.MainWindowHandle

# Click on the client area so the window really owns the keyboard focus,
# then force the foreground hand-off (plain SetForegroundWindow is refused
# when another process owns the foreground).
$r = New-Object K6+RECT
[K6]::GetWindowRect($h, [ref]$r) | Out-Null
[K6]::ShowWindow($h, 9) | Out-Null
$fg = [K6]::GetForegroundWindow()
$fgPid = 0
$fgThread = [K6]::GetWindowThreadProcessId($fg, [ref]$fgPid)
$myThread = [K6]::GetCurrentThreadId()
[K6]::AttachThreadInput($myThread, $fgThread, $true) | Out-Null
[K6]::BringWindowToTop($h) | Out-Null
[K6]::SetForegroundWindow($h) | Out-Null
[K6]::SetFocus($h) | Out-Null
[K6]::AttachThreadInput($myThread, $fgThread, $false) | Out-Null
Start-Sleep -Milliseconds 250

$cx = [int](($r.L + $r.R) / 2); $cy = [int](($r.T + $r.B) / 2)
[K6]::SetCursorPos($cx, $cy) | Out-Null
Start-Sleep -Milliseconds 100
[K6]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
[K6]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 250

[System.Windows.Forms.SendKeys]::SendWait($Keys)
Start-Sleep -Milliseconds $WaitMs
"sent $Keys to $ProcessName (fg=$fg)"
