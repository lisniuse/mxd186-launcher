// main.cpp —— 入口: 单实例 + GDI+ 初始化 + 读配置 + 起窗口
#include "ui.h"
#include "util.h"

#include <gdiplus.h>
#include <objbase.h>
#include "dlg.h"

#pragma comment(lib, "gdiplus.lib")

namespace {

bool IsElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION el{};
    DWORD cb = sizeof(el);
    bool elevated = GetTokenInformation(tok, TokenElevation, &el, cb, &cb) && el.TokenIsElevated;
    CloseHandle(tok);
    return elevated;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR cmdline, int) {
    // 调试用: /dlg 直接单开「mxd186 账号登录」第二窗体(不等游戏)
    bool onlyDialog = cmdline && wcsstr(cmdline, L"/dlg") != nullptr;

    // 单实例: 原版也不让同时开两个登录器
    HANDLE once = onlyDialog ? nullptr
                             : CreateMutexW(nullptr, TRUE, L"Mxd186Launcher_SingleInstance");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND h = FindWindowW(L"Mxd186LauncherClass", nullptr);
        if (h) {
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        }
        return 0;
    }

    SetProcessDPIAware();

    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &gsi, nullptr) != Gdiplus::Ok) {
        MessageBoxW(nullptr, L"GDI+ 初始化失败", L"错误", MB_ICONERROR);
        return 1;
    }

    Config cfg;
    cfg.Load();
    cfg.Log();

    if (cfg.bindIp && !IsElevated())
        LogLine(L"警告: 开了 bindip 但当前不是管理员, netsh 绑定会失败");

    int rc = 0;
    if (onlyDialog) {
        // 只开第二窗体, 方便单独看效果(不需要真启动游戏)
        LoginDialog dlg(cfg, nullptr);
        if (!dlg.Create(inst)) {
            MessageBoxW(nullptr, L"创建登录窗体失败", L"错误", MB_ICONERROR);
            rc = 3;
        } else {
            MSG msg;
            while (GetMessageW(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            rc = (int)msg.wParam;
        }
    } else {
        LauncherUI ui(cfg);
        if (!ui.Create(inst)) {
            MessageBoxW(nullptr, L"创建窗口失败", L"错误", MB_ICONERROR);
            rc = 2;
        } else {
            rc = ui.Run();
        }
    }

    Gdiplus::GdiplusShutdown(token);
    if (once) CloseHandle(once);
    return rc;
}

