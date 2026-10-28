// util.h —— 通用小工具: 编码转换 / ini / 日志 / 路径
#pragma once
#include <windows.h>
#include <string>

std::wstring GbkToWide(const std::string& s);
std::string  WideToGbk(const std::wstring& s);
std::wstring ExeDir();
std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
bool         FileExists(const std::wstring& path);

void         LogLine(const std::wstring& msg);
std::wstring LogPath();

std::wstring ReadIniStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key,
                        const std::wstring& def);
int          ReadIniInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int def);
void         WriteIniStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key,
                         const std::wstring& val);
void         WriteIniInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int val);

std::wstring RegReadStr(HKEY root, const wchar_t* sub, const wchar_t* name,
                        const std::wstring& def = L"");
bool         RegWriteStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& val);

std::wstring Format(const wchar_t* fmt, ...);

// 把焦点交给某个窗口后, 用 SendInput 逐字符敲进去(不注入)
bool TypeIntoWindow(HWND target, const std::wstring& text, int delayMs = 40);
bool SendVk(WORD vk, int delayMs = 40);
