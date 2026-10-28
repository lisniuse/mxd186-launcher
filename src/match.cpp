#include "match.h"
#include "util.h"

#include <gdiplus.h>
#include <cmath>
#include <vector>

using namespace Gdiplus;

namespace {

// BMP 像素 (BGRA, 自顶向下) → 灰度 + 下采样
std::vector<BYTE> ToGrayDown(const BYTE* bgra, int w, int h, int stride, int scale,
                             int& outW, int& outH) {
    outW = w / scale;
    outH = h / scale;
    std::vector<BYTE> out((size_t)outW * outH);
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            int sum = 0;
            for (int dy = 0; dy < scale; ++dy) {
                const BYTE* row = bgra + (size_t)(y * scale + dy) * stride + (size_t)x * scale * 4;
                for (int dx = 0; dx < scale; ++dx) {
                    const BYTE* p = row + dx * 4;
                    sum += (p[2] * 77 + p[1] * 150 + p[0] * 29) >> 8;   // 0.299R+0.587G+0.114B
                }
            }
            out[(size_t)y * outW + x] = (BYTE)(sum / (scale * scale));
        }
    }
    return out;
}

}  // namespace

// ------------------------------------------------------------------ 模板装载
bool ScreenMatcher::FinishTemplate(Bitmap& bmp) {
    const int w = (int)bmp.GetWidth(), h = (int)bmp.GetHeight();
    if (w < kScale * 4 || h < kScale * 4) {
        LogLine(Format(L"模板太小: %dx%d", w, h));
        return false;
    }
    Rect rc(0, 0, w, h);
    BitmapData bd{};
    if (bmp.LockBits(&rc, ImageLockModeRead, PixelFormat32bppARGB, &bd) != Ok) {
        LogLine(L"模板 LockBits 失败");
        return false;
    }
    int dw = 0, dh = 0;
    m_tpl = ToGrayDown((const BYTE*)bd.Scan0, w, h, bd.Stride, kScale, dw, dh);
    bmp.UnlockBits(&bd);
    m_tw = dw;
    m_th = dh;

    double sum = 0, sq = 0;
    for (BYTE v : m_tpl) { sum += v; sq += (double)v * v; }
    const double n = (double)m_tpl.size();
    m_tMean = sum / n;
    const double var = sq / n - m_tMean * m_tMean;
    m_tStd = var > 0 ? sqrt(var) : 0.0;

    LogLine(Format(L"模板已载入: %dx%d → %dx%d, mean=%.1f std=%.1f", w, h, m_tw, m_th, m_tMean,
                   m_tStd));
    return m_tw > 0 && m_th > 0 && m_tStd >= 1.0;
}

bool ScreenMatcher::LoadTemplate(const std::wstring& pngPath) {
    Bitmap bmp(pngPath.c_str());
    if (bmp.GetLastStatus() != Ok) {
        LogLine(L"模板文件载入失败: " + pngPath);
        return false;
    }
    return FinishTemplate(bmp);
}

bool ScreenMatcher::LoadTemplateFromResource(int resId) {
    HRSRC hr = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hr) {
        LogLine(Format(L"找不到模板资源 id=%d", resId));
        return false;
    }
    const DWORD size = SizeofResource(nullptr, hr);
    HGLOBAL hg = LoadResource(nullptr, hr);
    const void* data = hg ? LockResource(hg) : nullptr;
    if (!data || size == 0) {
        LogLine(L"读取模板资源失败");
        return false;
    }

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!mem) return false;
    void* dst = GlobalLock(mem);
    memcpy(dst, data, size);
    GlobalUnlock(mem);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK || !stream) {
        GlobalFree(mem);
        return false;
    }
    Bitmap bmp(stream);
    const bool ok = (bmp.GetLastStatus() == Ok) && FinishTemplate(bmp);
    stream->Release();
    LogLine(ok ? L"模板来自 exe 内嵌资源" : L"内嵌模板解析失败");
    return ok;
}

