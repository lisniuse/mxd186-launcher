@echo off
rem ===========================================================================
rem  mxd186 launcher build script  (MSVC 2022, x86 32-bit)
rem  sources stay in root, final product goes to dist\
rem ===========================================================================
setlocal

if defined VSCMD_ARG_TGT_ARCH goto :have_env
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if exist "%VCVARS%" goto :call_vcvars
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if exist "%VCVARS%" goto :call_vcvars
echo [x] vcvars32.bat not found. Install VS2022 C++ build tools.
exit /b 1

:call_vcvars
call "%VCVARS%" >nul

:have_env
cd /d "%~dp0"
if not exist dist mkdir dist

rem --- resources: template png embedded into the exe
if not exist res mkdir res
rc /nologo /fo res\app.res res\app.rc
if errorlevel 1 goto :fail

rem --- compile
cl /nologo /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /O2 /MT /W3 ^
   src\main.cpp src\config.cpp src\auth.cpp src\game.cpp src\ui.cpp src\dlg.cpp src\match.cpp src\util.cpp ^
   /Fo:dist\ ^
   /Fe:dist\mxd186Launcher.exe ^
   res\app.res ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:src\app.manifest /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" ^
   gdiplus.lib shell32.lib ws2_32.lib advapi32.lib user32.lib gdi32.lib ole32.lib comdlg32.lib
if errorlevel 1 goto :fail

rem --- runtime files that must sit next to the exe

rem --- chinese-named copy (unicode-safe, done in powershell)

echo.
echo [v] built -^> dist\mxd186Launcher.exe
del /q *.obj 2>nul
del /q dist\*.obj 2>nul
endlocal
exit /b 0

:fail
echo.
echo [x] build failed
endlocal
exit /b 1
