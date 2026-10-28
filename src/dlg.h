// dlg.h —— 「mxd186 账号登录」第二窗体（原版游戏起来后弹的那个）
//
// 原版证据（脱壳映像 0x7064F6~0x7066B1）:
//   标签 "登录状态:未登录" / "版权: mxd186" / "服务端已启用自动注册"
//        "如需要更换账户随便填就可以注册" / 按钮 "登录" / 复选框 "记住账号"
//   控件名: 时钟1 时钟2 提示标签 标签EX1 按钮EX1  (E语言 ex_ui)
#pragma once
#include "config.h"
#include <windows.h>
#include <gdiplus.h>
#include <memory>
#include <string>

class LoginDialog {
public:
    LoginDialog(Config& cfg, HWND gameWnd) : m_cfg(cfg), m_gameWnd(gameWnd) {}

    bool Create(HINSTANCE inst, HWND parent = nullptr, bool asChild = false);
    void Close();
    bool IsOpen() const { return m_hwnd != nullptr; }

private:
    enum Hit { NONE = 0, ACCOUNT, PASSWORD, REMEMBER, LOGIN, COUNT };

    static LRESULT CALLBACK Thunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);

    void Layout();
    Hit  HitTest(int x, int y) const;
    void Repaint();
    void Paint(HDC dc);
    void DrawAll(Gdiplus::Graphics& g);
    void OnLogin();
    void FollowGameWindow();
    void SetStatus(const std::wstring& s);

    Config&      m_cfg;
    HWND         m_gameWnd = nullptr;
    HWND         m_hwnd = nullptr;
    HINSTANCE    m_inst = nullptr;
    RECT         m_rc[COUNT]{};

    std::wstring m_account;
    std::wstring m_password;
    std::wstring m_status = L"登录状态:未登录";
    int          m_focus = 0;
    bool         m_caret = true;
    bool         m_remember = true;
    bool         m_busy = false;
    Hit          m_hover = NONE;
    bool         m_dragging = false;
    POINT        m_dragOrigin{};

    int  m_w = 452;
    int  m_h = 236;      // 去掉底部标语后收矮
    int  m_titleH = 0;      // 子窗口模式下自绘标题条高度
    bool m_child = false;

public:
    enum : UINT {
        WM_DLG_OK = WM_APP + 11,
        WM_DLG_FAIL
    };
};
