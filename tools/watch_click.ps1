# -*- coding: utf-8 -*-
# watch_click.ps1 —— 长时段监视「整条流程」：登录器 -> 进游戏 -> 点 BMS 登录
#   监视项: HKCU\SOFTWARE\Maple 三个值的变化 / 相关进程的连接 / 进程增减
#   用法:  右键 watch_click.bat -> 以管理员身份运行
#          然后慢慢来: 开登录器 -> 登录 -> 进游戏 -> 点 BMS 窗的「登录」
#          完事按任意键结束（不按也会一直在，默认 15 分钟）
param([int]$Seconds = 900)

$ErrorActionPreference = "SilentlyContinue"
$out = Join-Path ([Environment]::GetFolderPath("Desktop")) ("click_watch_{0}.txt" -f (Get-Date -Format "HHmmss"))
$log = New-Object System.Collections.Generic.List[string]

function W($s) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $s
    $log.Add($line) | Out-Null
    Write-Host $line
}

function Reg-Snapshot {
    $r = @{}
    foreach ($k in @("name", "pawd", "login")) {
        $v = (Get-ItemProperty -Path "HKCU:\SOFTWARE\Maple" -Name $k -ErrorAction SilentlyContinue).$k
        $r[$k] = if ($null -eq $v) { "<none>" } else { [string]$v }
    }
    return $r
}

function ProcNames {
    $names = @{}
    Get-Process -ErrorAction SilentlyContinue | ForEach-Object { $names[[uint32]$_.Id] = $_.ProcessName }
    return $names
}

function Conn-Snapshot($names) {
    $set = @{}
    foreach ($c in Get-NetTCPConnection -ErrorAction SilentlyContinue) {
        $pn = $names[[uint32]$c.OwningProcess]
        if ($pn -match "BMS|MapleStory|sdologin|wke") {
            $set[("{0}({1}) {2}:{3} -> {4}:{5}" -f $pn, $c.OwningProcess, $c.LocalAddress, $c.LocalPort, $c.RemoteAddress, $c.RemotePort)] = $true
        }
    }
    return $set
}

function Proc-Snapshot {
    $set = @{}
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match "BMS|MapleStory|sdologin|wke" } |
        ForEach-Object { $set["{0}({1})" -f $_.ProcessName, $_.Id] = $true }
    return $set
}

Write-Host ""
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host " 监视中：上限 60 分钟，随时按任意键结束并写文件" -ForegroundColor Cyan
Write-Host " 现在按顺序做：" -ForegroundColor Cyan
Write-Host "   1) 开 BMS登录器普通模式.exe -> 登录" -ForegroundColor Cyan
Write-Host "   2) 等游戏起来（启动页会被关掉）" -ForegroundColor Cyan
Write-Host "   3) 等「mxd186 账号登录」窗出现" -ForegroundColor Cyan
Write-Host "   4) 在它上面点「登录」  <-- 关键动作" -ForegroundColor Yellow
Write-Host "   5) 回来按任意键" -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host ""

W ("开始监视，上限 {0} 秒" -f $Seconds)

$regPrev = Reg-Snapshot
$connPrev = Conn-Snapshot (ProcNames)
$procPrev = Proc-Snapshot
W ("初始注册表: name={0} pawd={1} login={2}" -f $regPrev["name"], $regPrev["pawd"], $regPrev["login"])
W ("初始相关进程: " + (($procPrev.Keys) -join ", "))

$sw = [Diagnostics.Stopwatch]::StartNew()
$lastBeat = 0
while ($true) {
    if ($Seconds -gt 0 -and $sw.Elapsed.TotalSeconds -ge $Seconds) { W "到达时间上限，结束"; break }
    if ([Console]::KeyAvailable) { [void][Console]::ReadKey($true); W "手动结束"; break }

    Start-Sleep -Milliseconds 200

    $reg = Reg-Snapshot
    foreach ($k in @("name", "pawd", "login")) {
        if ($reg[$k] -ne $regPrev[$k]) {
            W ("*** 注册表变化: {0}: '{1}' -> '{2}'" -f $k, $regPrev[$k], $reg[$k])
        }
    }
    $regPrev = $reg

    $names = ProcNames
    $conn = Conn-Snapshot $names
    foreach ($c in $conn.Keys) { if (-not $connPrev.ContainsKey($c)) { W ("*** 新连接: " + $c) } }
    foreach ($c in $connPrev.Keys) { if (-not $conn.ContainsKey($c)) { W ("    断开: " + $c) } }
    $connPrev = $conn

    $proc = Proc-Snapshot
    foreach ($p in $proc.Keys) { if (-not $procPrev.ContainsKey($p)) { W ("*** 进程出现: " + $p) } }
    foreach ($p in $procPrev.Keys) { if (-not $proc.ContainsKey($p)) { W ("    进程退出: " + $p) } }
    $procPrev = $proc

    if (($sw.Elapsed.TotalSeconds - $lastBeat) -ge 10) {
        $lastBeat = $sw.Elapsed.TotalSeconds
        W ("    在盯… {0:N0}s | 进程: {1} | login={2}" -f $sw.Elapsed.TotalSeconds,
            (($proc.Keys) -join ","), $reg["login"])
    }
}

$log | Set-Content -Path $out -Encoding UTF8
Write-Host ""
Write-Host ("[v] 已写出: " + $out) -ForegroundColor Green
Write-Host "    把这个 txt 发我（带 *** 的行就是关键动作）" -ForegroundColor Green
