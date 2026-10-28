// config.h —— 配置层: 复刻原版 fig.ini / Patcher.info / HKCU\SOFTWARE\Maple 三处配置
#pragma once
#include <windows.h>
#include <string>

struct Config {
    // ---- 原版 fig.ini 的键(键名照抄, 老配置可以直接拿来用) ----
    std::wstring title;            // [biaoti] biaoti1   界面标题文字
    bool         autoLogin = false;// [biaoti] 自动登录=0/1
    int          bmsMode   = 0;    // [bms] bmskjfs      0=普通模式 1=快速模式
    std::wstring clientDir;        // [fig]  fig1        客户端目录

    // ---- 原版 Patcher.info(定长文本: 游戏exe / IP / 端口 / 目录) ----
    std::wstring gameExe  = L"MapleStory.exe";
    std::wstring serverIp = L"127.0.0.1";
    int          serverPort = 8484;

    // ---- 复刻版扩展键(原版会忽略, 不冲突) ----
    std::wstring authMode = L"mysql";   // [bms] auth = mysql | tcp | none
    bool         bindIp   = false;      // [bms] bindip  用 netsh 把服务器IP绑到环回网卡
    std::wstring website = L"http://www.super-ec.cn/"; // [bms] website 官网/注册页
    int          dlgDelayMs = 0;        // [bms] dlgdelay  游戏窗口出现后再等多久才弹第二窗体
    bool         dlgChild   = false;    // [bms] dlgchild  1 = 挂成游戏窗口的子窗口
    int          authPort8800 = 8800;   // [bms] authport8800  原版「点登录」时连的端口
    bool         autoType = false;     // [bms] autotype 登录成功后用 SendInput 把账号密码敲进游戏
    bool         killGame = true;       // [bms] killgame 关闭登录器时是否结束游戏
    int          waitGame = 60000;      // [bms] waitgame 等待 MapleStoryClass 的超时(ms)

    // MySQL 直连校验(原版内嵌 libmysql)
    std::wstring dbHost = L"auto";   // auto/空 = 跟随主窗 IP
    int          dbPort = 3306;
    std::wstring dbUser = L"root";
    std::wstring dbPass;
    std::wstring dbName = L"maplestory";
    // %s 依次替换为 账号 / 密码
    std::wstring dbSql = L"SELECT id FROM accounts WHERE name='%s' AND password='%s' LIMIT 1";

    // 记住的账号(对应原版 HKCU\SOFTWARE\Maple\name / pawd)
    std::wstring savedAccount;
    std::wstring savedPassword;
    bool         remember = true;

    std::wstring iniPath;
    const std::wstring& IniFile() const { return iniPath; }   // 给「打开配置文件」用
    std::wstring exeDir;

    bool Load();
    static std::wstring ConfigDir();      // %USERPROFILE%\.Mxd186Launcher
    bool Save();
    void Log() const;
};

// 读原版 Patcher.info(UTF-16LE 定长文本), 失败返回 false
bool LoadPatcherInfo(const std::wstring& path, std::wstring& exe, std::wstring& ip,
                     int& port, std::wstring& dir);


