@echo off
chcp 936 >nul
setlocal
rem 加料版监视：注册表全量 + Client 文件写入 + 心跳；上限 60 分钟，任意键结束
rem 用法：右键本文件 -> 以管理员身份运行；走完流程后回到黑窗口按任意键
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_click2.ps1" -Seconds 3600
echo.
pause
