#include "dlg.h"
#include "auth.h"
#include "util.h"

#include <windowsx.h>
#include <thread>

using namespace Gdiplus;

namespace {

const wchar_t* kDlgClass = L"Mxd186LoginDlg";

const Color kInk     (255,  17,  17,  17);
const Color kPaper   (255, 255, 243, 214);
const Color kWhite   (255, 255, 255, 255);
const Color kPink    (255, 255, 122, 168);
const Color kYellow  (255, 255, 201,  60);
const Color kYellowHi(255, 255, 214, 102);
const Color kGreen   (255,  47, 191, 113);
const Color kRed     (255, 224,  62,  62);
const Color kMuted   (255, 110, 102,  90);

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

void Text(Graphics& g, const std::wstring& s, const RECT& r, float px, const Color& c,
          int align = StringAlignmentCenter, bool bold = false, bool noWrap = false) {
    FontFamily fam(L"Microsoft YaHei UI");
    Font f(&fam, px, FontStyleBold, UnitPixel);   // 一律加粗
    SolidBrush b(c);
    StringFormat fmt;
    fmt.SetAlignment((StringAlignment)align);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    if (noWrap) fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(s.c_str(), (INT)s.size(), &f,
                 RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left),
                       (REAL)(r.bottom - r.top)),
                 &fmt, &b);
}

}  // namespace

bool LoginDialog::Create(HINSTANCE inst, HWND parent, bool asChild) {
    m_inst = inst;
    Layout();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = Thunk;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kDlgClass;
    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    m_child = asChild && m_gameWnd && IsWindow(m_gameWnd);
    m_titleH = 30;                            // 统一自绘标题条（顶层/子窗口一致）
    int totalH = m_h + m_titleH;

    int x = 0, y = 0, w = m_w, h = totalH;
    if (m_child) {
        // 挂成游戏窗口的子窗口：坐标相对游戏客户区
        RECT cr{};
        GetClientRect(m_gameWnd, &cr);
        int cw = cr.right - cr.left, ch = cr.bottom - cr.top;
        x = (cw - m_w) / 2;
        y = (ch - totalH) / 2;
        m_hwnd = CreateWindowExW(0, kDlgClass, L"mxd186 账号登录",
                                 WS_CHILD | WS_VISIBLE, x, y, w, h,
                                 m_gameWnd, nullptr, inst, this);
    } else {
        w = m_w;
        h = totalH;
        RECT gr{};
        if (m_gameWnd && IsWindow(m_gameWnd) && GetWindowRect(m_gameWnd, &gr) &&
            (gr.right - gr.left) > 200) {
            x = gr.left + ((gr.right - gr.left) - w) / 2;
            y = gr.top + ((gr.bottom - gr.top) - h) / 2;
        } else {
            x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
            y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
        }
        // 粗野主义：不要系统标题栏，自己画一条
        m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kDlgClass,
                                 L"mxd186 账号登录", WS_POPUP, x, y, w, h,
                                 parent, nullptr, inst, this);
    }
    if (!m_hwnd) return false;

    if (!m_cfg.savedAccount.empty()) {
        m_account = m_cfg.savedAccount;
        m_password = m_cfg.savedPassword;
    }
    m_status = L"登录状态:未登录";

    ShowWindow(m_hwnd, SW_SHOW);
    if (!m_child) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        SetTimer(m_hwnd, 4, 200, nullptr);   // 跟随游戏窗口
    }
    SetTimer(m_hwnd, 1, 500, nullptr);
    Repaint();
    LogLine(Format(L"[个性登录] 窗口已显示 hwnd=%p 位置=(%d,%d) 大小=%dx%d 游戏窗口=%p",
                   m_hwnd, x, y, w, h, m_gameWnd));
    return true;
}

