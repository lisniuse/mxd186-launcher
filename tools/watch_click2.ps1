# -*- coding: utf-8 -*-
# watch_click2.ps1 —— 加料版：连"没看见的通道"一起盯
#   v2 比 v1 多盯两样：
#     ① 注册表全量：HKCU\SOFTWARE\Maple 下所有键和值（不只 name/pawd/login）
#     ② 文件写入：Client 目录里任何文件被改（排除 wz / BlackCipher / 各种 log）
#   用法：右键 watch_click2.bat -> 以管理员身份运行
#         开原版登录器 -> 登录 -> 进游戏 -> 点 BMS 窗「登录」-> 回来按任意键
param([int]$Seconds = 3600)

$ErrorActionPreference = "SilentlyContinue"
$out = Join-Path ([Environment]::GetFolderPath("Desktop")) ("click_watch2_{0}.txt" -f (Get-Date -Format "HHmmss"))
$log = New-Object System.Collections.Generic.List[string]
$CLIENT = "D:\dgames\MXD186NEW\Client"

function W($s) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $s
    $log.Add($line) | Out-Null
    Write-Host $line
}

# ---- 注册表：HKCU\SOFTWARE\Maple 全量（键\值 = 数据）
function Reg-Snapshot {
    $h = @{}
    $root = "HKCU:\SOFTWARE\Maple"
    if (-not (Test-Path $root)) { return $h }
    $keys = @($root) + @(Get-ChildItem $root -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.PSPath })
    foreach ($k in $keys) {
        $item = Get-Item -Path $k -ErrorAction SilentlyContinue
        if (-not $item) { continue }
        foreach ($n in $item.GetValueNames()) {
            $h[("{0}\{1}" -f $k, $n)] = [string]$item.GetValue($n)
        }
    }
    return $h
}

# ---- 文件：Client 下会被改的文件（排除巨量 wz 和日志）
function File-Snapshot {
    $h = @{}
    Get-ChildItem $CLIENT -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -notmatch "\\wz\\" -and
            $_.FullName -notmatch "\\BlackCipher\\" -and
            $_.FullName -notmatch "\\logs\\" -and
            $_.FullName -notmatch "\\crash\\" -and
            $_.Length -lt 20MB
        } |
        ForEach-Object { $h[$_.FullName] = $_.LastWriteTime.Ticks }
    return $h
}

Write-Host ""
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host " 加料版监视（注册表全量 + 文件写入），上限 60 分钟，q 结束 / 其它键打标记" -ForegroundColor Cyan
Write-Host " 1) 开 BMS登录器普通模式.exe -> 登录" -ForegroundColor Cyan
Write-Host " 2) 进游戏，等「mxd186 账号登录」窗" -ForegroundColor Cyan
Write-Host " 3) 点它的「登录」  <-- 关键动作" -ForegroundColor Yellow
Write-Host " 4) 点完按钮后: 在黑窗口按一下空格(会打一条“用户标记”), 再按 q 结束" -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan

W "监视开始"
$regPrev = Reg-Snapshot
$filePrev = File-Snapshot
W ("初始注册表 {0} 项 / 文件 {1} 个" -f $regPrev.Count, $filePrev.Count)
foreach ($k in $regPrev.Keys) { W ("    初始 {0} = {1}" -f $k, $regPrev[$k]) }

$sw = [Diagnostics.Stopwatch]::StartNew()
$lastBeat = 0
while ($true) {
    if ($Seconds -gt 0 -and $sw.Elapsed.TotalSeconds -ge $Seconds) { W "到时间上限，结束"; break }
    if ([Console]::KeyAvailable) {
        $k = [Console]::ReadKey($true)
        if ($k.KeyChar -eq 'q' -or $k.KeyChar -eq 'Q') { W "手动结束(q)"; break }
        W "=== 用户标记: 我刚做了个动作(按了键 '" + $k.KeyChar + "') ==="
    }

    Start-Sleep -Milliseconds 200

    $reg = Reg-Snapshot
    foreach ($k in $reg.Keys) {
        if (-not $regPrev.ContainsKey($k)) { W ("*** 注册表新增: {0} = {1}" -f $k, $reg[$k]) }
        elseif ($reg[$k] -ne $regPrev[$k]) { W ("*** 注册表变化: {0}: '{1}' -> '{2}'" -f $k, $regPrev[$k], $reg[$k]) }
    }
    foreach ($k in $regPrev.Keys) { if (-not $reg.ContainsKey($k)) { W ("*** 注册表删除: {0}" -f $k) } }
    $regPrev = $reg

    $files = File-Snapshot
    foreach ($f in $files.Keys) {
        if (-not $filePrev.ContainsKey($f)) { W ("*** 新文件: {0}" -f $f) }
        elseif ($files[$f] -ne $filePrev[$f]) { W ("*** 文件被改: {0}" -f $f) }
    }
    $filePrev = $files

    if (($sw.Elapsed.TotalSeconds - $lastBeat) -ge 10) {
        $lastBeat = $sw.Elapsed.TotalSeconds
        $ys = (Get-Process -Name MapleStory -ErrorAction SilentlyContinue | Measure-Object).Count
        $ls = (Get-Process -Name "BMS登录器普通模式" -ErrorAction SilentlyContinue | Measure-Object).Count
        W ("    在盯… {0:N0}s | 游戏进程={1} 登录器={2} | login={3}" -f $sw.Elapsed.TotalSeconds, $ys, $ls, $reg["HKCU:\SOFTWARE\Maple\login"])
    }
}

$log | Set-Content -Path $out -Encoding UTF8
Write-Host ""
Write-Host ("[v] 已写出: " + $out) -ForegroundColor Green
