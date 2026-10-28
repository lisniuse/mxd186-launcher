# -*- coding: utf-8 -*-
# snapshot_launch.ps1 —— 给「BAT 启动 vs BMS登录器启动」做一次现场快照, 用来 diff
#  用法:  powershell -ExecutionPolicy Bypass -File snapshot_launch.ps1 -Tag bat
#        powershell -ExecutionPolicy Bypass -File snapshot_launch.ps1 -Tag launcher
#  输出:  桌面\launch_<Tag>.txt
param([string]$Tag = "run")

$ErrorActionPreference = "SilentlyContinue"
$out = Join-Path ([Environment]::GetFolderPath("Desktop")) ("launch_{0}.txt" -f $Tag)
$lines = New-Object System.Collections.Generic.List[string]
function W($s) { $lines.Add($s) | Out-Null; Write-Host $s }
function H($s) { W ""; W ("=== " + $s + " ===") }

W ("snapshot tag={0}  time={1}" -f $Tag, (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
W ("user={0}  elevated={1}" -f $env:USERNAME, ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))

# ---------------- 进程 ----------------
H "进程 (name | pid | ppid | cmdline)"
Get-CimInstance Win32_Process |
    Where-Object { $_.Name -match "MapleStory|sdologin|BMS|wke|update|BlackCipher|nProtect|DevCon|cmd|powershell" } |
    Sort-Object Name |
    ForEach-Object {
        W ("{0,-22} pid={1,-7} ppid={2,-7} {3}" -f $_.Name, $_.ProcessId, $_.ParentProcessId, $_.CommandLine)
    }

# ---------------- 顶层窗口 ----------------
H "顶层窗口 (hwnd | class | title | pid | 进程)"
Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class WinEnum {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
}
"@
$procName = @{}
Get-Process | ForEach-Object { $procName[[uint32]$_.Id] = $_.ProcessName }
$rows = New-Object System.Collections.Generic.List[object]
$cb = [WinEnum+EnumProc]{
    param($h, $l)
    if ([WinEnum]::IsWindowVisible($h)) {
        $cn = New-Object System.Text.StringBuilder 256
        $tt = New-Object System.Text.StringBuilder 256
        [void][WinEnum]::GetClassName($h, $cn, 256)
        [void][WinEnum]::GetWindowTextW($h, $tt, 256)
        $pid_ = 0
        [void][WinEnum]::GetWindowThreadProcessId($h, [ref]$pid_)
        $title = $tt.ToString()
        $cls = $cn.ToString()
        if ($title -match "BMS|冒险岛|Maplestory|Maple|登录|盛大|sdo" -or $cls -match "MapleStory|sdologin|BMS") {
            $rows.Add([pscustomobject]@{ Hwnd = $h; Cls = $cls; Title = $title; Pid = $pid_ })
        }
    }
    return $true
}
[void][WinEnum]::EnumWindows($cb, [IntPtr]::Zero)
foreach ($r in ($rows | Sort-Object Pid)) {
    $pn = $procName[[uint32]$r.Pid]
    W ("{0,-12} {1,-28} {2,-34} pid={3,-7} {4}" -f $r.Hwnd, $r.Cls, $r.Title, $r.Pid, $pn)
}

# ---------------- 网络 ----------------
H "监听/连接 (8484 / 7575 / 443 / 3306)"
Get-NetTCPConnection | Where-Object { $_.LocalPort -in 8484, 7575, 443, 3306 -or $_.RemotePort -in 8484, 7575, 443, 3306 } |
    Select-Object State, LocalAddress, LocalPort, RemoteAddress, RemotePort, OwningProcess |
    Sort-Object LocalPort |
    ForEach-Object { W ("{0,-12} {1}:{2} -> {3}:{4}  pid={5}" -f $_.State, $_.LocalAddress, $_.LocalPort, $_.RemoteAddress, $_.RemotePort, $_.OwningProcess) }

# ---------------- hosts / 防火墙 ----------------
H "hosts (mxd/sdo)"
Get-Content "$env:windir\System32\drivers\etc\hosts" | Where-Object { $_ -match "mxd|sdo|221\.231" } | ForEach-Object { W $_ }

H "防火墙规则 (Maple/sdologin/sdo)"
(netsh advfirewall firewall show rule name=all) | Select-String -Pattern "Rule Name|Program:|Action:|Direction:" |
    Select-String -Pattern "Rule Name" -Context 0,0 | ForEach-Object { $_.Line }

# ---------------- sdologin 现场 ----------------
H "sdologin 目录 (最近修改的 15 个)"
Get-ChildItem "D:\dgames\MXD186NEW\Client\SDO" -Recurse -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 15 |
    ForEach-Object { W ("{0}  {1,8}  {2}" -f $_.LastWriteTime, $_.Length, $_.FullName) }

H "sdologin config/skin 指纹"
Get-ChildItem "D:\dgames\MXD186NEW\Client\SDO\sdologin\config\*.cfg",
              "D:\dgames\MXD186NEW\Client\SDO\sdologin\skin\*.zip",
              "D:\dgames\MXD186NEW\Client\SDO\temp\sdologin\config\*.cfg" |
    ForEach-Object {
        $h = (Get-FileHash $_.FullName -Algorithm MD5).Hash
        W ("{0}  {1}  {2}" -f $h, $_.Length, $_.FullName)
    }

H "sdologin 今天的日志 (最新 12 个)"
Get-ChildItem "D:\dgames\MXD186NEW\Client\SDO\sdologin\logs" -Recurse -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 12 |
    ForEach-Object { W ("{0}  {1,7}  {2}" -f $_.LastWriteTime, $_.Length, $_.Name) }

$lines | Set-Content -Path $out -Encoding UTF8
Write-Host ""
Write-Host ("[v] 已写出: " + $out) -ForegroundColor Green
Write-Host "    把 bat 和 launcher 两份 txt 都发我, 我直接 diff 出差异点"
