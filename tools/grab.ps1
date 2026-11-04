param([string]$Out = "shots\grab.png")
Add-Type -AssemblyName System.Drawing
if (-not ("C3" -as [type])) {
Add-Type @'
using System;using System.Runtime.InteropServices;
public class C3{
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr hdc,uint f);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int L,T,R,B;}
}
'@
}
$pid0 = [int](Get-Content (Join-Path $PSScriptRoot "..\work\dosbox.pid"))
$p = Get-Process -Id $pid0
$h = [IntPtr]$p.MainWindowHandle
[C3]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300
$r = New-Object C3+RECT
[C3]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[C3]::PrintWindow($h, $hdc, 0) | Out-Null
$g.ReleaseHdc($hdc); $g.Dispose()
$full = Join-Path (Join-Path $PSScriptRoot "..") $Out
$bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"$full  ${w}x${ht}"
