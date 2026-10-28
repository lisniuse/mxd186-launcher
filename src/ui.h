// ui.h —— 无边框皮肤窗口(GDI+ 自绘), 复刻原版 ex_ui 那套观感
#pragma once
#include "config.h"
#include "game.h"
#include <windows.h>
#include <gdiplus.h>
#include <memory>
#include "dlg.h"
#include <string>

class LauncherUI {
public:
    explicit LauncherUI(Config& cfg) : m_cfg(cfg) {}
    bool Create(HINSTANCE inst);
    int  Run();

    // 后台线程回投给窗口的消息
    enum : UINT {
        WM_APP_AUTH_OK = WM_APP + 1,
        WM_APP_AUTH_FAIL,
        WM_APP_GAME_READY,
        WM_APP_GAME_EXIT,
        WM_APP_WORLD_READY
    };

private:
    // 命中区域
    enum Hit {
        HIT_NONE = 0,
        HIT_TITLE,
        HIT_CLOSE,
        HIT_ACCOUNT,
    HIT_CLIENT,      // 客户端 exe 路径（只读显示）
    HIT_BROWSE,      // 浏览…
    HIT_OPENCFG,     // 打开配置文件
        HIT_PASSWORD,
        HIT_REMEMBER,
        HIT_AUTOLOGIN,
        HIT_START,
        HIT_REGISTER,
        HIT_CHANGEPWD,
        HIT_WEBSITE,
        HIT_QUIT,
        HIT_COUNT
    };

    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);

    void InitLayout();
    Hit  HitTest(int x, int y) const;

    void Draw(HDC dc);
    void DrawBackground(Gdiplus::Graphics& g);
    void DrawTitleBar(Gdiplus::Graphics& g);
    void DrawField(Gdiplus::Graphics& g, const RECT& r, const std::wstring& text,
                   const std::wstring& hint, bool focused, bool mask);
    void DrawButton(Gdiplus::Graphics& g, const RECT& r, const std::wstring& text, Hit id,
                    bool primary);
    void DrawCheck(Gdiplus::Graphics& g, int x, int y, const std::wstring& label, bool on,
                   Hit id);
    void DrawFooter(Gdiplus::Graphics& g);

    void OnClick(Hit id);
    void StartGame();
    void BrowseClient();
    void OpenConfig();
    void ShowLoginDialog();
    void SetStatus(const std::wstring& s);
    void Repaint();
    void OpenUrl(const std::wstring& url);

    static DWORD WINAPI WatchThread(LPVOID param);
    static DWORD WINAPI DetectWorldThread(LPVOID param);

    Config&       m_cfg;
    HWND          m_hwnd = nullptr;
    HINSTANCE     m_inst = nullptr;
    RECT          m_rc[HIT_COUNT]{};

    std::wstring  m_serverIp;            // 主窗只输服务器 IP（账号密码挪到第二窗体）
    std::wstring  m_clientExe;           // 客户端 exe 完整路径（浏览选择/自动探测）
    int           m_focus = 0;          // 0=无 1=IP 输入框
    bool          m_caretOn = true;
    bool          m_remember = true;
    bool          m_autoLogin = false;
    bool          m_busy = false;
    bool          m_dragging = false;
    bool          m_layered = true;
    POINT         m_dragOrigin{};
    Hit           m_hover = HIT_NONE;
    std::wstring  m_status;
    GameSession   m_game;
    std::unique_ptr<LoginDialog> m_dlg;
    int           m_width = 520;
    int           m_height = 360;
    ULONG_PTR     m_gdiToken = 0;
};




