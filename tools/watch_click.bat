@echo off
chcp 936 >nul
setlocal
rem ==========================================================================
rem  监视「点 BMS 登录按钮」那一刻发生了什么（注册表 / TCP 连接 / 进程）
rem  用法：右键本文件 -> 以管理员身份运行
rem        然后慢慢走：开登录器 -> 登录 -> 进游戏 -> 点 BMS 窗的「登录」
rem        完事回到黑窗口按任意键结束，桌面会生成 click_watch_*.txt
rem  时长：默认 3600 秒（60 分钟），按任意键可随时提前结束
rem ==========================================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_click.ps1" -Seconds 3600
echo.
pause
