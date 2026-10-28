#include "ui.h"

#include <commdlg.h>
#include "auth.h"
#include "match.h"
#include "util.h"

#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>
#include <thread>
#include <memory>
#include <string>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

namespace {

// 简单 IPv4 校验：四段 0-255，只允许数字和点
bool IsIpv4(const std::wstring& s) {
    if (s.empty()) return false;
    int parts = 0, val = -1;
    for (wchar_t c : s) {
        if (c >= L'0' && c <= L'9') {
            val = (val < 0 ? 0 : val) * 10 + (c - L'0');
            if (val > 255) return false;
        } else if (c == L'.') {
            if (val < 0) return false;
            ++parts; val = -1;
        } else {
            return false;
        }
    }
    return parts == 3 && val >= 0;
}

}  // namespace

namespace {

const wchar_t* kClassName = L"Mxd186LauncherClass";

// 配色(照皮肤登录器那套: 深蓝底 + 亮蓝按钮)
const Color kInk     (255,  17,  17,  17);   // 硬黑（描边/文字/阴影）
const Color kPaper   (255, 255, 243, 214);   // 米黄纸底
const Color kPaperHi (255, 255, 251, 235);
const Color kWhite   (255, 255, 255, 255);
const Color kPink    (255, 255, 122, 168);   // 标题色块
const Color kYellow  (255, 255, 201,  60);   // 主按钮
const Color kYellowHi(255, 255, 214, 102);
const Color kGreen   (255,  47, 191, 113);
const Color kRed     (255, 224,  62,  62);
const Color kMuted   (255, 110, 102,  90);

// 粗野主义基本件：填充 + 粗黑边 + 硬阴影（阴影不模糊，右下方偏移）
void BrutalBox(Graphics& g, const RECT& r, const Color& fill, int shadow = 5, float border = 3.0f) {
    if (shadow > 0) {
        SolidBrush sh(kInk);
        g.FillRectangle(&sh, (REAL)r.left + shadow, (REAL)r.top + shadow,
                        (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    }
    SolidBrush b(fill);
    g.FillRectangle(&b, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                    (REAL)(r.bottom - r.top));
    Pen p(kInk, border);
    g.DrawRectangle(&p, (REAL)r.left + border / 2, (REAL)r.top + border / 2,
                    (REAL)(r.right - r.left - border), (REAL)(r.bottom - r.top - border));
}

void RoundRect(GraphicsPath& p, const RectF& r, float rad) {
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

RectF ToRectF(const RECT& r) {
    return RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
}

void DrawText(Graphics& g, const std::wstring& s, const RECT& r, float sizePx, const Color& c,
              int align = StringAlignmentCenter, bool bold = false, bool noWrap = false) {
    FontFamily fam(L"Microsoft YaHei UI");
    Font font(&fam, sizePx, FontStyleBold, UnitPixel);   // 粗野主义：一律加粗
    SolidBrush br(c);
    StringFormat fmt;
    fmt.SetAlignment((StringAlignment)align);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    if (noWrap) fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    RectF rf = ToRectF(r);
    g.DrawString(s.c_str(), (INT)s.size(), &font, rf, &fmt, &br);
}

}  // namespace

// ---------------------------------------------------------------- 窗口
bool LauncherUI::Create(HINSTANCE inst) {
    m_inst = inst;
    InitLayout();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcThunk;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int x = (sw - m_width) / 2, y = (sh - m_height) / 2;

    m_hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_LAYERED, kClassName, L"mxd186 launcher",
                             WS_POPUP, x, y, m_width, m_height, nullptr, nullptr, inst, this);
    if (!m_hwnd) return false;

    m_remember = !m_cfg.savedAccount.empty();
    m_autoLogin = m_cfg.autoLogin;
    m_serverIp = m_cfg.serverIp;        // 主窗只关心服务器地址
    m_clientExe = JoinPath(m_cfg.clientDir, m_cfg.gameExe);
    m_status = Format(L"端口 %d  ·  校验方式 %s", m_cfg.serverPort,
                      AuthModeText(m_cfg.authMode));

    // 分层窗口必须先 UpdateLayeredWindow 再 ShowWindow:
    // 只等 WM_PAINT 的话首帧永远不会来, 窗口"可见"但完全透明 —— 任务栏有图标、屏幕上什么都没有。
    Repaint();
    ShowWindow(m_hwnd, SW_SHOW);
    Repaint();

    // 别让窗口一出来就被浏览器/聊天窗压住
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    SetTimer(m_hwnd, 1, 500, nullptr);

    return true;
}

int LauncherUI::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK LauncherUI::WndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    LauncherUI* self = nullptr;
    if (m == WM_NCCREATE) {
        auto cs = (CREATESTRUCTW*)l;
        self = (LauncherUI*)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = h;
    } else {
        self = (LauncherUI*)GetWindowLongPtrW(h, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(h, m, w, l);
    return self->Handle(m, w, l);
}

void LauncherUI::InitLayout() {
    ZeroMemory(m_rc, sizeof(m_rc));
    m_rc[HIT_TITLE]     = {0, 0, m_width - 40, 46};
    m_rc[HIT_CLOSE]     = {m_width - 34, 12, m_width - 14, 32};
    m_rc[HIT_ACCOUNT]   = {120, 92, 400, 134};      // 服务器 IP
    m_rc[HIT_CLIENT]    = {120, 176, 340, 218};     // 客户端 exe 路径（只读）
    m_rc[HIT_BROWSE]    = {348, 176, 400, 218};     // 浏览…
    m_rc[HIT_PASSWORD]  = {0, 0, 0, 0};
    m_rc[HIT_REMEMBER]  = {0, 0, 0, 0};
    m_rc[HIT_AUTOLOGIN] = {0, 0, 0, 0};
    m_rc[HIT_START]     = {120, 258, 300, 302};     // 开始游戏
    m_rc[HIT_OPENCFG]   = {308, 258, 400, 302};     // 打开配置文件
    m_rc[HIT_REGISTER]  = {0, 0, 0, 0};
    m_rc[HIT_CHANGEPWD] = {0, 0, 0, 0};
    m_rc[HIT_WEBSITE]   = {0, 0, 0, 0};
    m_rc[HIT_QUIT]      = {0, 0, 0, 0};
}

LauncherUI::Hit LauncherUI::HitTest(int x, int y) const {
    for (int i = HIT_TITLE; i < HIT_COUNT; ++i) {
        const RECT& r = m_rc[i];
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return (Hit)i;
    }
    return HIT_NONE;
}

// ---------------------------------------------------------------- 绘制
void LauncherUI::Draw(HDC dc) {
    Bitmap bmp(m_width, m_height, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    DrawBackground(g);
    DrawTitleBar(g);

    RECT lab{120, 54, 400, 86};
    DrawText(g, L"服务器 IP", lab, 14.0f, kInk, StringAlignmentNear, true);
    DrawField(g, m_rc[HIT_ACCOUNT], m_serverIp, L"例如 221.231.130.70", m_focus == 1, false);

    RECT lab2{120, 140, 400, 172};
    DrawText(g, L"游戏客户端", lab2, 14.0f, kInk, StringAlignmentNear, true);
    // 路径太长时保留尾部（文件名最关键）
    std::wstring shownExe = m_clientExe;
    if (shownExe.size() > 30) shownExe = L"…" + shownExe.substr(shownExe.size() - 28);
    DrawField(g, m_rc[HIT_CLIENT], shownExe, L"点右侧「浏览」选择 MapleStory.exe", false, false);
    DrawButton(g, m_rc[HIT_BROWSE], L"浏览", HIT_BROWSE, false);

    RECT tip{120, 226, 400, 248};
    DrawText(g, m_status.empty() ? Format(L"> 端口 %d", m_cfg.serverPort) : m_status, tip,
             12.0f, kMuted, StringAlignmentNear, true, true);

    DrawButton(g, m_rc[HIT_START], m_busy ? L"正在启动…" : L"开始游戏", HIT_START, true);
    DrawButton(g, m_rc[HIT_OPENCFG], L"打开配置文件", HIT_OPENCFG, false);

    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(Color(0, 0, 0, 0), &hbm);
    if (!hbm) return;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, hbm);

    if (m_layered) {
        RECT wr;
        GetWindowRect(m_hwnd, &wr);
        POINT dst{wr.left, wr.top};
        SIZE sz{m_width, m_height};
        POINT src{0, 0};
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(m_hwnd, screen, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA)) {
            // 某些环境(远程桌面/合成器异常)会失败, 退回普通窗口绘制, 绝不留下一个看不见的窗口
            LogLine(Format(L"UpdateLayeredWindow 失败(err=%lu)，退回普通窗口绘制", GetLastError()));
            m_layered = false;
            LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
            SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
            RedrawWindow(m_hwnd, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
        }
    } else {
        // 普通窗口: 直接画到窗口 DC(圆角处会露出底色, 只作兜底)
        RECT cr{0, 0, m_width, m_height};
        HBRUSH bg = CreateSolidBrush(RGB(14, 21, 38));
        FillRect(dc, &cr, bg);
        DeleteObject(bg);
        BitBlt(dc, 0, 0, m_width, m_height, mem, 0, 0, SRCCOPY);
    }

    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    DeleteObject(hbm);
}

void LauncherUI::DrawBackground(Graphics& g) {
    SolidBrush paper(kPaper);
    g.FillRectangle(&paper, 0, 0, m_width, m_height);

    // 标题区撞色块
    SolidBrush band(kPink);
    g.FillRectangle(&band, 0, 0, m_width, 46);

    Pen ink(kInk, 4.0f);
    g.DrawRectangle(&ink, 2.0f, 2.0f, (REAL)m_width - 4, (REAL)m_height - 4);

    Pen sep(kInk, 3.0f);
    g.DrawLine(&sep, 0.0f, 46.0f, (REAL)m_width, 46.0f);
}

void LauncherUI::DrawTitleBar(Graphics& g) {
    RECT r{18, 0, m_width - 60, 46};
    DrawText(g, L"mxd186 登录器", r, 18.0f, kInk, StringAlignmentNear, true);

    // 关闭按钮：方形 + 粗黑边 + 硬阴影，hover 反色
    RECT c = m_rc[HIT_CLOSE];
    bool hot = (m_hover == HIT_CLOSE);
    BrutalBox(g, c, hot ? kYellow : kWhite, 3, 2.5f);
    Pen pen(kInk, 2.6f);
    g.DrawLine(&pen, c.left + 6, c.top + 6, c.right - 6, c.bottom - 6);
    g.DrawLine(&pen, c.right - 6, c.top + 6, c.left + 6, c.bottom - 6);
}

void LauncherUI::DrawField(Graphics& g, const RECT& r, const std::wstring& text,
                           const std::wstring& hint, bool focused, bool mask) {
    BrutalBox(g, r, kWhite, focused ? 6 : 4, 3.0f);
    if (focused) {
        // 焦点：内圈再压一条黄边
        Pen p(kYellow, 3.0f);
        g.DrawRectangle(&p, (REAL)r.left + 5, (REAL)r.top + 5,
                        (REAL)(r.right - r.left - 10), (REAL)(r.bottom - r.top - 10));
    }

    std::wstring shown = mask ? std::wstring(text.size(), L'\u25CF') : text;
    RECT inner{r.left + 14, r.top, r.right - 14, r.bottom};

    if (shown.empty()) {
        DrawText(g, hint, inner, 13.0f, kMuted, StringAlignmentNear, true, true);
        if (focused && m_caretOn) {
            Pen cp(kInk, 2.4f);
            g.DrawLine(&cp, (REAL)inner.left, (REAL)(r.top + 9), (REAL)inner.left,
                       (REAL)(r.bottom - 9));
        }
        return;
    }

    DrawText(g, shown, inner, 14.0f, kInk, StringAlignmentNear, true, true);   // 不换行，超长省略
    if (focused && m_caretOn) {
        FontFamily fam(L"Microsoft YaHei UI");
        Font font(&fam, 14.0f, FontStyleBold, UnitPixel);
        RectF box;
        g.MeasureString(shown.c_str(), (INT)shown.size(), &font, PointF(0, 0), &box);
        REAL cx = inner.left + box.Width + 2;
        Pen cp(kInk, 2.4f);
        g.DrawLine(&cp, cx, (REAL)(r.top + 9), cx, (REAL)(r.bottom - 9));
    }
}

void LauncherUI::DrawButton(Graphics& g, const RECT& r, const std::wstring& text, Hit id,
                            bool primary) {
    const bool hot = (m_hover == id);
    const bool disabled = m_busy && (id == HIT_START);
    const bool down = hot && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    RECT rr = r;
    if (down) { rr.left += 4; rr.top += 4; rr.right += 4; rr.bottom += 4; }   // 按下位移

    Color fill = primary ? (disabled ? Color(230, 214, 200, 170) : (hot ? kYellowHi : kYellow))
                         : (hot ? kYellow : kWhite);
    BrutalBox(g, rr, fill, down ? 2 : 6, 3.0f);

    DrawText(g, text, rr, primary ? 19.0f : 13.0f, kInk, StringAlignmentCenter, true);
}

void LauncherUI::DrawCheck(Graphics& g, int x, int y, const std::wstring& label, bool on, Hit id) {
    RECT box{x, y, x + 18, y + 18};
    BrutalBox(g, box, on ? kYellow : kWhite, 3, 2.5f);
    if (on) {
        Pen tick(kInk, 3.0f);
        g.DrawLine(&tick, box.left + 4, box.top + 9, box.left + 7, box.top + 13);
        g.DrawLine(&tick, box.left + 7, box.top + 13, box.left + 14, box.top + 4);
    }
    RECT lr{x + 26, y - 2, x + 26 + 110, y + 20};
    DrawText(g, label, lr, 13.0f, kInk, StringAlignmentNear);
}

void LauncherUI::DrawFooter(Graphics& g) {
    RECT r{22, 304, m_width - 22, 332};
    bool bad = m_status.find(L"失败") != std::wstring::npos ||
               m_status.find(L"错误") != std::wstring::npos ||
               m_status.find(L"无帐号") != std::wstring::npos;
    bool good = m_status.find(L"成功") != std::wstring::npos;
    DrawText(g, m_status, r, 12.5f, good ? kGreen : (bad ? kRed : kMuted), StringAlignmentCenter,
             true);
}

// ---------------------------------------------------------------- 交互
void LauncherUI::SetStatus(const std::wstring& s) {
    m_status = s;
    LogLine(L"状态: " + s);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    Repaint();
}

void LauncherUI::Repaint() {
    if (!m_hwnd) return;
    HDC dc = GetDC(m_hwnd);
    if (dc) {
        Draw(dc);
        ReleaseDC(m_hwnd, dc);
    }
}

void LauncherUI::OpenUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    SetStatus(L"已打开: " + url);
}

void LauncherUI::OnClick(Hit id) {
    switch (id) {
        case HIT_CLOSE:
        case HIT_QUIT:
            PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
            break;
        case HIT_ACCOUNT:   m_focus = 1; SetStatus(L"输入服务器 IP"); break;
        case HIT_CLIENT:    BrowseClient(); break;
        case HIT_BROWSE:    BrowseClient(); break;
        case HIT_OPENCFG:   OpenConfig(); break;
        case HIT_START:     StartGame(); break;
        case HIT_REGISTER:  OpenUrl(m_cfg.website + L"register"); break;
        case HIT_CHANGEPWD: OpenUrl(m_cfg.website + L"changepassword"); break;
        case HIT_WEBSITE:   OpenUrl(m_cfg.website); break;
        default: break;
    }
}

void LauncherUI::BrowseClient() {
    wchar_t file[MAX_PATH * 2]{};
    if (!m_clientExe.empty()) lstrcpynW(file, m_clientExe.c_str(), MAX_PATH * 2);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"游戏客户端 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrTitle = L"选择游戏客户端 exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) {
        if (CommDlgExtendedError() == 0) SetStatus(L"已取消选择");
        return;
    }

    std::wstring full = file;
    const size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;

    m_cfg.clientDir = full.substr(0, pos);
    m_cfg.gameExe = full.substr(pos + 1);
    m_clientExe = full;
    LogLine(L"选择了客户端: " + full);
    SetStatus(L"客户端: " + m_cfg.gameExe);
}

void LauncherUI::OpenConfig() {
    const std::wstring path = m_cfg.IniFile();
    LogLine(L"打开配置文件: " + path);
    HINSTANCE r = ShellExecuteW(m_hwnd, L"open", L"notepad.exe", path.c_str(), nullptr,
                                SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) {
        SetStatus(L"打开配置文件失败");
    } else {
        SetStatus(L"已打开配置文件");
    }
}

void LauncherUI::StartGame() {
    if (m_busy) return;
    if (m_game.running) {
        SetStatus(L"游戏已在运行");
        return;
    }

    // 只校验 IP
    if (!IsIpv4(m_serverIp)) {
        SetStatus(L"服务器 IP 不合法，例如 221.231.130.70");
        return;
    }

    m_busy = true;
    SetStatus(L"正在启动游戏…");

    m_cfg.serverIp = m_serverIp;
    m_cfg.Save();                      // 记住 IP，下次直接带出来

    std::wstring err;
    if (!LaunchGame(m_cfg, m_game, err)) {
        m_busy = false;
        SetStatus(err);
        MessageBoxW(m_hwnd, err.c_str(), L"启动游戏失败", MB_ICONERROR);
        return;
    }
    m_busy = false;
    SetStatus(L"已启动游戏，等待登录窗口…");
    std::thread(WatchThread, this).detach();
}

DWORD WINAPI LauncherUI::DetectWorldThread(LPVOID param) {
    auto* self = (LauncherUI*)param;

    ScreenMatcher matcher;

    // 模板优先从 exe 内嵌资源取（单文件分发）；外面放一张 assets\backhome.png 可临时覆盖
    const int kBackHomeResId = 101;
    std::wstring tplPath = JoinPath(JoinPath(ExeDir(), L"assets"), L"backhome.png");
    bool haveTpl = false;
    if (GetFileAttributesW(tplPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        LogLine(L"发现外置模板，优先使用: " + tplPath);
        haveTpl = matcher.LoadTemplate(tplPath);
    }
    if (!haveTpl) haveTpl = matcher.LoadTemplateFromResource(kBackHomeResId);

    if (!haveTpl) {
        LogLine(L"没有可用模板（资源和 assets\\backhome.png 都没找到），直接弹第二窗体");
        PostMessageW(self->m_hwnd, WM_APP_WORLD_READY, 0, 0);
        return 0;
    }

    const DWORD timeoutMs = 15 * 60 * 1000;   // 最多等 15 分钟
    const DWORD t0 = GetTickCount();
    int hits = 0;
    double bestScore = 1e9;

    while (GetTickCount() - t0 < timeoutMs) {
        if (!self->m_game.running || !IsWindow(self->m_game.window)) break;
        double score = 1e9;
        POINT pos{};
        bool hit = matcher.MatchGameWindow(self->m_game.window, score, &pos);
        if (score < bestScore) bestScore = score;
        if (hit) {
            if (++hits >= 2) {                  // 连续两帧命中，防误判
                LogLine(Format(L"检测到「回到首页」(ncc=%.3f @%d,%d)，弹出第二窗体",
                               score, pos.x, pos.y));
                PostMessageW(self->m_hwnd, WM_APP_WORLD_READY, 0, 0);
                return 0;
            }
        } else {
            hits = 0;
        }
        Sleep(400);
    }

    LogLine(Format(L"等大区超时/游戏退出，兜底弹第二窗体（最好 ncc=%.3f）", bestScore));
    PostMessageW(self->m_hwnd, WM_APP_WORLD_READY, 0, 0);
    return 0;
}

void LauncherUI::ShowLoginDialog() {
    if (!m_dlg) m_dlg.reset(new LoginDialog(m_cfg, m_game.window));
    m_dlg->Create(m_inst, m_hwnd, m_cfg.dlgChild);
}

DWORD WINAPI LauncherUI::WatchThread(LPVOID param) {
    auto* self = (LauncherUI*)param;
    HWND game = WaitGameWindow(L"MapleStoryClass", self->m_cfg.waitGame, self->m_game.pi.hProcess);
    PostMessageW(self->m_hwnd, WM_APP_GAME_READY, (WPARAM)game, 0);
    if (self->m_game.pi.hProcess)
        WaitForSingleObject(self->m_game.pi.hProcess, INFINITE);
    PostMessageW(self->m_hwnd, WM_APP_GAME_EXIT, 0, 0);
    return 0;
}

LRESULT LauncherUI::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SHOWWINDOW:
            if (wp) Repaint();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(m_hwnd, &ps);
            Draw(dc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (m_dragging) {
                POINT p{x, y};
                ClientToScreen(m_hwnd, &p);
                SetWindowPos(m_hwnd, nullptr, p.x - m_dragOrigin.x, p.y - m_dragOrigin.y,
                             0, 0, SWP_NOSIZE | SWP_NOZORDER);
                return 0;
            }
            Hit h = HitTest(x, y);
            if (h != m_hover) {
                m_hover = h;
                SetCursor(LoadCursorW(nullptr, h == HIT_CLOSE ? IDC_HAND : IDC_ARROW));
Repaint();
            } else {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
                TrackMouseEvent(&tme);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (m_hover != HIT_NONE) {
                m_hover = HIT_NONE;
Repaint();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            Hit h = HitTest(x, y);
            if (h == HIT_TITLE) {
                m_dragging = true;
                m_dragOrigin = POINT{x, y};
                SetCapture(m_hwnd);
                return 0;
            }
            SetFocus(m_hwnd);
            OnClick(h);
Repaint();
            return 0;
        }

        case WM_LBUTTONUP:
            if (m_dragging) {
                m_dragging = false;
                ReleaseCapture();
            }
            return 0;

        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE) { PostMessageW(m_hwnd, WM_CLOSE, 0, 0); return 0; }
            if (wp == VK_RETURN) { StartGame(); return 0; }
            if (wp == VK_TAB)   { m_focus = (m_focus == 1) ? 2 : 1; SetStatus(L""); return 0; }
            if (wp == VK_BACK) {
                if (m_focus && !m_serverIp.empty()) m_serverIp.pop_back();
Repaint();
                return 0;
            }
            return 0;
        }

        case WM_CHAR: {
            wchar_t c = (wchar_t)wp;
            if (m_focus == 0) return 0;
            if (!((c >= L'0' && c <= L'9') || c == L'.')) return 0;   // 只收 IP 字符
            if (m_serverIp.size() < 15) m_serverIp.push_back(c);
Repaint();
            return 0;
        }

        case WM_TIMER: {
            if (wp == 2) {
                KillTimer(m_hwnd, 2);
                ShowLoginDialog();
                return 0;
            }
            if (wp == 1) {
                m_caretOn = !m_caretOn;
                if (m_focus) {
Repaint();
                }
            }
            return 0;
        }

        case WM_APP_WORLD_READY: {
            ShowLoginDialog();
            return 0;
        }

        case WM_APP_GAME_READY: {
            if (wp) {
                m_game.window = (HWND)wp;
                ShowWindow(m_hwnd, SW_HIDE);
                // 写死逻辑：等屏幕上出现「回到首页」按钮（玩家已选完大区）才弹第二窗体
                SetStatus(L"等待选择大区…");
                std::thread(DetectWorldThread, this).detach();
            } else {
                SetStatus(L"等不到 MapleStoryClass 窗口，游戏可能自己退出了");
            }
            return 0;
        }

        case WM_APP_GAME_EXIT: {
            if (m_dlg) { m_dlg->Close(); m_dlg.reset(); }
            m_game.running = false;
            if (m_cfg.bindIp) UnbindFakeIp(m_cfg.serverIp);
            ShowWindow(m_hwnd, SW_SHOW);
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetWindowPos(m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetStatus(L"游戏已退出");
            return 0;
        }

        case WM_CLOSE: {
            if (m_game.running && m_cfg.killGame) {
                int r = MessageBoxW(m_hwnd,
                                    L"检测到冒险岛正在运行中，\r\n关闭登录器游戏也会退出，\r\n\r\n确定要关闭吗？",
                                    L"确定关闭?", MB_YESNO | MB_ICONQUESTION);
                if (r != IDYES) return 0;
                StopGame(m_game);
            }
            KillTimer(m_hwnd, 1);
            DestroyWindow(m_hwnd);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(m_hwnd, msg, wp, lp);
    }
}