// ------------------------------------------------------------------ 匹配
bool ScreenMatcher::MatchGameWindow(HWND gameWnd, double& outScore, POINT* outPos) {
    outScore = -1.0;
    if (!HasTemplate() || !gameWnd || !IsWindow(gameWnd)) return false;
    if (IsIconic(gameWnd)) return false;

    RECT cr{};
    GetClientRect(gameWnd, &cr);
    const int w = cr.right - cr.left, h = cr.bottom - cr.top;
    if (w < 200 || h < 150) return false;

    HDC hdcWin = GetDC(gameWnd);
    if (!hdcWin) return false;
    HDC mem = CreateCompatibleDC(hdcWin);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdcWin, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        DeleteDC(mem);
        ReleaseDC(gameWnd, hdcWin);
        return false;
    }
    HGDIOBJ old = SelectObject(mem, hbmp);
    const BYTE* px = (const BYTE*)bits;

    auto isBlack = [](const BYTE* p) {
        long long s = 0;
        for (int i = 0; i < 2000; ++i) s += p[(size_t)i * 4];
        return s < 2000;
    };

    // ① 屏幕 DC 拷贝（D3D 最稳）  ② PrintWindow  ③ 窗口 DC
    POINT org{0, 0};
    ClientToScreen(gameWnd, &org);
    HDC hdcScreen = GetDC(nullptr);
    const BOOL okScreen = BitBlt(mem, 0, 0, w, h, hdcScreen, org.x, org.y, SRCCOPY);
    ReleaseDC(nullptr, hdcScreen);
    if (!okScreen || isBlack(px)) PrintWindow(gameWnd, mem, 2);
    if (isBlack(px)) BitBlt(mem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);

    int dw = 0, dh = 0;
    std::vector<BYTE> gray = ToGrayDown(px, w, h, w * 4, kScale, dw, dh);

    SelectObject(mem, old);
    DeleteObject(hbmp);
    DeleteDC(mem);
    ReleaseDC(gameWnd, hdcWin);

    if (dw <= m_tw || dh <= m_th) return false;
    if (m_tStd < 1.0) return false;

    // 积分图：O(1) 求窗口均值/方差
    std::vector<double> isum((size_t)(dw + 1) * (dh + 1), 0.0);
    std::vector<double> isq((size_t)(dw + 1) * (dh + 1), 0.0);
    for (int y = 0; y < dh; ++y) {
        double rs = 0, rq = 0;
        for (int x = 0; x < dw; ++x) {
            const double v = gray[(size_t)y * dw + x];
            rs += v;
            rq += v * v;
            isum[(size_t)(y + 1) * (dw + 1) + (x + 1)] = isum[(size_t)y * (dw + 1) + (x + 1)] + rs;
            isq[(size_t)(y + 1) * (dw + 1) + (x + 1)] = isq[(size_t)y * (dw + 1) + (x + 1)] + rq;
        }
    }
    auto boxSum = [](const std::vector<double>& ii, int stride, int x0, int y0, int bw, int bh) {
        return ii[(size_t)y0 * stride + x0] + ii[(size_t)(y0 + bh) * stride + (x0 + bw)]
             - ii[(size_t)y0 * stride + (x0 + bw)] - ii[(size_t)(y0 + bh) * stride + x0];
    };

    const int maxY = dh - m_th, maxX = dw - m_tw;
    const double n = (double)m_tw * m_th;
    const double stdFloor = m_tStd * minStdFactor;
    double bestNcc = -1e9;
    int bx = 0, by = 0;

    for (int y = 0; y <= maxY; ++y) {
        for (int x = 0; x <= maxX; ++x) {
            const double mean = boxSum(isum, dw + 1, x, y, m_tw, m_th) / n;
            const double var = boxSum(isq, dw + 1, x, y, m_tw, m_th) / n - mean * mean;
            if (var <= stdFloor * stdFloor) continue;
            const double istd = sqrt(var);

            double dot = 0;
            for (int ty = 0; ty < m_th; ++ty) {
                const BYTE* ip = gray.data() + (size_t)(y + ty) * dw + x;
                const BYTE* tp = m_tpl.data() + (size_t)ty * m_tw;
                for (int tx = 0; tx < m_tw; ++tx) dot += (double)ip[tx] * (double)tp[tx];
            }
            const double cov = dot / n - mean * m_tMean;
            const double ncc = cov / (istd * m_tStd);
            if (ncc > bestNcc) {
                bestNcc = ncc;
                bx = x;
                by = y;
            }
        }
    }

    outScore = bestNcc;
    if (outPos) {
        outPos->x = bx * kScale;
        outPos->y = by * kScale;
    }
    return bestNcc >= nccThreshold;
}