void LoginDialog::Close() {
    if (m_hwnd) {
        KillTimer(m_hwnd, 1);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void LoginDialog::Layout() {
    // 客户区坐标
    m_rc[ACCOUNT]  = {96, 116, 322, 144};
    m_rc[PASSWORD] = {96, 156, 322, 184};
    m_rc[REMEMBER] = {96, 190, 200, 210};
    m_rc[LOGIN]    = {334, 116, 410, 184};
}

LoginDialog::Hit LoginDialog::HitTest(int x, int y) const {
    y -= m_titleH;
    for (int i = ACCOUNT; i < COUNT; ++i) {
        const RECT& r = m_rc[i];
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return (Hit)i;
    }
    return NONE;
}

void LoginDialog::Repaint() {
    if (!m_hwnd) return;
    HDC dc = GetDC(m_hwnd);
    if (dc) {
        Paint(dc);
        ReleaseDC(m_hwnd, dc);
    }
}

void LoginDialog::Paint(HDC dc) {
    Bitmap bmp(m_w, m_h + m_titleH, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    DrawAll(g);

    HBITMAP hbm = nullptr;
    bmp.GetHBITMAP(Color(0, 0, 0, 0), &hbm);
    if (!hbm) return;
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, hbm);
    BitBlt(dc, 0, 0, m_w, m_h + m_titleH, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(hbm);
}

void LoginDialog::DrawAll(Graphics& g) {
    // 底色 + 外框
    SolidBrush paper(kPaper);
    g.FillRectangle(&paper, 0, 0, m_w, m_h + m_titleH);
    Pen frame(kInk, 4.0f);
    g.DrawRectangle(&frame, 2.0f, 2.0f, (REAL)m_w - 4, (REAL)(m_h + m_titleH) - 4);

    // 标题条
    if (m_titleH > 0) {
        SolidBrush band(kPink);
        g.FillRectangle(&band, 0, 0, m_w, m_titleH);
        Text(g, L"mxd186 账号登录", RECT{14, 0, m_w - 60, m_titleH}, 14.0f, kInk,
             StringAlignmentNear, true);

        RECT cb{m_w - 32, 5, m_w - 10, m_titleH - 5};
        BrutalBox(g, cb, kWhite, 2, 2.0f);
        Pen xp(kInk, 2.4f);
        g.DrawLine(&xp, (REAL)cb.left + 5, (REAL)cb.top + 5, (REAL)cb.right - 5, (REAL)cb.bottom - 5);
        g.DrawLine(&xp, (REAL)cb.right - 5, (REAL)cb.top + 5, (REAL)cb.left + 5, (REAL)cb.bottom - 5);

        Pen sep(kInk, 3.0f);
        g.DrawLine(&sep, 0.0f, (REAL)m_titleH, (REAL)m_w, (REAL)m_titleH);
        g.TranslateTransform(0, (REAL)m_titleH);
    }

    // 状态 / 提示
    bool ok = m_status.find(L"成功") != std::wstring::npos;
    bool bad = m_status.find(L"失败") != std::wstring::npos ||
               m_status.find(L"错") != std::wstring::npos ||
               m_status.find(L"无帐号") != std::wstring::npos;
    Text(g, m_status, RECT{10, 12, m_w - 10, 38}, 13.5f, ok ? kGreen : (bad ? kRed : kInk),
         StringAlignmentCenter, true, true);
    Text(g, L"版权: mxd186", RECT{0, 38, m_w, 58}, 12.0f, kMuted, StringAlignmentCenter, true);
    Text(g, L"服务端已启用自动注册", RECT{0, 58, m_w, 78}, 12.5f, kInk, StringAlignmentCenter, true);
    Text(g, L"换账户随便填就能注册", RECT{0, 78, m_w, 96}, 12.0f, kMuted, StringAlignmentCenter, true);

    // 输入框
    auto field = [&](const RECT& r, const std::wstring& val, const std::wstring& hint,
                     bool focused, bool mask) {
        BrutalBox(g, r, kWhite, focused ? 6 : 4, 3.0f);
        if (focused) {
            Pen p(kYellow, 3.0f);
            g.DrawRectangle(&p, (REAL)r.left + 5, (REAL)r.top + 5,
                            (REAL)(r.right - r.left - 10), (REAL)(r.bottom - r.top - 10));
        }
        std::wstring shown = mask ? std::wstring(val.size(), L'\u25CF') : val;
        RECT inner{r.left + 12, r.top, r.right - 10, r.bottom};
        if (shown.empty()) {
            Text(g, hint, inner, 13.0f, kMuted, StringAlignmentNear, true);
            if (focused && m_caret) {
                Pen cp(kInk, 2.4f);
                g.DrawLine(&cp, (REAL)inner.left, (REAL)(r.top + 7), (REAL)inner.left,
                           (REAL)(r.bottom - 7));
            }
        } else {
            Text(g, shown, inner, 14.0f, kInk, StringAlignmentNear, true);
            if (focused && m_caret) {
                FontFamily fam(L"Microsoft YaHei UI");
                Font f(&fam, 14.0f, FontStyleBold, UnitPixel);
                RectF box;
                g.MeasureString(shown.c_str(), (INT)shown.size(), &f, PointF(0, 0), &box);
                Pen cp(kInk, 2.4f);
                REAL cx = inner.left + box.Width + 2;
                g.DrawLine(&cp, cx, (REAL)(r.top + 7), cx, (REAL)(r.bottom - 7));
            }
        }
    };

    Text(g, L"账号", RECT{22, m_rc[ACCOUNT].top, 88, m_rc[ACCOUNT].bottom}, 14.5f, kInk,
         StringAlignmentNear, true);
    Text(g, L"密码", RECT{22, m_rc[PASSWORD].top, 88, m_rc[PASSWORD].bottom}, 14.5f, kInk,
         StringAlignmentNear, true);
    field(m_rc[ACCOUNT], m_account, L"请输入帐号", m_focus == 1, false);
    field(m_rc[PASSWORD], m_password, L"请输入密码", m_focus == 2, true);

    // 记住账号（方框 + 硬阴影）
    {
        RECT r = m_rc[REMEMBER];
        RECT box{r.left, r.top + 2, r.left + 20, r.top + 22};
        BrutalBox(g, box, m_remember ? kYellow : kWhite, 3, 2.5f);
        if (m_remember) {
            Pen tick(kInk, 3.0f);
            g.DrawLine(&tick, (REAL)box.left + 4, (REAL)box.top + 10, (REAL)box.left + 8,
                       (REAL)box.top + 15);
            g.DrawLine(&tick, (REAL)box.left + 8, (REAL)box.top + 15, (REAL)box.right - 4,
                       (REAL)box.top + 5);
        }
        Text(g, L"记住账号", RECT{r.left + 28, r.top - 1, r.left + 140, r.bottom + 3}, 13.0f, kInk,
             StringAlignmentNear, true);
    }

    // 登录按钮（大黄块 + 粗黑边 + 硬阴影，按下位移）
    {
        RECT r = m_rc[LOGIN];
        bool hot = (m_hover == LOGIN);
        bool down = hot && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        RECT rr = r;
        if (down) { rr.left += 4; rr.top += 4; rr.right += 4; rr.bottom += 4; }
        BrutalBox(g, rr, m_busy ? Color(230, 214, 200, 170) : (hot ? kYellowHi : kYellow),
                  down ? 2 : 6, 3.0f);
        Text(g, m_busy ? L"…" : L"登录", rr, 17.0f, kInk, StringAlignmentCenter, true);
    }

}

void LoginDialog::SetStatus(const std::wstring& s) {
    m_status = s;
    LogLine(L"[个性登录] " + s);
    Repaint();
}

void LoginDialog::OnLogin() {
    if (m_busy) return;
    if (m_account.size() < 5 || m_account.size() > 12) {
        SetStatus(L"帐号长度为5-12位字符");
        return;
    }
    if (m_password.empty()) {
        SetStatus(L"请输入密码");
        return;
    }
    m_busy = true;
    SetStatus(L"正在登录…");

    struct Ctx { Config cfg; std::wstring a, p; HWND hwnd; };
    auto* c = new Ctx{m_cfg, m_account, m_password, m_hwnd};
    std::thread([c] {
        AuthResult r = AuthLogin(c->cfg, c->a, c->p);
        bool ok = (r.code == AuthCode::Ok);
        std::wstring msg = r.msg;
        if (ok) {
            // 原版点「登录」之后真正干的事：连 serverIp:8800 发 账号/密码/aoa + 环境块，收 "A\n"
            std::wstring reply;
            bool a8800 = NotifyAuth8800(c->cfg, c->a, c->p, reply);
            LogLine(Format(L"[个性登录] 8800 认证: %s%s", a8800 ? L"A(通过)" : L"未通过",
                           reply.empty() ? L"" : (L"  回包=" + reply).c_str()));
            if (a8800) msg = L"登录成功 (8800 已认证)";
        }
        auto* pair = new std::pair<bool, std::wstring>(ok, msg);
        PostMessageW(c->hwnd, ok ? WM_DLG_OK : WM_DLG_FAIL, (WPARAM)pair, 0);
        delete c;
    }).detach();
}

void LoginDialog::FollowGameWindow() {
    if (m_child || !m_hwnd || !m_gameWnd || !IsWindow(m_gameWnd)) return;
    if (IsIconic(m_gameWnd) || !IsWindowVisible(m_gameWnd)) return;

    RECT cr{};
    if (!GetClientRect(m_gameWnd, &cr)) return;
    POINT org{0, 0};
    ClientToScreen(m_gameWnd, &org);

    RECT wr{};
    GetWindowRect(m_hwnd, &wr);
    const int w = wr.right - wr.left, h = wr.bottom - wr.top;
    const int cw = cr.right - cr.left, ch = cr.bottom - cr.top;
    const int x = org.x + (cw - w) / 2;
    const int y = org.y + (ch - h) / 2;

    if (abs(wr.left - x) > 1 || abs(wr.top - y) > 1) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        // 位置没变也要保证在最上层（游戏全屏刷新可能压过来）
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

LRESULT CALLBACK LoginDialog::Thunk(HWND h, UINT m, WPARAM w, LPARAM l) {
    LoginDialog* self = nullptr;
    if (m == WM_NCCREATE) {
        auto cs = (CREATESTRUCTW*)l;
        self = (LoginDialog*)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = h;
    } else {
        self = (LoginDialog*)GetWindowLongPtrW(h, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(h, m, w, l);
    return self->Handle(m, w, l);
}

LRESULT LoginDialog::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(m_hwnd, &ps);
            Paint(dc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            Hit h = HitTest(x, y);
            if (h != m_hover) {
                m_hover = h;
                SetCursor(LoadCursorW(nullptr, h != NONE ? IDC_HAND : IDC_ARROW));
                Repaint();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            // 标题条右上的 ✕
            if (y >= 0 && y < m_titleH && x >= m_w - 34 && x <= m_w - 8) {
                Close();
                return 0;
            }
            Hit h = HitTest(x, y);
            SetFocus(m_hwnd);
            switch (h) {
                case ACCOUNT:  m_focus = 1; SetStatus(L"登录状态:未登录"); break;
                case PASSWORD: m_focus = 2; SetStatus(L"登录状态:未登录"); break;
                case REMEMBER: m_remember = !m_remember; Repaint(); break;
                case LOGIN:    OnLogin(); break;
                default: break;
            }
            Repaint();
            return 0;
        }

        case WM_KEYDOWN:
            if (wp == VK_RETURN) { OnLogin(); return 0; }
            if (wp == VK_TAB)    { m_focus = (m_focus == 1) ? 2 : 1; Repaint(); return 0; }
            if (wp == VK_ESCAPE) { Close(); return 0; }
            if (wp == VK_BACK) {
                std::wstring& s = (m_focus == 1) ? m_account : m_password;
                if (m_focus && !s.empty()) s.pop_back();
                Repaint();
                return 0;
            }
            return 0;

        case WM_CHAR: {
            wchar_t c = (wchar_t)wp;
            if (m_focus == 0 || c < 32 || c > 126) return 0;
            std::wstring& s = (m_focus == 1) ? m_account : m_password;
            size_t limit = (m_focus == 1) ? 12 : 24;
            if (s.size() < limit) s.push_back(c);
            Repaint();
            return 0;
        }

        case WM_TIMER:
            if (wp == 4) {
                FollowGameWindow();
                return 0;
            }
            if (wp == 3) {
                KillTimer(m_hwnd, 3);
                LogLine(L"[个性登录] 登录成功，窗体自动关闭");
                Close();
                return 0;
            }
            if (wp == 1) {
                m_caret = !m_caret;
                if (m_focus) Repaint();
            }
            return 0;

        case WM_DLG_OK:
        case WM_DLG_FAIL: {
            auto* msgp = (std::pair<bool, std::wstring>*)wp;
            std::wstring text = msgp->second;
            bool ok = msgp->first;
            delete msgp;
            m_busy = false;
            if (!ok) { SetStatus(text); MessageBeep(MB_ICONWARNING); return 0; }

            SetStatus(L"登录成功");
            {
                RegWriteStr(HKEY_CURRENT_USER, L"SOFTWARE\\Maple", L"name", m_account);
                RegWriteStr(HKEY_CURRENT_USER, L"SOFTWARE\\Maple", L"pawd", m_password);
                RegWriteStr(HKEY_CURRENT_USER, L"SOFTWARE\\Maple", L"login", L"1");
                if (m_remember) { m_cfg.savedAccount = m_account; m_cfg.savedPassword = m_password; }
            }
            // 原版是留着窗口只改状态: 这里不再自动关闭, 你自己点 ✕ 收起来
            // 可选: 用模拟键盘把账号密码敲进游戏(不注入, 不会被 BlackCipher 当外挂)
            if (m_cfg.autoType && m_gameWnd && IsWindow(m_gameWnd)) {
                SetStatus(L"登录成功，正在输入到游戏…");
                LogLine(L"[个性登录] autotype: 开始向游戏窗口输入");
                TypeIntoWindow(m_gameWnd, m_account, 60);
                SendVk(VK_TAB, 120);
                TypeIntoWindow(m_gameWnd, m_password, 60);
                SendVk(VK_RETURN, 120);
                LogLine(L"[个性登录] autotype: 已发送 账号/Tab/密码/Enter");
            }

            // 登录成功 → 让「登录成功」停留 800ms，然后自动消失
            SetTimer(m_hwnd, 3, 800, nullptr);
            if (!m_child) {
                SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            return 0;
        }

        case WM_CLOSE:
            KillTimer(m_hwnd, 1);
            DestroyWindow(m_hwnd);
            return 0;

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;

        default:
            return DefWindowProcW(m_hwnd, msg, wp, lp);
    }
}




