param([string]$BuildDirectory=(Join-Path $PSScriptRoot '../cmake-build-mcss'))
$ErrorActionPreference='Stop'
$BuildDirectory=(Resolve-Path $BuildDirectory).Path
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class RegressionUI {
 [StructLayout(LayoutKind.Sequential)] public struct RECT {public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)] public struct POINT {public int X,Y;}
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern IntPtr GetLastActivePopup(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h,ref POINT p);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr h,IntPtr r,IntPtr region,uint flags);
 [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
 [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h,IntPtr dc);
 [StructLayout(LayoutKind.Sequential)] public struct SIZE {public int Width,Height;}
 [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc,IntPtr value);
 [DllImport("gdi32.dll",CharSet=CharSet.Unicode)] static extern bool GetTextExtentPoint32(IntPtr dc,string text,int length,out SIZE size);
 public static bool ButtonFits(IntPtr h){RECT r;GetWindowRect(h,out r);var dc=GetDC(h);var old=SelectObject(dc,SendMessage(h,0x31,IntPtr.Zero,IntPtr.Zero));SIZE size;var text=Text(h);GetTextExtentPoint32(dc,text,text.Length,out size);SelectObject(dc,old);ReleaseDC(h,dc);return r.Right-r.Left>=size.Width+36;}
 [DllImport("gdi32.dll")] public static extern uint GetPixel(IntPtr dc,int x,int y);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int m,IntPtr w,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,string value);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,StringBuilder value);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder b,int count);
 [DllImport("kernel32.dll")] static extern IntPtr GlobalAlloc(uint flags,UIntPtr size);
 [DllImport("kernel32.dll")] static extern IntPtr GlobalLock(IntPtr h);
 [DllImport("kernel32.dll")] static extern bool GlobalUnlock(IntPtr h);
 public static void Drop(IntPtr h,string[] paths){byte[] names=Encoding.Unicode.GetBytes(string.Join("\0",paths)+"\0\0");var memory=GlobalAlloc(0x42,(UIntPtr)(20+names.Length));var data=GlobalLock(memory);byte[] bytes=new byte[20+names.Length];BitConverter.GetBytes(20).CopyTo(bytes,0);BitConverter.GetBytes(1).CopyTo(bytes,16);names.CopyTo(bytes,20);Marshal.Copy(bytes,0,data,bytes.Length);GlobalUnlock(memory);PostMessage(h,0x233,memory,IntPtr.Zero);}
 public static string Text(IntPtr h){var text=new StringBuilder(32768);SendMessage(h,0xD,(IntPtr)text.Capacity,text);return text.ToString();}
 public static string Geometry(IntPtr h){RECT w,c;GetWindowRect(h,out w);GetClientRect(h,out c);POINT p=new POINT();ClientToScreen(h,ref p);return "outer="+(w.Bottom-w.Top)+" client="+c.Bottom+" top="+(p.Y-w.Top)+" bottom="+(w.Bottom-p.Y-c.Bottom);}
 public static bool Centered(IntPtr h){RECT window,client;GetWindowRect(h,out window);GetClientRect(h,out client);POINT origin=new POINT();ClientToScreen(h,ref origin);int top=origin.Y-window.Top,bottom=window.Bottom-origin.Y-client.Bottom;return top>0&&Math.Abs(top-bottom)<=1&&client.Bottom>=12;}
 public static uint Pixel(IntPtr h,int x,int y){var dc=GetDC(h);try{return GetPixel(dc,x,y);}finally{ReleaseDC(h,dc);}}
}
'@
$root=Join-Path ([IO.Path]::GetTempPath()) ('wrapper-ui-'+[guid]::NewGuid().ToString('N'))
$env:SERVERDESK_CACHE_DIR=Join-Path $root 'cache'  # keep the plugin cache inside the fixture
$server=Join-Path $root 'server';$sources=Join-Path $root 'sources'
[void](New-Item -ItemType Directory $server,$sources -Force)
Copy-Item (Join-Path $BuildDirectory 'MinecraftServerWrapper.exe') $root
Copy-Item (Join-Path $BuildDirectory 'wrapper_mock_server.exe') $root
Copy-Item (Join-Path $BuildDirectory 'assets') $root -Recurse
function Jar($path,$entry,$content){$archive=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create);try{$item=$archive.CreateEntry($entry);$writer=[IO.StreamWriter]::new($item.Open());try{$writer.Write($content)}finally{$writer.Dispose()}}finally{$archive.Dispose()}}
$jar=Join-Path $server 'server.jar';Jar $jar 'META-INF/MANIFEST.MF' "Main-Class: mock.Main`n"
[IO.File]::WriteAllText((Join-Path $root 'wrapper.ini'),@"
[Workspace]
Count=1
Theme=0
[Profile0]
Name=UI regression fixture
Java=$(Join-Path $root 'wrapper_mock_server.exe')
Jar=$jar
EulaAccepted=1
"@,[Text.Encoding]::Unicode)
$p=$null;$h=[IntPtr]::Zero
function Wait-For($test,$label){$end=[DateTime]::UtcNow.AddSeconds(20);while([DateTime]::UtcNow -lt $end){if(&$test){Write-Output "PASS: $label";return};Start-Sleep -Milliseconds 50};throw "Timeout: $label"}
function Click($id){[void][RegressionUI]::PostMessage($h,0x111,[IntPtr]$id,[IntPtr]::Zero)}
function Capture($window,$name){$r=New-Object RegressionUI+RECT;[void][RegressionUI]::GetWindowRect($window,[ref]$r);$bitmap=[Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top);$g=[Drawing.Graphics]::FromImage($bitmap);$dc=$g.GetHdc();try{[void][RegressionUI]::PrintWindow($window,$dc,2)}finally{$g.ReleaseHdc($dc)};$folder=Join-Path $BuildDirectory 'screenshots';[void](New-Item -ItemType Directory $folder -Force);$bitmap.Save((Join-Path $folder ($name+'.png')),[Drawing.Imaging.ImageFormat]::Png);$g.Dispose();$bitmap.Dispose()}
try {
 $p=Start-Process (Join-Path $root 'MinecraftServerWrapper.exe') -PassThru
 Wait-For {$p.Refresh();$p.MainWindowHandle -ne 0} 'UI fixture starts'
 $h=$p.MainWindowHandle
 [void][RegressionUI]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,0x13)
 Click 305;Start-Sleep -Milliseconds 300
 foreach($id in @(392,393,580)){if(-not [RegressionUI]::Centered([RegressionUI]::GetDlgItem($h,$id))){throw "Input $id not vertically centered"}}
 Write-Output 'PASS: scheduler interval, command, and preset text vertically centered'
 Capture $h 'neutral-scheduler'
 Click 306;Click 551;Start-Sleep -Milliseconds 200
 Wait-For {[RegressionUI]::IsWindowVisible([RegressionUI]::GetDlgItem($h,104)) -and [RegressionUI]::Centered([RegressionUI]::GetDlgItem($h,104))} 'launch fields finish layout'
 foreach($id in @(100,102,104)){if(-not [RegressionUI]::Centered([RegressionUI]::GetDlgItem($h,$id))){throw ("Launch input $id not centered: "+[RegressionUI]::Geometry([RegressionUI]::GetDlgItem($h,$id)))}}
 Click 552
 Wait-For {[RegressionUI]::IsWindowVisible([RegressionUI]::GetDlgItem($h,568))} 'server properties checkbox ready'
 $check=[RegressionUI]::GetDlgItem($h,568)
 [void][RegressionUI]::SendMessage($check,0xF1,[IntPtr]::Zero,[IntPtr]::Zero)
 [void][RegressionUI]::RedrawWindow($check,[IntPtr]::Zero,[IntPtr]::Zero,0x101)
 [void][RegressionUI]::SetForegroundWindow($h)

 $pt=New-Object RegressionUI+POINT;$pt.X=8;$pt.Y=12;[void][RegressionUI]::ClientToScreen($check,[ref]$pt);[void][RegressionUI]::SetCursorPos($pt.X,$pt.Y)
 Start-Sleep -Milliseconds 60
 [RegressionUI]::mouse_event(2,0,0,0,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 16
 foreach($delay in @(0,16,60)){if($delay){Start-Sleep -Milliseconds $delay};$color=[RegressionUI]::Pixel($check,5,10);if($color -ne (52+52*256+52*65536) -and $color -ne (96+205*256+255*65536)){throw "Native checkbox flashed while pressed: $color"}}
 Capture $h 'neutral-checkbox-held'
 [RegressionUI]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
 Start-Sleep -Milliseconds 50
 [void][RegressionUI]::SendMessage($check,0xF1,[IntPtr]::Zero,[IntPtr]::Zero)
 [void][RegressionUI]::SendMessage($check,0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-For {[RegressionUI]::SendMessage($check,0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1} 'checkbox click preserves native state'
 [void][RegressionUI]::SendMessage($check,0x100,[IntPtr]32,[IntPtr]::Zero)
 [void][RegressionUI]::SendMessage($check,0x101,[IntPtr]32,[IntPtr]::Zero)
 Wait-For {[RegressionUI]::SendMessage($check,0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0} 'checkbox Space toggles without changing control semantics'
 Click 800
 Wait-For {$popup=[RegressionUI]::GetLastActivePopup($h);$popup -ne $h -and [RegressionUI]::GetDlgItem($popup,109) -ne [IntPtr]::Zero} 'advanced form opens'
 $popup=[RegressionUI]::GetLastActivePopup($h)
 if(-not [RegressionUI]::Centered([RegressionUI]::GetDlgItem($popup,103))){throw 'Advanced form text not centered'}
 Capture $popup 'neutral-advanced'
 [void][RegressionUI]::PostMessage($popup,0x111,[IntPtr]2,[IntPtr]::Zero)
 Wait-For {[RegressionUI]::IsWindowEnabled($h)} 'advanced form closes'
 Click 302;Start-Sleep -Milliseconds 200
 Wait-For {[RegressionUI]::IsWindowVisible([RegressionUI]::GetDlgItem($h,351))} 'plugins toolbar ready'
 foreach($id in @(351,352,353,354)){if(-not [RegressionUI]::ButtonFits([RegressionUI]::GetDlgItem($h,$id))){throw "Plugin toolbar label clipped: $id"}}
 Write-Output 'PASS: plugin toolbar labels have room for text, icon, and padding'
 $a=Join-Path $sources 'drop-a.jar';$b=Join-Path $sources 'drop-b.jar';$c=Join-Path $sources 'drop-c.jar'
 foreach($file in @($a,$b,$c)){Jar $file 'plugin.yml' "name: DroppedPlugin`nversion: '1.0'`n"}
 [RegressionUI]::Drop([RegressionUI]::GetDlgItem($h,350),[string[]]@($a,$b))
 Wait-For {(Test-Path (Join-Path $server 'plugins/drop-a.jar')) -and (Test-Path (Join-Path $server 'plugins/drop-b.jar'))} 'multiple JARs dropped directly on plugin list are imported'
 [RegressionUI]::Drop($h,[string[]]@($c))
 Wait-For {Test-Path (Join-Path $server 'plugins/drop-c.jar')} 'JAR dropped on plugins page imports into plugins folder'
 if([RegressionUI]::Text([RegressionUI]::GetDlgItem($h,102)) -ne $jar -or -not [RegressionUI]::IsWindowVisible([RegressionUI]::GetDlgItem($h,350))){throw 'Plugin drop changed server JAR or page'}
 Write-Output 'PASS: plugin drop preserves server JAR and selected page'
 Wait-For {[RegressionUI]::SendMessage([RegressionUI]::GetDlgItem($h,350),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 3} 'plugin list finishes loading dropped JARs'
 Capture $h 'neutral-plugins'
 Click 107
 Wait-For {[RegressionUI]::IsWindowEnabled([RegressionUI]::GetDlgItem($h,109))} 'mock server starts'
 $d=Join-Path $sources 'blocked.jar';Jar $d 'plugin.yml' "name: Blocked`n"
 [RegressionUI]::Drop([RegressionUI]::GetDlgItem($h,350),[string[]]@($d))
 Wait-For {$popup=[RegressionUI]::GetLastActivePopup($h);$popup -ne $h -and -not [RegressionUI]::IsWindowEnabled($h)} 'running server rejects plugin file changes'
 if(Test-Path (Join-Path $server 'plugins/blocked.jar')){throw 'Running server modified plugins'}
 [void][RegressionUI]::PostMessage([RegressionUI]::GetLastActivePopup($h),0x111,[IntPtr]1,[IntPtr]::Zero)
 Wait-For {[RegressionUI]::IsWindowEnabled($h)} 'error dismissed'
 $p.CloseMainWindow()|Out-Null;Wait-For {$p.Refresh();$p.HasExited} 'UI regression fixture closes cleanly'
} finally {
 [RegressionUI]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
 if($p){$p.Refresh();if(-not $p.HasExited){Stop-Process -Id $p.Id -Force;$p.WaitForExit()}}
 Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
}
