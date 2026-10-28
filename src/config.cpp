#include "config.h"
#include "util.h"
#include <vector>

static std::wstring ReadAllTextAuto(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    DWORD size = GetFileSize(h, nullptr);
    std::vector<char> buf(size + 2, 0);
    DWORD got = 0;
    ReadFile(h, buf.data(), size, &got, nullptr);
    CloseHandle(h);

    // 奇数位大量 0 => UTF-16LE; 否则按 GBK
    int zeros = 0;
    for (DWORD i = 1; i < got; i += 2)
        if (buf[i] == 0) zeros++;
    if (got > 8 && zeros > (int)(got / 4))
        return std::wstring((const wchar_t*)buf.data(), got / 2);
    return GbkToWide(std::string(buf.data(), got));
}

bool LoadPatcherInfo(const std::wstring& path, std::wstring& exe, std::wstring& ip,
                     int& port, std::wstring& dir) {
    std::wstring text = ReadAllTextAuto(path);
    if (text.empty()) return false;
    // 定长字段用空格填充, 直接按空白切词
    std::vector<std::wstring> tok;
    std::wstring cur;
    for (wchar_t c : text) {
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0) {
            if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) tok.push_back(cur);
    if (tok.size() >= 3) {
        exe = tok[0];
        ip = tok[1];
        port = _wtoi(tok[2].c_str());
        if (tok.size() >= 4) dir = tok[3];
        return true;
    }
    return false;
}

std::wstring Config::ConfigDir() {
    wchar_t buf[MAX_PATH * 2]{};
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH * 2);
    std::wstring profile = (n > 0 && n < MAX_PATH * 2) ? std::wstring(buf) : ExeDir();
    std::wstring dir = JoinPath(profile, L".Mxd186Launcher");
    CreateDirectoryW(dir.c_str(), nullptr);      // 已存在就返回 FALSE, 无所谓
    return dir;
}

// 首次运行的默认配置（GBK 写盘，带注释）
static void WriteDefaultIni(const std::wstring& path) {
    const wchar_t* tpl =
        L"[fig]\r\n"
        L"; 客户端目录（唯一必需项）—— 里面要有 MapleStory.exe\r\n"
        L"fig1=\r\n"
        L"\r\n"
        L"[bms]\r\n"
        L"gameexe=MapleStory.exe\r\n"
        L"serverip=127.0.0.1\r\n"
        L"serverport=8484\r\n"
        L"auth=mysql\r\n"
        L"authport8800=8800\r\n"
        L"waitgame=60000\r\n"
        L"killgame=1\r\n"
        L"\r\n"
        L"[mysql]\r\n"
        L"; host 写 auto 或留空 = 用主窗体里填的服务器 IP\r\n"
        L"host=auto\r\n"
        L"port=3306\r\n"
        L"user=root\r\n"
        L"password=root\r\n"
        L"db=bms\r\n";

    std::string ansi = WideToGbk(tpl);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(h, ansi.data(), (DWORD)ansi.size(), &wrote, nullptr);
    CloseHandle(h);
    LogLine(L"首次运行，已生成默认配置: " + path);
}

