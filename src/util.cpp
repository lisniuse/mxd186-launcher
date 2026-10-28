#include "util.h"
#include <cstdarg>
#include <cstdio>
#include <vector>

std::wstring GbkToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(936, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(936, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string WideToGbk(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(936, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string a(n, '\0');
    WideCharToMultiByte(936, 0, s.data(), (int)s.size(), a.data(), n, nullptr, nullptr);
    return a;
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 2] = {0};
    GetModuleFileNameW(nullptr, buf, _countof(buf));
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    wchar_t last = a[a.size() - 1];
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring LogPath() {
    // 日志放在用户配置目录，跟 fig.ini 同一个地方（exe 目录保持干净）
    wchar_t buf[MAX_PATH * 2]{};
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH * 2);
    std::wstring profile = (n > 0 && n < MAX_PATH * 2) ? std::wstring(buf) : ExeDir();
    std::wstring dir = JoinPath(profile, L".Mxd186Launcher");
    CreateDirectoryW(dir.c_str(), nullptr);      // 已存在则忽略
    return JoinPath(dir, L"launcher.log");
}

void LogLine(const std::wstring& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::wstring line = Format(L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",
                               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                               msg.c_str());
    std::string gbk = WideToGbk(line);
    HANDLE h = CreateFileW(LogPath().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(h, gbk.data(), (DWORD)gbk.size(), &written, nullptr);
    CloseHandle(h);
}

// fig.ini 是 GBK 编码(和原版一致), 所以读写都走 A 版 API + 936 转换
std::wstring ReadIniStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key,
                        const std::wstring& def) {
    char buf[4096] = {0};
    GetPrivateProfileStringA(WideToGbk(sec).c_str(), WideToGbk(key).c_str(),
                             WideToGbk(def).c_str(), buf, _countof(buf),
                             WideToGbk(ini).c_str());
    return GbkToWide(buf);
}

int ReadIniInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int def) {
    return (int)GetPrivateProfileIntA(WideToGbk(sec).c_str(), WideToGbk(key).c_str(), def,
                                      WideToGbk(ini).c_str());
}

void WriteIniStr(const std::wstring& ini, const wchar_t* sec, const wchar_t* key,
                 const std::wstring& val) {
    WritePrivateProfileStringA(WideToGbk(sec).c_str(), WideToGbk(key).c_str(),
                               WideToGbk(val).c_str(), WideToGbk(ini).c_str());
}

void WriteIniInt(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int val) {
    std::string v = std::to_string(val);
    WritePrivateProfileStringA(WideToGbk(sec).c_str(), WideToGbk(key).c_str(), v.c_str(),
                               WideToGbk(ini).c_str());
}

std::wstring RegReadStr(HKEY root, const wchar_t* sub, const wchar_t* name,
                        const std::wstring& def) {
    HKEY k = nullptr;
    std::wstring out = def;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        wchar_t buf[1024] = {0};
        DWORD cb = sizeof(buf) - sizeof(wchar_t);
        DWORD type = 0;
        if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &cb) == ERROR_SUCCESS)
            out = buf;
        RegCloseKey(k);
    }
    return out;
}

bool RegWriteStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& val) {
    HKEY k = nullptr;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(),
                            (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

std::wstring Format(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    wchar_t buf[2048];
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    return std::wstring(buf);
}


// ---------------------------------------------------------------- SendInput 打字
bool SendVk(WORD vk, int delayMs) {
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(2, in, sizeof(INPUT)) != 2) return false;
    if (delayMs > 0) Sleep(delayMs);
    return true;
}

bool TypeIntoWindow(HWND target, const std::wstring& text, int delayMs) {
    if (!target || !IsWindow(target)) return false;
    ShowWindow(target, SW_SHOW);
    SetForegroundWindow(target);
    Sleep(600);   // 等它拿到焦点/现场

    for (wchar_t ch : text) {
        INPUT in[2] = {};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = ch;
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;
        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        if (SendInput(2, in, sizeof(INPUT)) != 2) return false;
        Sleep(delayMs > 0 ? delayMs : 30);
    }
    return true;
}
