@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /MT /W3 ^
   tools\match_test.cpp src\match.cpp src\util.cpp res\app.res ^
   /Fe:tools\match_test.exe /link gdiplus.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib
if errorlevel 1 exit /b 1
del /q *.obj 2>nul
echo OK


