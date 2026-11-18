param([string]$BuildDirectory=(Join-Path $PSScriptRoot '../cmake-build-mcss'),[switch]$KeepArtifacts)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class OpsUI {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,int m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetLastActivePopup(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,string value);
}
'@
$BuildDirectory=(Resolve-Path $BuildDirectory).Path
$root=Join-Path ([IO.Path]::GetTempPath()) ('wrapper-e2e-'+[guid]::NewGuid().ToString('N'))
$server=Join-Path $root 'server'
$backups=Join-Path $root 'backups'
[void](New-Item -ItemType Directory $server -Force)
Copy-Item (Join-Path $BuildDirectory 'MinecraftServerWrapper.exe') $root
Copy-Item (Join-Path $BuildDirectory 'wrapper_mock_server.exe') $root
Copy-Item (Join-Path $BuildDirectory 'assets') $root -Recurse
$mock=Join-Path $root 'wrapper_mock_server.exe'
$listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
$id='a'*64;$admin='b'*64;$viewer='c'*64;$operator='d'*64
$ini=@"
[Workspace]
Count=1
Language=en
[Remote]
Enabled=1
Port=$port
Users=admin:admin:$admin;reader:viewer:$viewer;operator:operator:$operator
[Profile0]
Id=$id
Name=Operational test
LaunchMode=3
LaunchTarget=$mock
WorkingDirectory=$server
AutoStart=1
AutoRecover=1
RecoveryDelay=1
RecoveryLimit=2
EulaAccepted=1
Backup=$backups
ZipBackup=1
BackupKeep=2
"@
[IO.File]::WriteAllText((Join-Path $root 'wrapper.ini'),$ini,[Text.Encoding]::Unicode)
$p=$null
function Wait-For($predicate,$label,$seconds=20){$end=[DateTime]::UtcNow.AddSeconds($seconds);while([DateTime]::UtcNow -lt $end){if(&$predicate){Write-Output "PASS: $label";return};Start-Sleep -Milliseconds 100};throw "Timeout: $label"}
function Api($path,$method='GET',$body='',$token=$admin){
 $params=@{Uri="http://127.0.0.1:$port$path";Method=$method;Headers=@{Authorization="Bearer $token"};UseBasicParsing=$true;TimeoutSec=8}
 if($method -ne 'GET'){$params.Body=[Text.Encoding]::UTF8.GetBytes($body);$params.ContentType='text/plain; charset=utf-8'}
 $response=Invoke-WebRequest @params
 return ($response.Content|ConvertFrom-Json)
}
function Assert-Denied($path,$method,$token,$status){try{Api $path $method '' $token|Out-Null;throw 'Expected denial'}catch{if(-not $_.Exception.Response -or [int]$_.Exception.Response.StatusCode -ne $status){throw}};Write-Output "PASS: request denied with $status"}
function State {return (Api '/api/servers').servers[0]}
try{
 $p=Start-Process (Join-Path $root 'MinecraftServerWrapper.exe') -PassThru
 Wait-For {try{(State).running}catch{$false}} 'autostart native server and authenticated API'
 $base="/api/servers/$id"
 Assert-Denied '/api/servers' 'GET' 'wrong-key' 401
 Assert-Denied "$base/stop" 'POST' $viewer 403
 Assert-Denied '/api/users' 'GET' $operator 403
 $before=(State).pid
 Api "$base/command" 'POST' 'crash'|Out-Null
 Wait-For {$s=State;$s.running -and $s.pid -ne $before} 'unexpected exit recovers automatically'
 $before=(State).pid
 Api "$base/command" 'POST' 'crash'|Out-Null
 Wait-For {$s=State;$s.running -and $s.pid -ne $before} 'second recovery allowed within limit'
 Api "$base/command" 'POST' 'crash'|Out-Null
 Wait-For {-not (State).running} 'third crash exits server'
 Start-Sleep -Seconds 3
 if((State).running){throw 'Recovery limit ignored'}
 Write-Output 'PASS: recovery limit stops restart loop'
 Api "$base/start" 'POST'|Out-Null
 Wait-For {(State).running} 'manual start resets recovery budget'
 Api "$base/stop" 'POST'|Out-Null
 Wait-For {-not (State).running} 'remote normal stop completes'
 Start-Sleep -Seconds 2
 if((State).running){throw 'Intentional stop restarted unexpectedly'}
 Write-Output 'PASS: intentional stop never triggers recovery'
 Api "$base/start" 'POST'|Out-Null
 Wait-For {(State).running} 'remote start'
 Api "$base/tasks" 'POST' '0|1|-1|colors'|Out-Null
 Wait-For {(Api "$base/log").log.Contains('ACK:colors')} 'scheduled command executes through app timer'
 $tasks=(Api "$base/tasks").tasks
 if($tasks.Count -ne 1 -or -not $tasks[0].lastRun){throw 'Task execution metadata missing'}
 Api "$base/tasks/0" 'DELETE'|Out-Null
 if((Api "$base/tasks").tasks.Count -ne 0){throw 'Task delete failed'}
 Api "$base/tasks" 'POST' '0|3600|1439|save-all'|Out-Null
 if((Api "$base/tasks").tasks[0].minute -ne 1439){throw 'Daily timing failed'}
 Write-Output 'PASS: task CRUD and daily timing'
 Api "$base/tasks/0" 'DELETE'|Out-Null
 [void](New-Item -ItemType Directory (Join-Path $server 'world/region') -Force)
 [IO.File]::WriteAllText((Join-Path $server 'world/region/test.mca'),'world-data')
 $before=(State).pid
 Api "$base/backup" 'POST'|Out-Null
 Wait-For {$s=State;$s.running -and $s.pid -ne $before -and @(Get-ChildItem $backups -Recurse -Filter '*.zip' -ErrorAction SilentlyContinue).Count -eq 1} 'running backup stops, archives, and restarts server' 40
 $names=(Api "$base/backups").backups
 if($names.Count -ne 1 -or $names[0] -notlike '*.zip'){throw 'Backup API listing failed'}
 Write-Output 'PASS: completed backup listed through API'
 $newToken='e'*64
 Api '/api/users' 'POST' "newreader:viewer:$newToken"|Out-Null
 if((Api '/api/servers' 'GET' '' $newToken).servers.Count -ne 1){throw 'New user cannot read'}
 $users=Api '/api/users'
 if(($users|ConvertTo-Json -Depth 5).Contains($newToken)){throw 'User listing leaked token'}
 Api '/api/users/newreader' 'DELETE'|Out-Null
 Assert-Denied '/api/servers' 'GET' $newToken 401
 Write-Output 'PASS: account creation, revocation, and secret-free listing'
 Api "$base/stop" 'POST'|Out-Null
 Wait-For {-not (State).running} 'server stopped before relaunch'
 $p.Refresh();$main=$p.MainWindowHandle
 [void][OpsUI]::PostMessage($main,0x111,[IntPtr]800,[IntPtr]::Zero)
 Wait-For {$popup=[OpsUI]::GetLastActivePopup($main);$popup -ne $main -and [OpsUI]::GetDlgItem($popup,109) -ne [IntPtr]::Zero -and -not [OpsUI]::IsWindowEnabled($main)} 'advanced launch dialog fully created'
 $popup=[OpsUI]::GetLastActivePopup($main)
 [void][OpsUI]::SendMessage([OpsUI]::GetDlgItem($popup,108),0xC,[IntPtr]::Zero,'2')
 [void][OpsUI]::PostMessage($popup,0x111,[IntPtr]1,[IntPtr]::Zero)
 Wait-For {[OpsUI]::IsWindowEnabled($main) -and [IO.File]::ReadAllText((Join-Path $root 'wrapper.ini')).Contains('RecoveryDelay=2')} 'advanced dialog validates and persists settings'
 [void][OpsUI]::PostMessage($main,0x111,[IntPtr]804,[IntPtr]::Zero)
 Wait-For {$popup=[OpsUI]::GetLastActivePopup($main);$popup -ne $main -and [OpsUI]::GetDlgItem($popup,101) -ne [IntPtr]::Zero -and -not [OpsUI]::IsWindowEnabled($main)} 'backup options dialog created'
 $popup=[OpsUI]::GetLastActivePopup($main)
 [void][OpsUI]::SendMessage([OpsUI]::GetDlgItem($popup,101),0xC,[IntPtr]::Zero,'3')
 [void][OpsUI]::PostMessage($popup,0x111,[IntPtr]1,[IntPtr]::Zero)
 Wait-For {[OpsUI]::IsWindowEnabled($main) -and [IO.File]::ReadAllText((Join-Path $root 'wrapper.ini')).Contains('BackupKeep=3')} 'backup dialog persists retention'

 $p.CloseMainWindow()|Out-Null
 Wait-For {$p.Refresh();$p.HasExited} 'app shuts down with remote panel enabled'
 $p=Start-Process (Join-Path $root 'MinecraftServerWrapper.exe') -PassThru
 Wait-For {try{(State).running}catch{$false}} 'configuration persists and autostart works after relaunch'
 Api "$base/stop" 'POST'|Out-Null
 Wait-For {-not (State).running} 'reopened server stops'
 $p.CloseMainWindow()|Out-Null
 Wait-For {$p.Refresh();$p.HasExited} 'reopened app exits cleanly'
}finally{
 if($p){$p.Refresh();if(-not $p.HasExited){Stop-Process -Id $p.Id -Force;$p.WaitForExit()}}
 if($KeepArtifacts){Write-Output "Artifacts: $root"}else{Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue}
}