bool Config::Load() {
    exeDir = ExeDir();

    // 配置统一放在 %USERPROFILE%\.Mxd186Launcher\fig.ini
    iniPath = JoinPath(ConfigDir(), L"fig.ini");

    if (!FileExists(iniPath)) {
        // 老用户迁移：exe 旁边如果还有 fig.ini，直接搬过去
        std::wstring legacy = JoinPath(exeDir, L"fig.ini");
        if (FileExists(legacy)) {
            if (CopyFileW(legacy.c_str(), iniPath.c_str(), FALSE)) {
                LogLine(L"已把旧 fig.ini 迁移到: " + iniPath);
            } else {
                WriteDefaultIni(iniPath);
            }
        } else {
            WriteDefaultIni(iniPath);
        }
    }

    // 1) 先用 Patcher.info 打底(原版启动器同样读它)
    {
        std::wstring e, ip, d;
        int p = 0;
        if (LoadPatcherInfo(JoinPath(exeDir, L"Patcher.info"), e, ip, p, d)) {
            gameExe = e;
            serverIp = ip;
            if (p > 0) serverPort = p;
            if (!d.empty()) clientDir = d;
        }
    }

    // 2) fig.ini 覆盖(键名与原版一致)
    if (FileExists(iniPath)) {
        title     = ReadIniStr(iniPath, L"biaoti", L"biaoti1", title);
        autoLogin = ReadIniInt(iniPath, L"biaoti", L"自动登录", autoLogin ? 1 : 0) != 0;
        bmsMode   = ReadIniInt(iniPath, L"bms", L"bmskjfs", bmsMode);
        std::wstring f = ReadIniStr(iniPath, L"fig", L"fig1", clientDir);
        if (!f.empty()) clientDir = f;

        serverIp     = ReadIniStr(iniPath, L"bms", L"serverip", serverIp);
        serverPort   = ReadIniInt(iniPath, L"bms", L"serverport", serverPort);
        gameExe      = ReadIniStr(iniPath, L"bms", L"gameexe", gameExe);
        authMode     = ReadIniStr(iniPath, L"bms", L"auth", authMode);
        bindIp       = ReadIniInt(iniPath, L"bms", L"bindip", bindIp ? 1 : 0) != 0;
        killGame     = ReadIniInt(iniPath, L"bms", L"killgame", killGame ? 1 : 0) != 0;
        waitGame     = ReadIniInt(iniPath, L"bms", L"waitgame", waitGame);
        autoType     = ReadIniInt(iniPath, L"bms", L"autotype", autoType ? 1 : 0) != 0;
        authPort8800 = ReadIniInt(iniPath, L"bms", L"authport8800", authPort8800);
        dlgDelayMs   = ReadIniInt(iniPath, L"bms", L"dlgdelay", dlgDelayMs);
        dlgChild     = ReadIniInt(iniPath, L"bms", L"dlgchild", dlgChild ? 1 : 0) != 0;
        website      = ReadIniStr(iniPath, L"bms", L"website", website);
        dbHost       = ReadIniStr(iniPath, L"mysql", L"host", dbHost);
        dbPort       = ReadIniInt(iniPath, L"mysql", L"port", dbPort);
        dbUser       = ReadIniStr(iniPath, L"mysql", L"user", dbUser);
        dbPass       = ReadIniStr(iniPath, L"mysql", L"password", dbPass);
        dbName       = ReadIniStr(iniPath, L"mysql", L"db", dbName);
        dbSql        = ReadIniStr(iniPath, L"mysql", L"sql", dbSql);
    }

    // 3) 客户端目录缺省 = 本程序所在目录
    if (clientDir.empty()) clientDir = exeDir;

    // 4) 记住的账号(原版 HKCU\SOFTWARE\Maple)
    savedAccount  = RegReadStr(HKEY_CURRENT_USER, L"SOFTWARE\\Maple", L"name");
    savedPassword = RegReadStr(HKEY_CURRENT_USER, L"SOFTWARE\\Maple", L"pawd");
    remember      = !savedAccount.empty();
    // 3) 客户端目录兜底：ini 没写或写的路径不存在 → 自动探测（支持 dist 只放一个 exe）
    if (!FileExists(JoinPath(clientDir, gameExe))) {
        const std::wstring cand[] = {
            JoinPath(exeDir, L"Client"),                        // exe 同目录下的 Client
            JoinPath(JoinPath(exeDir, L".."), L"Client"),       // 上一级（dist\..\Client）
            JoinPath(JoinPath(JoinPath(exeDir, L".."), L".."), L"Client"),  // 上两级
            exeDir,                                             // 就放在客户端目录里
        };
        for (const auto& c : cand) {
            if (FileExists(JoinPath(c, gameExe))) {
                clientDir = c;
                break;
            }
        }
    }

    return true;
}

bool Config::Save() {
    // 只写"真正会用到"的键；其余键（bindip/autotype/dlgdelay/dlgchild/website/标语/sql）
    // 仍可手工写进 ini 生效，但不再自动写回，避免配置文件越长越乱。
    // iniPath 在 Load() 里已经算好（exe 同目录的 fig.ini），这里直接用

    WriteIniStr(iniPath, L"fig", L"fig1", clientDir);        // 客户端目录（必需）
    WriteIniStr(iniPath, L"bms", L"gameexe", gameExe);      // 客户端 exe 名
    WriteIniStr(iniPath, L"bms", L"serverip", serverIp);    // 主窗默认 IP
    WriteIniInt(iniPath, L"bms", L"serverport", serverPort);
    WriteIniStr(iniPath, L"bms", L"auth", authMode);
    WriteIniInt(iniPath, L"bms", L"authport8800", authPort8800);
    WriteIniInt(iniPath, L"bms", L"waitgame", waitGame);
    WriteIniInt(iniPath, L"bms", L"killgame", killGame ? 1 : 0);

    WriteIniStr(iniPath, L"mysql", L"host", dbHost);
    WriteIniInt(iniPath, L"mysql", L"port", dbPort);
    WriteIniStr(iniPath, L"mysql", L"user", dbUser);
    WriteIniStr(iniPath, L"mysql", L"password", dbPass);
    WriteIniStr(iniPath, L"mysql", L"db", dbName);
    return true;
}

void Config::Log() const {
    LogLine(Format(L"配置: dlgchild=%d dlgdelay=%d autotype=%d authport8800=%d",
                   dlgChild ? 1 : 0, dlgDelayMs, autoType ? 1 : 0, authPort8800));
    LogLine(Format(L"配置: title=%s clientDir=%s game=%s ip=%s port=%d auth=%s bindip=%d "
                   L"bms=%d autologin=%d remember=%d",
                   title.c_str(), clientDir.c_str(), gameExe.c_str(), serverIp.c_str(),
                   serverPort, authMode.c_str(), bindIp ? 1 : 0, bmsMode,
                   autoLogin ? 1 : 0, remember ? 1 : 0));
}



