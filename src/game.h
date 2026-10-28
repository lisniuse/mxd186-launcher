// game.h —— 启动/守护游戏进程: 复刻原版 netsh 假 IP + 「MapleStory.exe <ip> <port>」+ 等窗口
#pragma once
#include "config.h"
#include <windows.h>
#include <string>

struct GameSession {
    PROCESS_INFORMATION pi{};
    HWND    window = nullptr;
    bool    running = false;
};

// 游戏是否已经在跑(原版提示"请勿多次启动游戏!")
bool IsGameRunning(const std::wstring& exeName, DWORD* pidOut = nullptr);

// netsh int ip add addr 1 <ip>  —— 原版把服务器 IP 绑到环回网卡, 让客户端直接连本机
bool BindFakeIp(const std::wstring& ip);
void UnbindFakeIp(const std::wstring& ip);

// 组命令行 "MapleStory.exe <ip> <port>", 工作目录 = 客户端目录
bool LaunchGame(const Config& cfg, GameSession& out, std::wstring& err);

// 等游戏主窗口(原版等的是窗口类 MapleStoryClass)
HWND WaitGameWindow(const wchar_t* cls, int timeoutMs, HANDLE proc);

void StopGame(GameSession& s);

std::wstring BuildGameCmdline(const Config& cfg);
