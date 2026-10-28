#include "game.h"
#include "util.h"
#include <tlhelp32.h>
#include <vector>

namespace {

// 跑一条命令, 等它结束(隐藏窗口), 返回退出码
DWORD RunHidden(const std::wstring& cmdline, DWORD timeoutMs = 5000) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdline;
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi))
        return (DWORD)-1;
    WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

}  // namespace

std::wstring BuildGameCmdline(const Config& cfg) {
    // 原版字符串: "MapleStory.exe 221.231.130.70 8484"
    return Format(L"\"%s\" %s %d", cfg.gameExe.c_str(), cfg.serverIp.c_str(), cfg.serverPort);
}

bool IsGameRunning(const std::wstring& exeName, DWORD* pidOut) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    std::wstring want = exeName;
    for (auto& c : want) c = towlower(c);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring cur = pe.szExeFile;
            for (auto& c : cur) c = towlower(c);
            if (cur == want) {
                found = true;
                if (pidOut) *pidOut = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

bool BindFakeIp(const std::wstring& ip) {
    std::wstring cmd = Format(L"netsh int ip add addr 1 %s", ip.c_str());
    DWORD rc = RunHidden(cmd, 4000);
    LogLine(Format(L"netsh 绑定假 IP: %s -> rc=%lu", cmd.c_str(), rc));
    // 已经绑过会返回非 0, 不算致命错误
    return true;
}

void UnbindFakeIp(const std::wstring& ip) {
    std::wstring cmd = Format(L"netsh int ip delete addr 1 %s", ip.c_str());
    RunHidden(cmd, 4000);
    LogLine(Format(L"netsh 解绑假 IP: %s", cmd.c_str()));
}

bool LaunchGame(const Config& cfg, GameSession& out, std::wstring& err) {
    std::wstring exeFull = JoinPath(cfg.clientDir, cfg.gameExe);
    if (!FileExists(exeFull)) {
        err = L"找不到游戏主程序：\n" + exeFull +
              L"\n\n请检查 fig.ini 的 [fig] fig1 是否指向客户端目录。";
        return false;
    }

    if (IsGameRunning(cfg.gameExe)) {
        err = L"请勿多次启动游戏!";
        return false;
    }

    if (cfg.bindIp) BindFakeIp(cfg.serverIp);

    std::wstring cmd = BuildGameCmdline(cfg);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);

    // 工作目录必须是客户端目录, 否则 WZ 一个都读不到
    if (!CreateProcessW(exeFull.c_str(), buf.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, cfg.clientDir.c_str(), &si, &pi)) {
        err = Format(L"启动游戏失败，GetLastError=%lu", GetLastError());
        return false;
    }

    out.pi = pi;
    out.running = true;
    LogLine(Format(L"已启动: %s (pid=%lu, cwd=%s)", cmd.c_str(), pi.dwProcessId,
                   cfg.clientDir.c_str()));
    return true;
}

namespace {

// 按窗口类名精确找顶层窗口（比 FindWindowW 稳，能处理同名/隐藏窗口）
HWND FindTopWindowByClass(const wchar_t* cls) {
    struct Ctx {
        const wchar_t* cls;
        HWND found;
    } ctx{cls, nullptr};

    EnumWindows(
        [](HWND h, LPARAM l) -> BOOL {
            Ctx* c = (Ctx*)l;
            wchar_t buf[128] = {0};
            GetClassNameW(h, buf, _countof(buf));
            if (wcscmp(buf, c->cls) == 0) {
                c->found = h;
                return FALSE;
            }
            return TRUE;
        },
        (LPARAM)&ctx);
    return ctx.found;
}

}  // namespace

HWND WaitGameWindow(const wchar_t* cls, int timeoutMs, HANDLE proc) {
    const DWORD step = 100;
    DWORD waited = 0;
    HWND closedStartup = nullptr;

    while (waited < (DWORD)timeoutMs) {
        // 原版行为(0x403A7A 那段): 等 StartUpDlgClass(启动页) 出现, 一出现就给它 WM_CLOSE 关掉
        HWND dlg = FindTopWindowByClass(L"StartUpDlgClass");
        if (dlg && dlg != closedStartup) {
            PostMessageW(dlg, WM_CLOSE, 0, 0);
            LogLine(L"跳过启动页: WM_CLOSE -> StartUpDlgClass");
            closedStartup = dlg;
        }

        HWND h = FindTopWindowByClass(cls);
        if (h) return h;
        if (proc && WaitForSingleObject(proc, 0) == WAIT_OBJECT_0) return nullptr;  // 游戏自己退了
        Sleep(step);
        waited += step;
    }
    return nullptr;
}

void StopGame(GameSession& s) {
    if (!s.running) return;
    if (s.window && IsWindow(s.window)) PostMessageW(s.window, WM_CLOSE, 0, 0);
    if (s.pi.hProcess) {
        if (WaitForSingleObject(s.pi.hProcess, 3000) != WAIT_OBJECT_0)
            TerminateProcess(s.pi.hProcess, 0);
        CloseHandle(s.pi.hProcess);
        CloseHandle(s.pi.hThread);
        s.pi = {};
    }
    s.running = false;
    s.window = nullptr;
}
