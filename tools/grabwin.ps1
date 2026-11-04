param([string]$ProcessName = "buraco", [string]$Out = "shots\grab.png", [int]$WaitMs = 400)
Add-Type -AssemblyName System.Drawing
if (-not ("C5" -as [type])) {
Add-Type @'
using System;using System.Runtime.InteropServices;
public class C5{
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr hdc,uint f);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int L,T,R,B;}
}
'@
}
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "no window for $ProcessName" }
$h = [IntPtr]$p.MainWindowHandle
[C5]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds $WaitMs
$r = New-Object C5+RECT
[C5]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[C5]::PrintWindow($h, $hdc, 0) | Out-Null
$g.ReleaseHdc($hdc); $g.Dispose()
$full = Join-Path (Join-Path $PSScriptRoot "..") $Out
$bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"$full  ${w}x${ht}"
