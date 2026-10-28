# -*- coding: utf-8 -*-
# run_frida_elevated.ps1 —— 由提权进程执行：找到（或启动）BMS登录器，然后用 frida 挂上它
# 输出写到 tools\frida_launcher.log
param(
    [string]$Target = "D:\dgames\MXD186NEW\Client\BMS登录器普通模式.exe",
    [string]$Script = "D:\dgames\MXD186NEW\Mxd186Launcher\tools\dump_8800.js",
    [string]$Log    = "D:\dgames\MXD186NEW\Mxd186Launcher\tools\frida_launcher.log"
)

$ErrorActionPreference = "Continue"
function Say($s) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $s) }

Say "elevated=$(([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))"

$proc = Get-Process | Where-Object { $_.ProcessName -eq "BMS登录器普通模式" } | Select-Object -First 1
if (-not $proc) {
    Say "登录器没在跑, 启动它: $Target"
    Start-Process -FilePath $Target | Out-Null
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $proc = Get-Process | Where-Object { $_.ProcessName -eq "BMS登录器普通模式" } | Select-Object -First 1
        if ($proc) { break }
    }
}
if (-not $proc) { Say "启动失败, 退出"; Read-Host "回车关闭"; exit 1 }

Say ("登录器 PID = {0}" -f $proc.Id)
if (Test-Path $Log) { Remove-Item $Log -Force -ErrorAction SilentlyContinue }

Say "启动 frida 挂载 ..."
$frida = (Get-Command frida -ErrorAction SilentlyContinue)
if (-not $frida) { Say "找不到 frida.exe"; Read-Host "回车关闭"; exit 1 }

Say "命令: frida -p $($proc.Id) -l $Script -o $Log"
Start-Process -FilePath $frida.Source `
    -ArgumentList @("-p", "$($proc.Id)", "-l", $Script, "-o", $Log) `
    -WindowStyle Minimized -PassThru | Out-Null

Start-Sleep -Seconds 3
if (Test-Path $Log) {
    Say "frida 已开始写日志: $Log"
} else {
    Say "日志还没出现, 可能 attach 失败"
}
Say "现在去操作：主窗登录 -> 进游戏 -> 等 BMS 窗 -> 点击「登录」"
Say "完事后回来按回车, 我会结束 frida 并汇总日志"
Read-Host "按回车结束监视" | Out-Null

Get-Process -Name "frida" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Say "frida 已停止。日志路径: $Log"
if (Test-Path $Log) {
    Say "日志行数: $((Get-Content $Log).Count)"
}
Read-Host "回车关闭本窗口" | Out-Null

