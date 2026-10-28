@echo off
chcp 936 >nul
setlocal
rem 快照工具: 记录当前进程/窗口/网络/sdologin 状态, 用来对比 bat 启动 vs 登录器启动
rem   snapshot_launch.bat bat        游戏窗口出来后跑一次
rem   snapshot_launch.bat launcher   用登录器启动、窗口出来后跑一次
set "TAG=%~1"
if "%TAG%"=="" set "TAG=run"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0snapshot_launch.ps1" -Tag "%TAG%"
echo.
pause
