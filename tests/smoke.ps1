param(
 [string]$BuildDirectory = (Join-Path (Split-Path $PSScriptRoot) 'cmake-build-mcss'),
 [switch]$CaptureScreenshots,
 [switch]$KeepArtifacts
)
$ErrorActionPreference = 'Stop'
$BuildDirectory = (Resolve-Path $BuildDirectory).Path
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class UI {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("kernel32.dll")] static extern IntPtr GlobalAlloc(uint f,UIntPtr n);
 [DllImport("kernel32.dll")] static extern IntPtr GlobalLock(IntPtr h);
 [DllImport("kernel32.dll")] static extern bool GlobalUnlock(IntPtr h);
 [DllImport("kernel32.dll")] static extern IntPtr GlobalFree(IntPtr h);
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a,bool i,int p);
 [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr a,UIntPtr n,uint t,uint protection);
 [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr p,IntPtr a,UIntPtr n,uint t);
 [DllImport("kernel32.dll")] static extern bool WriteProcessMemory(IntPtr p,IntPtr a,byte[] b,int n,out IntPtr written);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p,IntPtr a,byte[] b,int n,out IntPtr read);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr p);
 public static bool DropFile(IntPtr window,string path) {
  byte[] bytes=Encoding.Unicode.GetBytes(path+"\0\0"); IntPtr h=GlobalAlloc(0x42,(UIntPtr)(20+bytes.Length));
  IntPtr p=GlobalLock(h); Marshal.WriteInt32(p,0,20); Marshal.WriteInt32(p,16,1); Marshal.Copy(bytes,0,IntPtr.Add(p,20),bytes.Length); GlobalUnlock(h);
  if (PostMessage(window,0x233,h,IntPtr.Zero)) return true; GlobalFree(h); return false;
 }
 [DllImport("user32.dll",EntryPoint="SendMessageW")] static extern IntPtr SendMessageRect(IntPtr window,int message,IntPtr w,ref RECT rect);
 public static RECT ListItemRect(IntPtr window,int pid,int index) { RECT rect=new RECT(); SendMessageRect(window,0x198,(IntPtr)index,ref rect); return rect; }
 public static string ListCell(IntPtr window,int pid,int column) {
  IntPtr process=OpenProcess(0x38,false,pid), memory=VirtualAllocEx(process,IntPtr.Zero,(UIntPtr)2048,0x3000,4);
  try {
   byte[] item=new byte[88]; BitConverter.GetBytes(column).CopyTo(item,8);
   BitConverter.GetBytes(IntPtr.Add(memory,128).ToInt64()).CopyTo(item,24); BitConverter.GetBytes(400).CopyTo(item,32); IntPtr n;
   WriteProcessMemory(process,memory,item,item.Length,out n); SendMessage(window,0x1073,IntPtr.Zero,memory);
   byte[] value=new byte[800]; ReadProcessMemory(process,IntPtr.Add(memory,128),value,value.Length,out n);
   return Encoding.Unicode.GetString(value).TrimEnd('\0');
  } finally { VirtualFreeEx(process,memory,UIntPtr.Zero,0x8000); CloseHandle(process); }
 }
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd,out RECT rect);
 public static bool LastLogLineVisible(IntPtr window,int pid) {
  IntPtr process=OpenProcess(0x38,false,pid), memory=VirtualAllocEx(process,IntPtr.Zero,(UIntPtr)512,0x3000,4);
  try {
   byte[] find=new byte[24]; BitConverter.GetBytes(-1).CopyTo(find,4); IntPtr n;
   BitConverter.GetBytes(IntPtr.Add(memory,128).ToInt64()).CopyTo(find,8); byte[] search=Encoding.Unicode.GetBytes("SCROLL-LINE-79\0");
   WriteProcessMemory(process,IntPtr.Add(memory,128),search,search.Length,out n); WriteProcessMemory(process,memory,find,find.Length,out n);
   int index=SendMessage(window,0x47C,(IntPtr)1,memory).ToInt32(); if (index<0) return false;
   SendMessage(window,0x426,IntPtr.Add(memory,256),(IntPtr)index);
   byte[] point=new byte[8]; ReadProcessMemory(process,IntPtr.Add(memory,256),point,point.Length,out n);
   int y=BitConverter.ToInt32(point,4); RECT rect; GetClientRect(window,out rect); return y>=0 && y+14<=rect.Bottom;
  } finally { VirtualFreeEx(process,memory,UIntPtr.Zero,0x8000); CloseHandle(process); }
 }
 public static string ColorSearch="RGB-COLOR";
 public static int LastPosition;
 public static int ColorAt(IntPtr window,int pid,int position) {
  IntPtr process=OpenProcess(0x38,false,pid), memory=VirtualAllocEx(process,IntPtr.Zero,(UIntPtr)512,0x3000,4);
  try {
   byte[] find=new byte[24]; BitConverter.GetBytes(-1).CopyTo(find,4); IntPtr n;
   BitConverter.GetBytes(IntPtr.Add(memory,128).ToInt64()).CopyTo(find,8);
   byte[] search=Encoding.Unicode.GetBytes(ColorSearch+"\0");
   WriteProcessMemory(process,IntPtr.Add(memory,128),search,search.Length,out n);
   WriteProcessMemory(process,memory,find,find.Length,out n);
   position=SendMessage(window,0x47C,(IntPtr)1,memory).ToInt32();
   LastPosition=position;
   if (position<0) throw new Exception("Rich Edit search failed");
   byte[] value=new byte[116]; BitConverter.GetBytes(116).CopyTo(value,0);
   if (!WriteProcessMemory(process,memory,value,value.Length,out n)) throw new Exception("WriteProcessMemory");
   byte[] range=new byte[8]; BitConverter.GetBytes(position).CopyTo(range,0); BitConverter.GetBytes(position+3).CopyTo(range,4);
   WriteProcessMemory(process,IntPtr.Add(memory,256),range,range.Length,out n);
   SendMessage(window,0x437,IntPtr.Zero,IntPtr.Add(memory,256)); SendMessage(window,0x43A,(IntPtr)1,memory);
   if (!ReadProcessMemory(process,memory,value,value.Length,out n)) throw new Exception("ReadProcessMemory");
   return BitConverter.ToInt32(value,20);
  } finally { VirtualFreeEx(process,memory,UIntPtr.Zero,0x8000); CloseHandle(process); }
 }
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr w,int index);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr w,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr w,string s);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr w,StringBuilder b,int n);
 [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr w,int m,IntPtr a,IntPtr b);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr w,int m,IntPtr a,StringBuilder b);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr w,int m,IntPtr a,string b);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w,int m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr w);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr w,int s);
 [DllImport("user32.dll")] public static extern IntPtr GetLastActivePopup(IntPtr w);
}
'@
$cache = Get-Content (Join-Path $BuildDirectory 'CMakeCache.txt')
$compilerLine = $cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:[^=]*=' } | Select-Object -First 1
if (-not $compilerLine) { throw 'No CMake compiler found; build the project first.' }
$compiler = ($compilerLine -split '=',2)[1]
$env:PATH = (Split-Path $compiler) + ';' + $env:PATH
$root = Join-Path $BuildDirectory ('smoke-' + [guid]::NewGuid().ToString('N'))
# Keep the plugin metadata cache inside the fixture instead of the user's %LOCALAPPDATA%.
$env:SERVERDESK_CACHE_DIR = Join-Path $root 'cache'
[void](New-Item -ItemType Directory -Path $root)
$mock = Join-Path $root 'mock java.exe'
& $compiler -std=c++20 -municode -static (Join-Path $PSScriptRoot 'mock_server.cpp') -o $mock
if ($LASTEXITCODE -ne 0) { throw 'Failed to compile mock server.' }
Copy-Item (Join-Path $BuildDirectory 'MinecraftServerWrapper.exe') $root
Copy-Item (Join-Path $BuildDirectory 'assets') $root -Recurse
function New-ServerJar($path,$mainClass='mock.Main') {
 $archive=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create)
 $entry=$archive.CreateEntry('META-INF/MANIFEST.MF'); $writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
 $writer.Write("Manifest-Version: 1.0`nMain-Class: $mainClass`n`n"); $writer.Dispose(); $archive.Dispose()
}
New-ServerJar (Join-Path $root 'server test.jar')
Set-Content (Join-Path $root 'eula.txt') 'eula=true' -Encoding Ascii
function Read-Control($id) {
 $c=[IntPtr]::Zero
 if (-not $script:h) { return "<window not available>" }
 $c=[UI]::GetDlgItem($script:h,$id)
 $b=New-Object Text.StringBuilder ([UI]::SendMessage($c,0xE,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()+1)
 [void][UI]::SendMessage($c,0xD,[IntPtr]$b.Capacity,$b)
 return [regex]::Replace($b.ToString(), '\r?\n|\r', "`r`n")
}
function Put($id,$value) { [void][UI]::SendMessage([UI]::GetDlgItem($script:h,$id),0xC,[IntPtr]::Zero,[string]$value) }
function Click($id) { [void][UI]::PostMessage($script:h,0x111,[IntPtr]$id,[IntPtr]::Zero) }
function Capture($name) {
 if (-not $CaptureScreenshots) { return }
 [void][UI]::ShowWindow($h,4)
 Start-Sleep -Milliseconds 300
 $r=New-Object UI+RECT; if (-not [UI]::GetWindowRect($h,[ref]$r)) { $p.Refresh(); throw ("Capture lost GUI: exit="+$p.ExitCode) }
 $bitmap=New-Object Drawing.Bitmap ($r.Right-$r.Left),($r.Bottom-$r.Top)
 $graphics=[Drawing.Graphics]::FromImage($bitmap)
 $dc=$graphics.GetHdc()
 try { [void][UI]::PrintWindow($h,$dc,2) } finally { $graphics.ReleaseHdc($dc) }
 $folder=Join-Path $BuildDirectory 'screenshots'; [void](New-Item -ItemType Directory -Path $folder -Force)
 $bitmap.Save((Join-Path $folder ($name+'.png')),[Drawing.Imaging.ImageFormat]::Png)
 $graphics.Dispose(); $bitmap.Dispose()
 [void][UI]::ShowWindow($h,0)
}
function Read-Ini {
 try {
  $stream=[IO.File]::Open((Join-Path $root 'wrapper.ini'),[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
  $reader=New-Object IO.StreamReader $stream
  try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
 } catch { return '' }
}
function Player-Count { return [UI]::SendMessage([UI]::GetDlgItem($script:h,7001),0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() }
function Send-Command($value) { Put 112 $value; Click 113 }
function Wait-Until($check,$description) {
 $until=(Get-Date).AddSeconds(30)
 while (-not (& $check)) {
  if ($p) { $p.Refresh(); if ($p.HasExited) { throw ("GUI exited during "+$description+" code="+$p.ExitCode) } }
  if ((Get-Date) -gt $until) {
   Click 301; Start-Sleep -Milliseconds 300
   Capture 'timeout-state'
   throw ("Timeout: $description`nBackup path: "+(Read-Control 374)+"`n"+(Read-Control 114 | ForEach-Object { $_.Substring([Math]::Max(0,$_.Length-3000)) }))
  }
  Start-Sleep -Milliseconds 100
 }
 Write-Output "PASS: $description"
}
function Open-Groups {
 Click 595
 Wait-Until { $w=[UI]::GetLastActivePopup($h); $w -ne $h -and [UI]::GetDlgItem($w,720) -ne [IntPtr]::Zero } 'separate group manager opens'
 $script:manager=[UI]::GetLastActivePopup($h)
}
function Manager-Click($id) { [void][UI]::PostMessage($script:manager,0x111,[IntPtr]$id,[IntPtr]::Zero) }
function Manager-Name($name) { [void][UI]::SendMessage([UI]::GetDlgItem($script:manager,721),0xC,[IntPtr]::Zero,[string]$name) }
function Select-Group($name) {
 $list=[UI]::GetDlgItem($script:manager,720)
 $index=if ($name -eq '') { 0 } else { [UI]::SendMessage($list,0x1A2,[IntPtr](-1),[string]$name).ToInt32() }
 if ($index -lt 0) { throw ('Group missing: '+$name) }
 [void][UI]::SendMessage($list,0x186,[IntPtr]$index,[IntPtr]::Zero)
 [void][UI]::PostMessage($script:manager,0x111,[IntPtr](720+65536),[IntPtr]::Zero)
 Start-Sleep -Milliseconds 100
}
function Assign-Group($name,$members) {
 Open-Groups
 $list=[UI]::GetDlgItem($script:manager,720)
 if ([UI]::SendMessage($list,0x1A2,[IntPtr](-1),[string]$name).ToInt32() -lt 0) {
  Manager-Name $name; Manager-Click 723
  Wait-Until { [UI]::SendMessage($list,0x1A2,[IntPtr](-1),[string]$name).ToInt32() -ge 0 } 'group created in manager'
 }
 Select-Group $name
 $list=[UI]::GetDlgItem($script:manager,722)
 [void][UI]::SendMessage($list,0x185,[IntPtr]0,[IntPtr](-1))
 foreach ($member in $members) { [void][UI]::SendMessage($list,0x185,[IntPtr]1,[IntPtr]$member) }
 Manager-Click 726
 Wait-Until { (Read-Control 591) -eq $name } 'membership applied without restarting server'
 [void][UI]::SendMessage($script:manager,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
}
function Capture-Owned($target,$name) {
 $original=$script:h; $script:h=$target
 try { Capture $name } finally { $script:h=$original; [void][UI]::ShowWindow($target,4) }
}
function Measure-WrapperCpu($name) {
 $p.Refresh(); $before=$p.TotalProcessorTime.TotalSeconds; $watch=[Diagnostics.Stopwatch]::StartNew()
 Start-Sleep -Seconds 3
 $p.Refresh(); $oneCore=100*($p.TotalProcessorTime.TotalSeconds-$before)/$watch.Elapsed.TotalSeconds
 $total=$oneCore/[Environment]::ProcessorCount
 $script:performance[$name]=[Math]::Round($total,4)
 Write-Output ('CPU '+$name+': '+[Math]::Round($total,4)+'% of system ('+[Math]::Round($oneCore,4)+'% of one core)')
}
$script:performance=@{}
$backupRoot=$null
$launchWatch=[Diagnostics.Stopwatch]::StartNew()
$p=Start-Process (Join-Path $root 'MinecraftServerWrapper.exe') -PassThru
try {
 Wait-Until { $p.Refresh(); $p.MainWindowHandle -ne 0 } 'GUI launch without toolchain PATH'
 $script:performance["startup_seconds"]=[Math]::Round($launchWatch.Elapsed.TotalSeconds,3)
 $script:h=$p.MainWindowHandle
 [void][UI]::ShowWindow($h,0)
 if ([UI]::GetDlgItem($h,483) -ne [IntPtr]::Zero) { throw "Footer was not removed" }
 Measure-WrapperCpu 'idle_system_cpu_percent'
 Put 100 (Join-Path $root 'mock java.exe')
 Put 102 (Join-Path $root 'server test.jar')
 Put 104 '1024'
 Click 263
 Click 107
 Wait-Until { (Read-Control 114).Contains('READY') -and (Read-Control 114).Contains('STDERR') } 'stdout and stderr capture'
 $korean=[string][char]0xD55C+[char]0xAE00
 Wait-Until { (Read-Control 114).Contains($korean) } 'split UTF-8 Korean decoding'
 Send-Command 'colors'
 Wait-Until { (Read-Control 114).Contains('RGB-COLOR') } 'RGB log text displayed without escape codes'
 $position=(Read-Control 114).Replace("`r",'').IndexOf('RGB-COLOR')
 $actual=[UI]::ColorAt([UI]::GetDlgItem($h,114),$p.Id,$position)
 if ($actual -ne (12+200*256+80*65536)) { Capture 'console-rgb-failure'; throw ('RGB formatting not applied: actual='+$actual+' position='+[UI]::LastPosition) }
 Write-Output 'PASS: actual Rich Edit truecolor formatting'
 $beforeEscape=Read-Control 425
 [void][UI]::PostMessage([UI]::GetDlgItem($h,112),0x100,[IntPtr]27,[IntPtr]::Zero)
 [void][UI]::PostMessage([UI]::GetDlgItem($h,112),0x101,[IntPtr]27,[IntPtr]::Zero)
 Send-Command 'say after-escape'
 Wait-Until { (Read-Control 114).Contains('ACK:say after-escape') } 'Escape does not stop the server or close the app'
 if ((Read-Control 425) -ne $beforeEscape) { throw 'Escape changed the server process' }

 [UI]::ColorSearch='RESET-COLOR'
 if ([UI]::ColorAt([UI]::GetDlgItem($h,114),$p.Id,0) -ne (240+240*256+240*65536)) { throw 'Dark console default is not white' }
 [UI]::ColorSearch='RGB-COLOR'
 Write-Output 'PASS: dark console default is white after ANSI reset' 
 Measure-WrapperCpu 'running_mock_system_cpu_percent'
 $a=Get-Content (Join-Path $root 'args.txt') -Raw
 if (-not $a.Contains('-Xmx1024M') -or -not $a.Contains('server test.jar')) { throw 'Wrong arguments' }
 Write-Output 'PASS: memory, spaced paths and server working directory'
 Put 112 '/say hello'
 Click 113
 Wait-Until { (Read-Control 114).Contains('ACK:say hello') } 'command forwarding and slash removal'
 Click 301
 Start-Sleep -Milliseconds 150
 $original = Read-Control 114
 [void][UI]::SendMessage([UI]::GetDlgItem($h,114),0xB1,[IntPtr]0,[IntPtr]30)
 Send-Command 'say caret-test'
 Wait-Until { (Read-Control 114).EndsWith("ACK:say caret-test`r`n") } 'new output appends at end despite earlier selection'
 if (-not (Read-Control 114).StartsWith($original)) { throw 'Appending replaced or inserted into old output' }
 Write-Output 'PASS: existing console text preserved'
 Send-Command 'login'
 Wait-Until { (Player-Count) -eq 1 } 'fragmented default login log adds player'
 Send-Command 'joined'
 Wait-Until { (Read-Control 114).Contains('ZenE_ joined the game') } 'join log delivered'
 if ((Player-Count) -ne 1) { throw 'Duplicate player' }
 Write-Output 'PASS: login and join logs deduplicated'
 Send-Command 'ipv6'
 Wait-Until { (Player-Count) -eq 2 } 'IPv6 login adds second player'
 if ($CaptureScreenshots) {
  Start-Sleep -Seconds 3
  Click 300; Capture 'dashboard'
  Click 301; Capture 'console'
  Click 303; Capture 'players'
  Click 306; Capture 'settings'
  Click 301
 }
 $original=(Read-Control 114)
 $log=[UI]::GetDlgItem($h,114)
 $wrapped=[UI]::SendMessage($log,0xBA,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
 # A long single line must wrap, then remain byte-for-byte intact after toggling.
 Send-Command ('say '+('wraptest' * 60))
 Wait-Until { (Read-Control 114).EndsWith(('ACK:say '+('wraptest' * 60)+"`r`n")) } 'long command delivered'
 $beforeWrap=Read-Control 114
 [void][UI]::SendMessage([UI]::GetDlgItem($h,330),0xF1,[IntPtr]0,[IntPtr]::Zero)
 Click 330
 Wait-Until { ([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x100000) -ne 0 } 'line wrap off enables horizontal scrolling'
 $offLines=[UI]::SendMessage([UI]::GetDlgItem($h,114),0xBA,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
 if ((Read-Control 114) -ne $beforeWrap) { Set-Content (Join-Path $root 'before-wrap.txt') $beforeWrap; Set-Content (Join-Path $root 'after-wrap.txt') (Read-Control 114); throw 'Wrap toggle changed log content' }
 [void][UI]::SendMessage([UI]::GetDlgItem($h,330),0xF1,[IntPtr]1,[IntPtr]::Zero)
 Click 330
 Wait-Until { ([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x100000) -eq 0 } 'line wrap on disables horizontal scrolling'
 Wait-Until { (Read-Control 114) -eq $beforeWrap -and [UI]::SendMessage([UI]::GetDlgItem($h,114),0xBA,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -gt $offLines } 'long lines finish reflowing after wrap toggle' 
 if ((Read-Control 114) -ne $beforeWrap) { throw 'Wrap toggle lost text' }
 $position=(Read-Control 114).Replace("`r",'').IndexOf('RGB-COLOR')
 if ([UI]::ColorAt([UI]::GetDlgItem($h,114),$p.Id,$position) -ne (12+200*256+80*65536)) { throw 'Wrap toggle lost RGB formatting' }
 Write-Output 'PASS: console colors survive wrap recreation' 
 Write-Output 'PASS: toggling line wrap preserves complete log'

 Send-Command 'spoof'
 Wait-Until { (Read-Control 114).Contains('<Steve> Evil joined the game') } 'chat text delivered'
 if ((Player-Count) -ne 2) { throw 'Chat spoof created a player' }
 Write-Output 'PASS: chat text does not create players'
 Send-Command 'longline'
 Wait-Until { (Read-Control 114).Contains(('x' * 100)) } 'long server log delivered'
 if ((Player-Count) -ne 2) { throw 'Long line created a player' }
 Write-Output 'PASS: oversized line ignored by player parser'
 Click 111
 Wait-Until { (Read-Control 114).Length -eq 0 } 'console cleared'
 if ((Player-Count) -ne 2) { throw 'Clearing console cleared players' }
 Write-Output 'PASS: clearing console preserves connected players'
 # WM_SETREDRAW(TRUE) must not make the hidden console visible on another page.
 foreach ($tab in @(302,303,304,305,306)) {
  Click $tab
  Wait-Until { ([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -eq 0 } ("console hidden on page "+$tab)
  Send-Command ('say hidden-page-'+$tab)
  Wait-Until { try { (Get-Content (Join-Path $root 'last-command.txt') -Raw) -eq ('say hidden-page-'+$tab) } catch { $false } } ("background command received on page "+$tab)
  if (([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -ne 0) { throw ("Background output showed console on page "+$tab) }
  Click 301
  Wait-Until { (Read-Control 114).Contains('ACK:say hidden-page-'+$tab) } 'background model renders when console is selected'
 }
 Click 306
 Wait-Until { ([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -eq 0 } 'settings hides console before burst' 
 Send-Command 'logburst'
 Wait-Until { Test-Path (Join-Path $root 'burst-done.txt') } 'large background log burst delivered while editor remains hidden'
 if (([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -ne 0) { throw 'Log trim showed console over settings' }
 Capture 'settings-background-log'
 Click 301
 Wait-Until { ([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -ne 0 -and (Read-Control 114).Contains('BURST-DONE') } 'console tab restores collected background output'
 if ((Read-Control 114).Length -gt 500000) { throw 'Log trim did not run' }

 Send-Command 'quit'
 Wait-Until { (Player-Count) -eq 1 } 'lost connection removes player'
 Send-Command 'left'
 Wait-Until { (Read-Control 114).Contains('ZenE_ left the game') } 'duplicate departure delivered'
 if ((Player-Count) -ne 1) { throw 'Duplicate quit removed the wrong player' }
 Write-Output 'PASS: duplicate departure is harmless'
 Send-Command 'notchquit'
 Wait-Until { (Player-Count) -eq 0 } 'unprefixed default departure removes player'
 Send-Command 'login'
 Wait-Until { (Player-Count) -eq 1 } 'player can reconnect'
 Click 108
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'graceful stop returns UI to idle'
 if (-not (Test-Path (Join-Path $root 'stopped.txt'))) { throw 'Stop was not delivered' }
 if ((Player-Count) -ne 0) { throw 'Server stop did not clear players' }
 Write-Output 'PASS: server stop clears players'
 # New settings and drag/drop features use this fixture only.
 Click 306
 Click 551
 $folderPath=Join-Path $root 'server test.jar'
 Put 102 ('"'+$folderPath+'"')
 Click 263
 Wait-Until { (Read-Control 102) -eq $folderPath } 'quoted paths normalized on save'
 $image=Join-Path $root 'custom-profile.png'
 $bitmap=New-Object Drawing.Bitmap 64,64
 $graphics=[Drawing.Graphics]::FromImage($bitmap); $graphics.Clear([Drawing.Color]::CornflowerBlue)
 $bitmap.Save($image,[Drawing.Imaging.ImageFormat]::Png); $graphics.Dispose(); $bitmap.Dispose()
 [void][UI]::DropFile($h,$image)
 Wait-Until { (Read-Control 520).Contains('profile-images') } 'profile image drag and drop'
 $managed=Read-Control 520
 Click 263
 if (-not (Test-Path $managed)) { throw 'Image not copied into app storage' }
 Remove-Item $image
 Click 553
 [void][UI]::SendMessage([UI]::GetDlgItem($h,557),0x14E,[IntPtr]1,[IntPtr]::Zero)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14E,[IntPtr]1,[IntPtr]::Zero)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,576),0x14E,[IntPtr]1,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Read-Control 300) -eq 'Dashboard' -and (Read-Ini).Contains('Language=en') -and (Read-Ini).Contains('Theme=1') -and (Read-Ini).Contains('Font=Noto Sans KR') } 'English, light theme and bundled Noto font saved'
 if (([UI]::GetWindowLong([UI]::GetDlgItem($h,114),-16) -band 0x10000000) -ne 0) { throw 'Theme change showed hidden console' }
 Capture 'settings-appearance-light-en' 
 Click 107
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,112)) } 'light-theme server starts'
 $livePid=Read-Control 425
 Click 550
 if (-not [UI]::IsWindowEnabled([UI]::GetDlgItem($h,264))) { throw 'Running server name is disabled' }
 Put 264 'Live Renamed Server'
 Click 263
 Wait-Until { (Read-Control 424) -eq 'Live Renamed Server' -and (Read-Ini).Contains('Name=Live Renamed Server') } 'running server can be renamed and saved'
 if ((Read-Control 425) -ne $livePid) { throw 'Renaming restarted the server' }
 Assign-Group 'Integration' @(0)
 Open-Groups
 Manager-Name 'Integration'; Manager-Click 723
 Wait-Until { ($popup=[UI]::GetLastActivePopup($h)) -ne $script:manager -and $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'duplicate group name opens themed error'
 $dialog=[UI]::GetLastActivePopup($h)
 if ([UI]::GetDlgItem($dialog,10) -eq [IntPtr]::Zero -or [UI]::IsWindowEnabled($h)) { throw 'Error must use themed modal and disable root owner' }
 Capture-Owned $dialog 'dialog-error-light-en'
 [void][UI]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
 Wait-Until { [UI]::IsWindowEnabled($h) -and [UI]::IsWindowEnabled($script:manager) } 'error dismissal restores both owner windows'
 [void][UI]::SendMessage($script:manager,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Click 605
 Wait-Until { $popup=[UI]::GetLastActivePopup($h); $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'themed information dialog opens'
 $dialog=[UI]::GetLastActivePopup($h)
 if ([UI]::GetDlgItem($dialog,10) -eq [IntPtr]::Zero) { throw 'Information dialog is not themed' }
 Capture-Owned $dialog 'dialog-information-light-en'
 [void][UI]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
 Wait-Until { [UI]::IsWindowEnabled($h) } 'information dialog closes'
 Click 552
 if (-not [UI]::IsWindowEnabled([UI]::GetDlgItem($h,559))) { throw 'Running server properties are not editable' }
 Put 559 '25567'
 Put 561 '25'
 Click 263
 Wait-Until { (Test-Path (Join-Path $root 'server.properties')) -and (Get-Content (Join-Path $root 'server.properties') -Raw).Contains('server-port=25567') } 'running server properties save for the next restart'
 if ((Read-Control 425) -ne $livePid) { throw 'Saving properties restarted the server' }
 [void][UI]::SendMessage([UI]::GetDlgItem($h,568),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Get-Content (Join-Path $root 'server.properties') -Raw).Contains('pvp=false') } 'custom checkbox state saves while running'
 if ([UI]::SendMessage([UI]::GetDlgItem($h,568),0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0) { throw 'Checkbox visual state reset after save' }
 Capture 'checkbox-saved-running'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,568),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Get-Content (Join-Path $root 'server.properties') -Raw).Contains('pvp=true') } 'custom checkbox can toggle and save repeatedly'
 Capture 'settings-properties-running'
 Click 301
 [void][UI]::SendMessage([UI]::GetDlgItem($h,331),0xF1,[IntPtr]0,[IntPtr]::Zero)
 Click 331
 Wait-Until { (Read-Ini).Contains('AutoScroll=0') } 'auto scroll can be paused'
 Send-Command 'colors'
 Wait-Until { (Read-Control 114).Contains('RGB-COLOR') -and [UI]::SendMessage([UI]::GetDlgItem($h,331),0xF0,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 } 'sending command resumes auto scroll for its result'
 [UI]::ColorSearch='RESET-COLOR'
 if ([UI]::ColorAt([UI]::GetDlgItem($h,114),$p.Id,0) -ne (27+27*256+27*65536)) { throw 'Light console default is not dark' }
 [UI]::ColorSearch='RGB-COLOR'
 Capture 'console-light-rgb'
 Send-Command 'tabs'
 Wait-Until { (Read-Control 114).Contains("TAB-A`tTAB-B`tTAB-C") } 'console preserves tab-separated log text'
 Send-Command 'scrolltest'
 Wait-Until { (Read-Control 114).Contains('SCROLL-LINE-79') -and [UI]::SendMessage([UI]::GetDlgItem($h,114),0xCE,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -gt 0 } 'console follows the latest command output'
 [void][UI]::ShowWindow($h,4)
 Wait-Until { [UI]::LastLogLineVisible([UI]::GetDlgItem($h,114),$p.Id) } 'last console result line is fully visible'
 Capture 'console-light-scroll'
 Click 300
 Wait-Until { [UI]::SendMessage([UI]::GetDlgItem($h,114),0xCE,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -gt 0 } 'recent console follows the log tail after dashboard switch'
 [void][UI]::ShowWindow($h,4)
 Wait-Until { [UI]::LastLogLineVisible([UI]::GetDlgItem($h,114),$p.Id) } 'last recent-console line is fully visible after resize'
 Capture 'dashboard-recent-scroll'
 Click 108
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'light-theme server stops'
 Click 306; Click 553

 [void][UI]::ShowWindow($h,4)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14F,[IntPtr]1,[IntPtr]::Zero)
 Capture 'dropdown-light-en'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14F,[IntPtr]0,[IntPtr]::Zero)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,557),0x14E,[IntPtr]0,[IntPtr]::Zero)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14E,[IntPtr]0,[IntPtr]::Zero)
 [void][UI]::SendMessage([UI]::GetDlgItem($h,576),0x14E,[IntPtr]0,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Read-Ini).Contains('Language=ko') -and (Read-Ini).Contains('Font=NanumGothic') } 'Korean and default bundled Nanum restored'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14E,[IntPtr]2,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Read-Ini).Contains('Theme=2') } 'system theme saved'
 Capture 'settings-system'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,574),0x14E,[IntPtr]0,[IntPtr]::Zero)
 Click 263
 Wait-Until { (Read-Ini).Contains('Theme=0') } 'dark theme restored'
 Set-Content (Join-Path $root 'server.properties') "# keep comment`ncustom-property=keep`nserver-port=25565" -Encoding Ascii
 Click 552
 Put 559 '25564'
 Click 571
 Wait-Until { (Read-Control 559) -eq '25565' } 'properties reload completes before editing'
 Put 559 '25566'
 Put 561 '30'
 Click 263
 Wait-Until { (Get-Content (Join-Path $root 'server.properties') -Raw).Contains('server-port=25566') } 'server properties save button applies edits'
 if (-not (Get-Content (Join-Path $root 'server.properties') -Raw).Contains('custom-property=keep')) { throw 'Unknown properties lost' }
 Capture 'settings-properties'
 Click 550; Capture 'settings-general'
 Click 551; Capture 'settings-launch'
 Put 102 (Join-Path $root 'missing-server.jar')
 Click 263
 Wait-Until { (Read-Control 583).Length -gt 0 -and -not (Read-Ini).Contains('missing-server.jar') } 'typed nonexistent server JAR is rejected with inline feedback'
 if ((Read-Ini).Contains('missing-server.jar')) { throw 'Invalid path was persisted' }
 Put 102 (Join-Path $root 'server test.jar'); Click 263
 Wait-Until { (Read-Control 102) -eq (Join-Path $root 'server test.jar') } 'valid typed JAR restored' 
 Click 553; Capture 'settings-appearance-dark-ko'
 # File management is exercised only in this isolated fixture.
 [void](New-Item -ItemType Directory -Path (Join-Path $root 'plugins') -Force)
 Add-Type -AssemblyName System.IO.Compression
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $plugin=Join-Path $root 'plugins/test-plugin.jar'
 $archive=[IO.Compression.ZipFile]::Open($plugin,[IO.Compression.ZipArchiveMode]::Create)
 $entry=$archive.CreateEntry('plugin.yml')
 $writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false))
 $writer.Write("name: ActualPlugin`nversion: '1.2.3'`n"); $writer.Dispose(); $archive.Dispose()
 Click 302
 Wait-Until { [UI]::SendMessage([UI]::GetDlgItem($h,350),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 } 'plugins tab lists JAR files'
 Wait-Until { [UI]::ListCell([UI]::GetDlgItem($h,350),$p.Id,1) -eq 'ActualPlugin' -and [UI]::ListCell([UI]::GetDlgItem($h,350),$p.Id,2) -eq '1.2.3' } 'actual plugin name and version displayed from deflated plugin.yml'
 $hash=(Get-FileHash $plugin -Algorithm SHA256).Hash.ToLowerInvariant()
 if (-not (Test-Path (Join-Path $root ('cache/plugin-cache/'+$hash+'.txt')))) { throw 'SHA-256 metadata cache missing' }
 if (Test-Path (Join-Path $root 'plugin-cache')) { throw 'Plugin cache was created next to the EXE' }
 Write-Output 'PASS: SHA-256 text cache matches JAR contents' 
 function Select-Row($id) {
  $list=[UI]::GetDlgItem($script:h,$id)
  $header=[UI]::SendMessage($list,0x101F,[IntPtr]::Zero,[IntPtr]::Zero)
  $r=New-Object UI+RECT; [void][UI]::GetWindowRect($header,[ref]$r)
  $point=($r.Bottom-$r.Top+6)*65536+24
  [void][UI]::PostMessage($list,0x201,[IntPtr]1,[IntPtr]$point)
  [void][UI]::PostMessage($list,0x202,[IntPtr]0,[IntPtr]$point)
  Start-Sleep -Milliseconds 150
 }
 Select-Row 350
 Click 353
 Wait-Until { (Test-Path (Join-Path $root 'plugins/test-plugin.jar.disabled')) -and [UI]::SendMessage([UI]::GetDlgItem($h,350),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 } 'plugin can be disabled while stopped'
 Select-Row 350
 Click 353
 Wait-Until { (Test-Path (Join-Path $root 'plugins/test-plugin.jar')) -and [UI]::SendMessage([UI]::GetDlgItem($h,350),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 } 'plugin can be enabled again'
 Capture 'plugins'
 $backupRoot=Join-Path ([IO.Path]::GetTempPath()) ('wrapper-backups-'+[guid]::NewGuid().ToString('N'))
 [void](New-Item -ItemType Directory -Path (Join-Path $root 'world/region') -Force)
 Set-Content (Join-Path $root 'world/region/test.mca') 'world-fixture'
 Put 374 $backupRoot
 Click 304
 Click 371
 Wait-Until { (Test-Path $backupRoot) -and @(Get-ChildItem $backupRoot -Recurse -Filter 'snapshot-*.zip' -ErrorAction SilentlyContinue).Count -eq 1 } 'stopped-server ZIP backup completes outside source'
 $backup=Get-ChildItem $backupRoot -Recurse -Filter 'snapshot-*.zip' | Select-Object -First 1
 $archive=[IO.Compression.ZipFile]::OpenRead($backup.FullName)
 try {
  $entry=$archive.GetEntry('world/region/test.mca')
  if (-not $entry) { $entry=$archive.GetEntry('world\region\test.mca') }
  $reader=[IO.StreamReader]::new($entry.Open())
  try { $savedWorld=$reader.ReadToEnd() } finally { $reader.Dispose() }
 } finally { $archive.Dispose() }
 if ($savedWorld -ne (Get-Content (Join-Path $root 'world/region/test.mca') -Raw)) { throw 'Backup content differs' }
 Write-Output 'PASS: backup preserves world file contents'
 Capture 'backups'
 Click 305
 Put 392 '720'
 Put 580 'Custom 12m'
 Click 581
 Wait-Until { (Read-Ini).Contains('Preset0Name=Custom 12m') -and (Read-Ini).Contains('Preset0Seconds=720') } 'custom interval preset saved'
 Put 392 '1'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,529),0x14E,[IntPtr]7,[IntPtr]::Zero)
 [void][UI]::PostMessage($h,0x111,[IntPtr](529+65536),[IntPtr]::Zero)
 Wait-Until { (Read-Control 392) -eq '720' } 'custom interval preset applied'
 Capture 'scheduler-custom-preset'
 Put 392 '1'
 Put 393 'say scheduler-test' 
 Click 394
 Wait-Until { [UI]::SendMessage([UI]::GetDlgItem($h,390),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 1 } 'scheduler task added'
 Capture 'scheduler'
 Click 301
 Click 107
 Wait-Until { (Read-Control 114).Contains('ACK:say scheduler-test') } 'scheduled command reaches running server'
 Select-Row 390
 Click 396
 Wait-Until { (Read-Ini).Contains('Task0Enabled=0') } 'scheduled task can be paused and persisted'
 Select-Row 390
 Put 392 '3600'
 Put 393 'say edited-task'
 Click 528
 Wait-Until { (Read-Ini).Contains('Task0Seconds=3600') -and (Read-Ini).Contains('Task0Command=say edited-task') -and (Read-Ini).Contains('Task0Enabled=0') } 'selected scheduler task edits preserve paused state'
 [void][UI]::SendMessage([UI]::GetDlgItem($h,529),0x14E,[IntPtr]1,[IntPtr]::Zero)
 [void][UI]::PostMessage($h,0x111,[IntPtr](529+65536),[IntPtr]::Zero)
 Wait-Until { (Read-Control 392) -eq '60' } 'scheduler minute preset uses seconds'
 Click 301
 $pidBefore=(Read-Control 425)
 Click 250
 Wait-Until { (Read-Control 425) -ne $pidBefore -and (Read-Control 425).Contains('PID ') } 'restart waits for old process and starts new PID'
 $firstPid=(Read-Control 425)
 Click 261
 Wait-Until { $popup=[UI]::GetLastActivePopup($h); $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'new profile asks for EULA once'
 [void][UI]::PostMessage([UI]::GetLastActivePopup($h),0x111,[IntPtr]6,[IntPtr]::Zero)
 Wait-Until { [UI]::GetLastActivePopup($h) -eq $h } 'EULA consent accepted'
 $second=Join-Path $root 'second-server'
 [void](New-Item -ItemType Directory -Path $second -Force)
 New-ServerJar (Join-Path $second 'second.jar') 'com.velocitypowered.proxy.Velocity'
 Set-Content (Join-Path $second 'eula.txt') 'eula=false' -Encoding Ascii
 Put 264 'Second Test Server'
 Put 102 (Join-Path $second 'second.jar')
 Click 263
 Assign-Group 'Integration' @(0,1)
 Click 301
 Click 107
 Wait-Until { (Read-Control 114).Contains('READY') -and (Read-Control 425).Contains('PID ') } 'second profile starts its own server'
 $secondPid=Read-Control 425
 if ($secondPid -eq $firstPid) { throw 'Two profiles share process metadata' }
 function Select-Server($index) {
  [void][UI]::SendMessage([UI]::GetDlgItem($script:h,260),0x186,[IntPtr]$index,[IntPtr]::Zero)
  [void][UI]::PostMessage($script:h,0x111,[IntPtr](260+65536),[IntPtr]::Zero)
 }
 Select-Server 0
 Wait-Until { (Read-Control 425) -eq $firstPid } 'switching profiles preserves first running process'
 Send-Command 'say first-profile'
 Wait-Until { (Read-Control 114).Contains('ACK:say first-profile') } 'first profile command routes to first process'
 Send-Command 'login'
 Select-Server 1
 Wait-Until { (Read-Control 425) -eq $secondPid } 'second process remains running while first selected'
 Send-Command 'say second-profile'
 Wait-Until { (Read-Control 114).Contains('ACK:say second-profile') } 'second profile command routes to second process'
 if ((Read-Control 114).Contains('ACK:say first-profile')) { throw 'Logs mixed across profiles' }
 if ((Player-Count) -ne 0) { throw 'Roster mixed across profiles' }
 Write-Output 'PASS: profile consoles and player lists remain isolated'
 Open-Groups; Manager-Name 'Empty archive'; Manager-Click 723
 Wait-Until { (Read-Ini).Contains('Empty archive') } 'empty group can be created and persisted'
 Capture-Owned $script:manager 'group-manager-empty'
 [void][UI]::SendMessage($script:manager,0x10,[IntPtr]::Zero,[IntPtr]::Zero)

 Start-Sleep -Milliseconds 300
 Select-Server 0
 Wait-Until { (Player-Count) -eq 1 } 'inactive server login is collected and restored on selection'
 Click 108
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'first server stopped while second keeps running'
 Click 107
 Wait-Until { -not [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'restart after stop'
 Put 112 'crash'
 Click 113
 Wait-Until { (Read-Control 114).Contains(': 7') } 'unexpected exit code reporting'
 Click 107
 Wait-Until { -not [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'restart after unexpected exit'
 Remove-Item (Join-Path $root 'stopped.txt')
 [void][UI]::PostMessage($h,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until { $p.Refresh(); $p.HasExited } 'window close stops server before exiting'
 if (-not (Test-Path (Join-Path $root 'stopped.txt'))) { throw 'Close skipped graceful stop' }
 Write-Output 'PASS: close delivered stop'
 if (-not (Test-Path (Join-Path $root 'second-server/stopped.txt'))) { throw 'Window close did not stop second server' }
 Write-Output 'PASS: closing wrapper gracefully stops all server processes'
 # Reload saved settings and check assets / EULA persistence without automatic server start.
 $p=Start-Process (Join-Path $root 'MinecraftServerWrapper.exe') -PassThru
 Wait-Until { $p.Refresh(); $p.MainWindowHandle -ne 0 } 'saved workspace reopens'
 $script:h=$p.MainWindowHandle; [void][UI]::ShowWindow($h,0)
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) -and (Read-Control 520) -eq $managed } 'saved image restored and servers remain offline'
 Remove-Item (Join-Path $root 'second-server/eula.txt')
 Select-Server 1
 Click 107
 Wait-Until { (Read-Control 114).Contains('READY') } 'persisted EULA consent recreates missing eula.txt without asking again'
 Click 108
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'reopened second server stopped'
 Click 261
 Wait-Until { $popup=[UI]::GetLastActivePopup($h); $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'temporary profile EULA dialog'
 Capture-Owned ([UI]::GetLastActivePopup($h)) 'dialog-eula-dark-ko'
 [void][UI]::PostMessage([UI]::GetLastActivePopup($h),0x111,[IntPtr]6,[IntPtr]::Zero)
 Wait-Until { [UI]::SendMessage([UI]::GetDlgItem($h,260),0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 3 } 'temporary profile registered'
 Open-Groups
 Select-Group 'Integration'
 Manager-Click 727
 Select-Server 0
 Wait-Until { (Read-Control 425).Contains('PID ') } 'group start launches first member'
 Select-Server 1
 Wait-Until { (Read-Control 425).Contains('PID ') } 'group start launches second member'
 Select-Server 2
 Wait-Until { -not [UI]::IsWindowEnabled([UI]::GetDlgItem($h,109)) -and -not (Read-Control 425).Contains('PID ') } 'group start leaves non-member offline'
 Capture-Owned $script:manager 'group-manager-running'
 Manager-Name 'Ops'; Manager-Click 724
 Wait-Until { ([regex]::Matches((Read-Ini),'(?m)^Group=Ops\r?$')).Count -eq 2 } 'renaming populated group preserves both memberships'
 Manager-Name 'Integration'; Manager-Click 724
 Wait-Until { ([regex]::Matches((Read-Ini),'(?m)^Group=Integration\r?$')).Count -eq 2 } 'group rename saves without restarting members'
 Manager-Click 728
 Select-Server 0
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'group stop gracefully stops first member'
 Select-Server 1
 Wait-Until { [UI]::IsWindowEnabled([UI]::GetDlgItem($h,107)) } 'group stop gracefully stops second member'
 Select-Group 'Empty archive'; Manager-Name 'Renamed archive'; Manager-Click 724
 Wait-Until { (Read-Ini).Contains('Renamed archive') -and -not (Read-Ini).Contains('Empty archive') } 'empty group survives relaunch and can be renamed'
 Manager-Click 725
 Wait-Until { ($popup=[UI]::GetLastActivePopup($h)) -ne $script:manager -and $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'themed group delete confirmation opens'
 $dialog=[UI]::GetLastActivePopup($h)
 if ([UI]::GetDlgItem($dialog,10) -eq [IntPtr]::Zero) { throw 'Confirmation is not the themed dialog' }
 Capture-Owned $dialog 'dialog-group-delete'
 [void][UI]::PostMessage($dialog,0x100,[IntPtr]27,[IntPtr]::Zero)
 Wait-Until { -not [UI]::IsWindow($dialog) -and [UI]::IsWindowEnabled($h) } 'Escape cancels themed confirmation and restores owners'
 if (-not (Read-Ini).Contains('Renamed archive')) { throw 'Cancel deleted the group' }
 Manager-Click 725
 Wait-Until { ($popup=[UI]::GetLastActivePopup($h)) -ne $script:manager -and $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'group confirmation can be reopened'
 $dialog=[UI]::GetLastActivePopup($h)
 [void][UI]::PostMessage($dialog,0x111,[IntPtr]6,[IntPtr]::Zero)
 Wait-Until { -not (Read-Ini).Contains('Renamed archive') } 'group deletion retains member files'

 [void][UI]::SendMessage($script:manager,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Click 305
 if ([UI]::SendMessage([UI]::GetDlgItem($h,529),0x146,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 8) { throw 'Custom preset not restored after relaunch' }
 [void][UI]::SendMessage([UI]::GetDlgItem($h,529),0x14E,[IntPtr]7,[IntPtr]::Zero)
 Click 582
 Wait-Until { (Read-Ini).Contains('PresetCount=0') -and [UI]::SendMessage([UI]::GetDlgItem($h,529),0x146,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 7 } 'custom preset persists across relaunch and can be deleted'
 Select-Server 2

 $list=[UI]::GetDlgItem($h,260); $row=[UI]::ListItemRect($list,$p.Id,2); $point=(($row.Top+$row.Bottom)/2)*65536+$row.Right-12
 [void][UI]::PostMessage($list,0x200,[IntPtr]::Zero,[IntPtr]$point)
 Capture 'server-list-hover'
 [void][UI]::PostMessage($list,0x201,[IntPtr]1,[IntPtr]$point)
 [void][UI]::PostMessage($list,0x202,[IntPtr]::Zero,[IntPtr]$point)
 Wait-Until { $popup=[UI]::GetLastActivePopup($h); $popup -ne $h -and [UI]::GetDlgItem($popup,10) -ne [IntPtr]::Zero -and -not [UI]::IsWindowEnabled($h) } 'hover close asks before profile removal'
 [void][UI]::PostMessage([UI]::GetLastActivePopup($h),0x111,[IntPtr]6,[IntPtr]::Zero)
 Wait-Until { [UI]::SendMessage([UI]::GetDlgItem($h,260),0x18B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 2 } 'hover close removes only profile'
 if (-not (Test-Path (Join-Path $root 'server test.jar')) -or -not (Test-Path (Join-Path $root 'second-server/second.jar'))) { throw 'Profile removal deleted server files' }
 [void][UI]::PostMessage($h,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Wait-Until { $p.Refresh(); $p.HasExited } 'reopened workspace closes cleanly'

} finally {
 $script:performance | ConvertTo-Json | Set-Content (Join-Path $BuildDirectory 'performance.json')
 if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
 if ($KeepArtifacts) { Write-Output "Artifacts: $root" }
 else {
  for ($attempt=0;$attempt -lt 10 -and (Test-Path $root);$attempt++) {
   Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
   if (Test-Path $root) { Start-Sleep -Milliseconds 200 }
  }
  if ($backupRoot -and (Test-Path $backupRoot)) { Remove-Item $backupRoot -Recurse -Force -ErrorAction SilentlyContinue }
 }
}
