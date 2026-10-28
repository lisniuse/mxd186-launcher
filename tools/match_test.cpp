#include "../src/match.h"
#include <cstdio>
#include <cstdlib>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// 用法:
//   match_test -res             只验证「从 exe 资源载入模板」
//   match_test -res <hwnd>      资源模板 + 对该窗口匹配
//   match_test <tpl.png> <hwnd> 文件模板 + 匹配
int wmain(int argc, wchar_t** argv) {
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &gsi, nullptr);

    ScreenMatcher m;
    int rc = 0;

    if (argc >= 2 && wcscmp(argv[1], L"-res") == 0) {
        const bool ok = m.LoadTemplateFromResource(101);
        wprintf(L"resource template loaded = %d\n", ok ? 1 : 0);
        if (!ok) {
            rc = 2;
        } else if (argc >= 3) {
            HWND h = (HWND)_wtoi64(argv[2]);
            double score = -1;
            POINT pos{};
            const bool hit = m.MatchGameWindow(h, score, &pos);
            wprintf(L"hit=%d ncc=%.3f pos=(%d,%d)\n", hit ? 1 : 0, score, pos.x, pos.y);
        }
    } else if (argc >= 3) {
        if (!m.LoadTemplate(argv[1])) {
            rc = 2;
        } else {
            HWND h = (HWND)_wtoi64(argv[2]);
            double score = -1;
            POINT pos{};
            const bool hit = m.MatchGameWindow(h, score, &pos);
            wprintf(L"hit=%d ncc=%.3f pos=(%d,%d)\n", hit ? 1 : 0, score, pos.x, pos.y);
        }
    } else {
        wprintf(L"usage: match_test -res [hwnd] | match_test <tpl.png> <hwnd>\n");
        rc = 1;
    }

    Gdiplus::GdiplusShutdown(token);
    return rc;
}
