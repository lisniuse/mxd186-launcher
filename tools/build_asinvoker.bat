@echo off
rem 只为截图：用 asInvoker 清单编译一份临时 exe（不影响正式产物）
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d D:\dgames\MXD186NEW\Mxd186Launcher
cl /nologo /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /O2 /MT /W3 ^
   src\main.cpp src\config.cpp src\auth.cpp src\game.cpp src\ui.cpp src\dlg.cpp src\match.cpp src\util.cpp ^
   /Fe:_shot_test.exe ^
   /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='asInvoker' uiAccess='false'" ^
   gdiplus.lib shell32.lib ws2_32.lib advapi32.lib user32.lib gdi32.lib ole32.lib comdlg32.lib
if errorlevel 1 exit /b 1
del /q *.obj 2>nul
echo OK



