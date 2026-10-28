// match.h —— 在游戏窗口里找「回到首页」按钮（模板匹配，无外部依赖）
//
// 模板来源两种：
//   1) 编译进 exe 的 RCDATA 资源（默认，单文件分发）
//   2) 外置 PNG 文件（临时换图用，优先于资源）
//
// 算法：抓屏 → 4 倍下采样灰度 → NCC（归一化互相关）滑窗 + std 门槛
// 实测：命中 ncc 0.92~1.00，未命中 0.17（棕色游戏背景不再误判）
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Gdiplus { class Bitmap; }

class ScreenMatcher {
public:
    bool LoadTemplate(const std::wstring& pngPath);   // 从 PNG 文件
    bool LoadTemplateFromResource(int resId);         // 从 exe 的 RCDATA 资源

    bool HasTemplate() const { return m_tw > 0 && m_th > 0; }

    // 抓 gameWnd 客户区并匹配；命中返回 true，outScore 为 NCC（越大越像）
    bool MatchGameWindow(HWND gameWnd, double& outScore, POINT* outPos = nullptr);

    double nccThreshold = 0.80;   // 归一化互相关阈值（1.0 = 完全一致）
    double minStdFactor = 0.5;    // 搜索窗口 std 至少是模板 std 的这个比例

private:
    std::vector<BYTE> m_tpl;   // 灰度、已下采样
    int m_tw = 0, m_th = 0;
    double m_tMean = 0, m_tStd = 0;

    static const int kScale = 4;   // 下采样倍数
    bool FinishTemplate(Gdiplus::Bitmap& bmp);
};
