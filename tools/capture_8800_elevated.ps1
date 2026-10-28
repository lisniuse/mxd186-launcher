# -*- coding: utf-8 -*-
# capture_8800_elevated.ps1 —— 用 Windows 自带 pktmon 抓 8800 端口的本地回环流量
#   不进目标进程、不注入，抓完输出 pcap，供后面解析
param(
    [string]$Etl  = "D:\dgames\MXD186NEW\Mxd186Launcher\tools\cap8800b.etl",
    [string]$Pcap = "D:\dgames\MXD186NEW\Mxd186Launcher\tools\cap8800b.pcap",
    [int]$Seconds = 300
)

function Say($s) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $s) }
Say "elevated=$(([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))"

Remove-Item $Etl, $Pcap -Force -ErrorAction SilentlyContinue

Say "清理旧过滤器"
& pktmon filter remove | Out-Null
Say "加过滤器: 端口 8800"
& pktmon filter add -p 8800 | Out-Null

Say "开始抓包 -> $Etl"
& pktmon start --capture --pkt-size 0 -f $Etl | Out-Null
if (-not (Test-Path $Etl)) { Say "抓包启动失败"; Read-Host "回车关闭"; exit 1 }

Write-Host ""
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host " 正在抓 8800 端口。现在去做：" -ForegroundColor Cyan
Write-Host "   1) 登录器主窗 -> 开始游戏（如果游戏已经开着，跳过）" -ForegroundColor Cyan
Write-Host "   2) 等「mxd186 账号登录」窗" -ForegroundColor Cyan
Write-Host "   3) 点它的「登录」   <-- 就抓这一下" -ForegroundColor Yellow
Write-Host " 抓满 $Seconds 秒会自动停止（也可以按回车提前停）" -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan

$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
    Start-Sleep -Milliseconds 500
    if ([Console]::KeyAvailable) { [void][Console]::ReadKey($true); Say "手动提前停止"; break }
    if (([int]$sw.Elapsed.TotalSeconds % 15) -eq 0) {
        $n = (Get-Process -Name MapleStory -ErrorAction SilentlyContinue | Measure-Object).Count
        Say ("抓包中… {0:N0}s | MapleStory 进程={1}" -f $sw.Elapsed.TotalSeconds, $n)
        Start-Sleep -Milliseconds 1000
    }
}

Say "停止抓包"
& pktmon stop | Out-Null
Say "转成 pcap"
& pktmon etl2pcap $Etl -o $Pcap | Out-Null
if (Test-Path $Pcap) {
    Say ("pcap 已生成: {0} ({1} 字节)" -f $Pcap, (Get-Item $Pcap).Length)
} else {
    Say "etl2pcap 没成功，改出文本"
    & pktmon etl2txt $Etl -o "D:\dgames\MXD186NEW\Mxd186Launcher\tools\cap8800.txt" | Out-Null
    if (Test-Path "D:\dgames\MXD186NEW\Mxd186Launcher\tools\cap8800.txt") { Say "已生成 cap8800.txt" }
}
& pktmon filter remove | Out-Null
Say "完成"
Read-Host "回车关闭本窗口" | Out-Null

