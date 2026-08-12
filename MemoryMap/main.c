#include "optimizer.h"
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <wctype.h>

// UI 識別碼
#define IDC_SEARCH_EDIT    101
#define TIMER_STATS_UPDATE 1001
#define TIMER_PROC_UPDATE  1002
#define TIMER_CLEAR_STATUS 1003

// 視窗尺寸
#define WIN_WIDTH  850
#define WIN_HEIGHT 720

// 顏色定義 (深色主題)
#define COLOR_BG          RGB(15, 15, 18)      // 主背景：極深灰
#define COLOR_CARD        RGB(26, 26, 32)      // 卡片底色：深灰藍
#define COLOR_CARD_BORDER RGB(48, 48, 58)      // 卡片邊框
#define COLOR_TEXT_PRI    RGB(255, 255, 255)  // 主要文字：純白
#define COLOR_TEXT_SEC    RGB(150, 150, 160)  // 次要文字：灰色
#define COLOR_TEXT_MUTED  RGB(90, 90, 100)    // 禁用/微小文字：暗灰
#define COLOR_PROTECT     RGB(34, 197, 94)     // 綠色：白名單保護
#define COLOR_PROTECT_BG  RGB(20, 80, 40)

#define COLOR_ACCENT      RGB(14, 165, 233)    // 主題色：亮天藍
#define COLOR_ACCENT_HOV  RGB(56, 189, 248)    // 主題色懸停
#define COLOR_ACCENT_ACT  RGB(2, 132, 199)    // 主題色點擊
#define COLOR_ACCENT_BG   RGB(12, 45, 64)      // 主題色半透明底

#define COLOR_SAFE        RGB(16, 185, 129)    // 安全：翠綠
#define COLOR_SAFE_BG     RGB(6, 70, 50)
#define COLOR_CAUTION     RGB(245, 158, 11)    // 警告：琥珀黃
#define COLOR_CAUTION_BG  RGB(110, 60, 10)
#define COLOR_CRITICAL    RGB(239, 68, 68)     // 核心：鮮紅
#define COLOR_CRITICAL_BG RGB(110, 20, 20)

#define COLOR_LIST_HOVER  RGB(36, 36, 44)      // 行程懸停底色
#define COLOR_LIST_SEP    RGB(34, 34, 40)      // 行程分割線
#define COLOR_SCROLL_BAR  RGB(40, 40, 48)      // 捲軸軌道
#define COLOR_SCROLL_THM  RGB(80, 80, 92)      // 捲軸滑塊

// 過濾器狀態
#define FILTER_ALL      0
#define FILTER_SAFE     1
#define FILTER_CAUTION  2
#define FILTER_CRITICAL 3

// 全域變數
HINSTANCE g_hInstance = NULL;
int g_Dpi = 96;
HFONT g_FontTitle = NULL;
HFONT g_FontHeader = NULL;
HFONT g_FontNormal = NULL;
HFONT g_FontSmall = NULL;

// 記憶體與行程數據
MemoryStats g_Stats = {0};
float g_MemHistory[60] = {0};
float g_PageHistory[60] = {0};
float g_FragHistory[60] = {0};
ProcessInfo g_AllProcesses[512] = {0};
int g_AllProcessCount = 0;
ProcessInfo g_FilteredProcesses[512] = {0};
int g_FilteredProcessCount = 0;

// 控制項狀態
HWND g_hSearchEdit = NULL;
WCHAR g_SearchQuery[128] = L"";
int g_Filter = FILTER_ALL;
int g_ScrollOffset = 0;
BOOL g_IsOptimizing = FALSE;
WCHAR g_StatusMessage[256] = L"系統狀態：就緒。建議在記憶體吃緊時進行一鍵加速。";
COLORREF g_StatusColor = RGB(150, 150, 160);

// 滑鼠互動狀態
int g_HoveredTab = -1;       // 懸停的分頁 (0-3)
int g_HoveredRow = -1;       // 懸停的行程列 (0-filteredCount-1)
BOOL g_HoveredBtn = FALSE;   // 是否懸停在某一列的結束按鈕上
int g_HoveredBtnRow = -1;    // 懸停結束按鈕的列索引
BOOL g_OptimizeBtnHover = FALSE; // 一鍵優化按鈕懸停
BOOL g_OptimizeBtnActive = FALSE;// 一鍵優化按鈕點擊中
BOOL g_RefreshBtnHover = FALSE;  // 重新整理按鈕懸停

// 捲軸拖曳狀態
BOOL g_IsDraggingScroll = FALSE;
int g_DragStartY = 0;
int g_DragStartScrollOffset = 0;

// 自適應佈局全域值（由 UpdateLayout 計算）
int g_Padding = 20;
int g_AvailableW = 0;
int g_LeftW = 0;
int g_RightX = 0;
int g_RightW = 0;
int g_CardY = 0;
int g_CardH = 0;
int g_CardW = 0;
int g_ChartX = 0;
int g_ChartY = 0;
int g_ChartW = 0;
int g_ChartH = 0;
int g_OptX = 0;
int g_OptY = 0;
int g_OptW = 0;
int g_OptH = 0;
int g_RefX = 0;
int g_RefY = 0;
int g_RefW = 0;
int g_RefH = 0;
int g_ListX = 0;
int g_ListY = 0;
int g_ListW = 0;
int g_ListH = 0;
int g_RowH = 0;
int g_TabY = 0;
int g_TabH = 0;
int g_TabWArr[4] = {0,0,0,0};
int g_SearchX = 0;
int g_SearchY = 0;
int g_SearchW = 0;

// 縮放尺寸輔助函數
int Scale(int val) {
    return MulDiv(val, g_Dpi, 96);
}

// 建立適應 DPI 的字型
void UpdateFonts(HWND hwnd) {
    if (g_FontTitle) DeleteObject(g_FontTitle);
    if (g_FontHeader) DeleteObject(g_FontHeader);
    if (g_FontNormal) DeleteObject(g_FontNormal);
    if (g_FontSmall) DeleteObject(g_FontSmall);

    g_FontTitle = CreateFontW(-Scale(20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_FontHeader = CreateFontW(-Scale(14), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_FontNormal = CreateFontW(-Scale(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_FontSmall = CreateFontW(-Scale(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

// 應用程式篩選與搜尋邏輯
void ApplyFilterAndSearch() {
    g_FilteredProcessCount = 0;
    for (int i = 0; i < g_AllProcessCount; i++) {
        // 篩選分類
        BOOL matchType = FALSE;
        if (g_Filter == FILTER_ALL) matchType = TRUE;
        else if (g_Filter == FILTER_SAFE && g_AllProcesses[i].safety == SAFETY_SAFE) matchType = TRUE;
        else if (g_Filter == FILTER_CAUTION && g_AllProcesses[i].safety == SAFETY_CAUTION) matchType = TRUE;
        else if (g_Filter == FILTER_CRITICAL && g_AllProcesses[i].safety == SAFETY_CRITICAL) matchType = TRUE;

        if (!matchType) continue;

        // 模糊搜尋 (不區分大小寫)
        BOOL matchSearch = TRUE;
        if (wcslen(g_SearchQuery) > 0) {
            matchSearch = (StrStrIW(g_AllProcesses[i].name, g_SearchQuery) != NULL);
        }

        if (matchSearch) {
            g_FilteredProcesses[g_FilteredProcessCount++] = g_AllProcesses[i];
        }
    }

    // 限制捲動範圍（使用動態行高與清單高度）
    int rowHeight = g_RowH > 0 ? g_RowH : Scale(45);
    int listHeight = g_ListH > 0 ? g_ListH : Scale(290);
    int totalHeight = g_FilteredProcessCount * rowHeight;
    int maxScroll = max(0, totalHeight - listHeight);
    if (g_ScrollOffset > maxScroll) {
        g_ScrollOffset = maxScroll;
    }
}

// 計算所有動態佈局參數（在 WM_SIZE / WM_CREATE / WM_DPICHANGED 及需要時呼叫）
void UpdateLayout(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right;
    int height = rect.bottom;

    g_Padding = Scale(20);
    g_AvailableW = max(0, width - g_Padding * 2);
    int gap = Scale(15);

    // 左右寬度平均分配
    int halfW = (g_AvailableW - gap) / 2;
    g_LeftW = max(Scale(260), halfW);
    g_RightW = max(Scale(260), g_AvailableW - g_LeftW - gap);
    g_RightX = g_Padding + g_LeftW + gap;

    g_CardY = Scale(60);
    int availableLeftHeight = height - g_CardY - g_Padding;
    g_CardH = max(Scale(160), (availableLeftHeight - gap * 2) / 3);
    g_CardW = g_LeftW;

    g_ChartX = g_Padding;
    g_ChartY = g_CardY + g_CardH * 3 + gap * 3;
    g_ChartW = g_LeftW;
    g_ChartH = 0; // 不再使用單一綜合大圖板，改為每張卡片下方迷你走勢圖

    g_OptW = min(Scale(420), g_RightW - Scale(20));
    g_OptH = max(Scale(40), Scale(42));
    g_OptX = g_RightX + (g_RightW - g_OptW) / 2;
    g_OptY = g_CardY;

    g_RefW = Scale(35);
    g_RefH = Scale(35);
    g_RefX = g_RightX + g_RightW - g_RefW - Scale(10);

    g_ListX = g_RightX + Scale(10);
    g_TabY = g_OptY + g_OptH + gap;
    g_TabH = Scale(28);
    int searchMinW = Scale(140);
    int searchMargin = Scale(10);
    int maxSearchX = g_RefX - searchMinW - searchMargin;
    int tabTotalMax = min(max(0, maxSearchX - g_ListX - Scale(12)), (int)(g_RightW * 0.7));
    for (int i = 0; i < 4; i++) g_TabWArr[i] = tabTotalMax / 4;
    g_SearchW = searchMinW;
    g_SearchX = g_RefX - g_SearchW - searchMargin;
    g_SearchY = g_TabY + (g_TabH - Scale(26)) / 2;
    g_RefY = g_TabY + (g_TabH - g_RefH) / 2;

    if (tabTotalMax <= 0) {
        // 空間不足時改換行顯示搜尋框，靠右對齊
        g_SearchX = g_RightX + g_RightW - g_SearchW - searchMargin;
        g_SearchY = g_TabY + g_TabH + Scale(8);
        g_ListY = g_SearchY + Scale(26) + gap + Scale(26);
    } else {
        g_ListY = g_TabY + g_TabH + gap + Scale(26);
    }
    g_ListW = g_RightW - Scale(20);
    g_ListH = max(Scale(180), height - g_ListY - g_Padding);
    g_RowH = max(Scale(40), height / 18);

    // 調整搜尋框位置
    if (g_hSearchEdit) {
        MoveWindow(g_hSearchEdit, g_SearchX, g_SearchY, g_SearchW, Scale(26), TRUE);
        SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);
    }
}

// 重新整理行程數據
void RefreshData(HWND hwnd) {
    // 取得記憶體資訊
    GetSystemMemoryStats(&g_Stats);

    // 載入行程清單
    g_AllProcessCount = GetProcessList(g_AllProcesses, 512);

    // 套用篩選
    ApplyFilterAndSearch();

    // 重繪畫面
    InvalidateRect(hwnd, NULL, FALSE);
}

// 繪製圓角矩形框線與填充 (GDI)
void DrawRoundedRect(HDC hdc, int x, int y, int w, int h, int rx, int ry, COLORREF fillCol, COLORREF borderCol, int borderThickness) {
    HPEN hPen = (borderThickness > 0) ? CreatePen(PS_SOLID, borderThickness, borderCol) : CreatePen(PS_NULL, 0, 0);
    HBRUSH hBrush = CreateSolidBrush(fillCol);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    RoundRect(hdc, x, y, x + w, y + h, rx, ry);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

// 繪製文字輔助函數
void DrawTextHelper(HDC hdc, const WCHAR* text, int x, int y, int w, int h, HFONT hFont, COLORREF color, UINT alignFlags) {
    RECT rect = {x, y, x + w, y + h};
    SelectObject(hdc, hFont);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text, -1, &rect, alignFlags);
}

// 視窗客製化繪製 (雙緩衝)
void OnPaint(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    int width = rect.right;
    int height = rect.bottom;

    // 建立雙緩衝 DC
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // 1. 填滿主背景
    HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
    FillRect(hdcMem, &rect, hBgBrush);
    DeleteObject(hBgBrush);

    // === 標題列繪製 ===
    DrawTextHelper(hdcMem, L"管理員模式運行", Scale(20), Scale(15), Scale(600), Scale(30), g_FontTitle, COLOR_TEXT_PRI, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // === 三大核心儀表板卡片 (動態佈局) ===
    int cardY = g_CardY;
    int cardH = g_CardH;
    int cardW = g_CardW;
    int cardGap = Scale(30);

    // --- 卡片 1：實體記憶體 ---
    int card1X = g_Padding;
    int card2X = g_Padding;
    int card3X = g_Padding;
    int card3W = cardW;
    int card2Y = cardY + cardH + cardGap;
    int card3Y = card2Y + cardH + cardGap;

    DrawRoundedRect(hdcMem, card1X, cardY, cardW, cardH, Scale(8), Scale(8), COLOR_CARD, COLOR_CARD_BORDER, 1);
    DrawTextHelper(hdcMem, L"實體記憶體用量", card1X + Scale(15), cardY + Scale(12), cardW - Scale(30), Scale(20), g_FontHeader, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);
    
    double totalPhysGB = (double)g_Stats.totalPhys / (1024.0 * 1024.0 * 1024.0);
    double usedPhysGB = (double)(g_Stats.totalPhys - g_Stats.availPhys) / (1024.0 * 1024.0 * 1024.0);
    WCHAR buf[128];
    swprintf(buf, 128, L"已用 %.2f GB / 總共 %.2f GB", usedPhysGB, totalPhysGB);
    DrawTextHelper(hdcMem, buf, card1X + Scale(15), cardY + Scale(35), cardW - Scale(30), Scale(20), g_FontNormal, COLOR_TEXT_PRI, DT_LEFT | DT_SINGLELINE);
    
    swprintf(buf, 128, L"%u%%", g_Stats.memoryLoad);
    DrawTextHelper(hdcMem, buf, card1X + cardW - Scale(50), cardY + Scale(12), Scale(35), Scale(20), g_FontHeader, COLOR_ACCENT, DT_RIGHT | DT_SINGLELINE);

    int barY = cardY + cardH - Scale(18);
    int barW = cardW - Scale(30);
    int barH = Scale(8);
    DrawRoundedRect(hdcMem, card1X + Scale(15), barY, barW, barH, Scale(4), Scale(4), RGB(40, 40, 50), 0, 0);
    int fillW = (int)((double)barW * (g_Stats.memoryLoad / 100.0));
    COLORREF memColor = (g_Stats.memoryLoad > 85) ? COLOR_CRITICAL : ((g_Stats.memoryLoad > 65) ? COLOR_CAUTION : COLOR_SAFE);
    if (fillW > 0) {
        DrawRoundedRect(hdcMem, card1X + Scale(15), barY, fillW, barH, Scale(4), Scale(4), memColor, 0, 0);
    }

    // --- 卡片 2：虛擬記憶體 ---
    DrawRoundedRect(hdcMem, card2X, card2Y, cardW, cardH, Scale(8), Scale(8), COLOR_CARD, COLOR_CARD_BORDER, 1);
    DrawTextHelper(hdcMem, L"分頁檔 / Commit Charge", card2X + Scale(15), card2Y + Scale(12), cardW - Scale(30), Scale(20), g_FontHeader, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

    double totalPageGB = (double)g_Stats.totalPageFile / (1024.0 * 1024.0 * 1024.0);
    double usedPageGB = (double)(g_Stats.totalPageFile - g_Stats.availPageFile) / (1024.0 * 1024.0 * 1024.0);
    swprintf(buf, 128, L"已配置 %.2f GB / 限制 %.2f GB", usedPageGB, totalPageGB);
    DrawTextHelper(hdcMem, buf, card2X + Scale(15), card2Y + Scale(35), cardW - Scale(30), Scale(20), g_FontNormal, COLOR_TEXT_PRI, DT_LEFT | DT_SINGLELINE);

    double commitRatio = totalPageGB > 0 ? (usedPageGB / totalPageGB) : 0.0;
    swprintf(buf, 128, L"%d%%", (int)(commitRatio * 100.0));
    DrawTextHelper(hdcMem, buf, card2X + cardW - Scale(50), card2Y + Scale(12), Scale(35), Scale(20), g_FontHeader, COLOR_ACCENT, DT_RIGHT | DT_SINGLELINE);

    DrawRoundedRect(hdcMem, card2X + Scale(15), card2Y + cardH - Scale(18), barW, barH, Scale(4), Scale(4), RGB(40, 40, 50), 0, 0);
    int fillW2 = (int)((double)barW * commitRatio);
    if (fillW2 > 0) {
        DrawRoundedRect(hdcMem, card2X + Scale(15), card2Y + cardH - Scale(18), fillW2, barH, Scale(4), Scale(4), COLOR_ACCENT, 0, 0);
    }

    // --- 卡片 3：碎片化估計 ---
    DrawRoundedRect(hdcMem, card3X, card3Y, card3W, cardH, Scale(8), Scale(8), COLOR_CARD, COLOR_CARD_BORDER, 1);
    DrawTextHelper(hdcMem, L"記憶體碎片化分數", card3X + Scale(15), card3Y + Scale(12), card3W - Scale(30), Scale(20), g_FontHeader, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

    swprintf(buf, 128, L"%.1f%%", g_Stats.fragmentationScore);
    DrawTextHelper(hdcMem, buf, card3X + card3W - Scale(70), card3Y + Scale(12), Scale(55), Scale(20), g_FontHeader, COLOR_ACCENT, DT_RIGHT | DT_SINGLELINE);

    COLORREF fragColor;
    const WCHAR* fragLevel;
    if (g_Stats.fragmentationScore > 50.0) {
        fragColor = COLOR_CRITICAL;
        fragLevel = L"狀態：重度碎片化 (建議加速)";
    } else if (g_Stats.fragmentationScore > 25.0) {
        fragColor = COLOR_CAUTION;
        fragLevel = L"狀態：中度碎片化";
    } else {
        fragColor = COLOR_SAFE;
        fragLevel = L"狀態：極佳 (碎片率低)";
    }
    DrawTextHelper(hdcMem, fragLevel, card3X + Scale(15), card3Y + Scale(35), card3W - Scale(30), Scale(20), g_FontNormal, fragColor, DT_LEFT | DT_SINGLELINE);

    int barW3 = card3W - Scale(30);
    DrawRoundedRect(hdcMem, card3X + Scale(15), card3Y + cardH - Scale(18), barW3, barH, Scale(4), Scale(4), RGB(40, 40, 50), 0, 0);
    int fillW3 = (int)((double)barW3 * (g_Stats.fragmentationScore / 100.0));
    if (fillW3 > 0) {
        DrawRoundedRect(hdcMem, card3X + Scale(15), card3Y + cardH - Scale(18), fillW3, barH, Scale(4), Scale(4), fragColor, 0, 0);
    }

    // === 各項歷史趨勢（分別對應每張卡片下方） ===
    int miniChartH = min(Scale(200), cardH - Scale(40));
    miniChartH = max(miniChartH, Scale(100));
    int miniChartX = card1X + Scale(15);
    int miniChartW = barW;
    int miniBorder = Scale(3);

    // 卡片 1：記憶體歷史
    int miniChartGap = Scale(14);
    int memTrendY = barY - miniChartGap - miniChartH;
    DrawRoundedRect(hdcMem, miniChartX, memTrendY, miniChartW, miniChartH, miniBorder, miniBorder, RGB(24, 30, 38), COLOR_CARD_BORDER, 1);
    int plotLeft = miniChartX + Scale(8);
    int plotRight = miniChartX + miniChartW - Scale(8);
    int plotBottom = memTrendY + miniChartH - Scale(8);
    int plotTop = memTrendY + Scale(8);
    int plotHeight = plotBottom - plotTop;
    int plotWidth = plotRight - plotLeft;

    // 繪製座標軸線與刻度數字
    HPEN hAxisPen = CreatePen(PS_SOLID, 1, COLOR_TEXT_MUTED);
    HPEN hOldAxisPen = (HPEN)SelectObject(hdcMem, hAxisPen);
    MoveToEx(hdcMem, plotLeft, plotTop, NULL);
    LineTo(hdcMem, plotLeft, plotBottom);
    LineTo(hdcMem, plotRight, plotBottom);
    SelectObject(hdcMem, hOldAxisPen);
    DeleteObject(hAxisPen);

    HPEN hGridPen = CreatePen(PS_DOT, 1, RGB(64, 64, 72));
    HPEN hOldGridPen = (HPEN)SelectObject(hdcMem, hGridPen);
    for (int tick = 10; tick < 100; tick += 10) {
        int tickY = plotBottom - (plotHeight * tick) / 100;
        MoveToEx(hdcMem, plotLeft, tickY, NULL);
        LineTo(hdcMem, plotRight, tickY);
    }
    for (int sec = 0; sec <= 60; sec += 10) {
        int tickX = plotLeft + (plotWidth * sec) / 60;
        MoveToEx(hdcMem, tickX, plotTop, NULL);
        LineTo(hdcMem, tickX, plotBottom);
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%ds", sec);
        DrawTextHelper(hdcMem, tickLabel, tickX - Scale(12), plotBottom + Scale(2), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    SelectObject(hdcMem, hOldGridPen);
    DeleteObject(hGridPen);

    for (int tick = 0; tick <= 100; tick += 10) {
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%d%%", tick);
        int tickY = plotBottom - (plotHeight * tick) / 100;
        DrawTextHelper(hdcMem, tickLabel, plotLeft - Scale(2), tickY - Scale(7), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    HPEN hMemPen = CreatePen(PS_SOLID, 2, COLOR_ACCENT);
    HPEN hOldMemPen = (HPEN)SelectObject(hdcMem, hMemPen);
    for (int i = 0; i < 60; i++) {
        int px = plotLeft + (i * plotWidth) / 59;
        int py = plotBottom - (int)((g_MemHistory[i] * plotHeight) / 100.0f);
        if (i == 0) MoveToEx(hdcMem, px, py, NULL);
        else LineTo(hdcMem, px, py);
    }
    SelectObject(hdcMem, hOldMemPen);
    DeleteObject(hMemPen);
    DrawTextHelper(hdcMem, L"記憶體趨勢", miniChartX, memTrendY - Scale(18), miniChartW, Scale(16), g_FontSmall, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

    // 卡片 2：分頁檔歷史
    int pageMiniChartX = card2X + Scale(15);
    int pageBarY = card2Y + cardH - Scale(18);
    int pageTrendY = pageBarY - miniChartGap - miniChartH;
    DrawRoundedRect(hdcMem, pageMiniChartX, pageTrendY, miniChartW, miniChartH, miniBorder, miniBorder, RGB(24, 30, 38), COLOR_CARD_BORDER, 1);
    HPEN hPageLinePen = CreatePen(PS_SOLID, 1, RGB(56, 189, 248));
    HPEN hOldPageLinePen = (HPEN)SelectObject(hdcMem, hPageLinePen);
    plotLeft = pageMiniChartX + Scale(8);
    plotRight = pageMiniChartX + miniChartW - Scale(8);
    plotBottom = pageTrendY + miniChartH - Scale(8);
    plotTop = pageTrendY + Scale(8);
    plotHeight = plotBottom - plotTop;
    plotWidth = plotRight - plotLeft;

    HPEN hPageAxisPen = CreatePen(PS_SOLID, 1, COLOR_TEXT_MUTED);
    HPEN hOldPageAxisPen = (HPEN)SelectObject(hdcMem, hPageAxisPen);
    MoveToEx(hdcMem, plotLeft, plotTop, NULL);
    LineTo(hdcMem, plotLeft, plotBottom);
    LineTo(hdcMem, plotRight, plotBottom);
    SelectObject(hdcMem, hOldPageAxisPen);
    DeleteObject(hPageAxisPen);

    HPEN hPageGridPen = CreatePen(PS_DOT, 1, RGB(64, 64, 72));
    HPEN hOldPageGridPen = (HPEN)SelectObject(hdcMem, hPageGridPen);
    for (int tick = 10; tick < 100; tick += 10) {
        int tickY = plotBottom - (plotHeight * tick) / 100;
        MoveToEx(hdcMem, plotLeft, tickY, NULL);
        LineTo(hdcMem, plotRight, tickY);
    }
    for (int sec = 0; sec <= 60; sec += 10) {
        int tickX = plotLeft + (plotWidth * sec) / 60;
        MoveToEx(hdcMem, tickX, plotTop, NULL);
        LineTo(hdcMem, tickX, plotBottom);
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%ds", sec);
        DrawTextHelper(hdcMem, tickLabel, tickX - Scale(12), plotBottom + Scale(2), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    SelectObject(hdcMem, hOldPageGridPen);
    DeleteObject(hPageGridPen);

    for (int tick = 0; tick <= 100; tick += 10) {
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%d%%", tick);
        int tickY = plotBottom - (plotHeight * tick) / 100;
        DrawTextHelper(hdcMem, tickLabel, plotLeft - Scale(2), tickY - Scale(7), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdcMem, hOldPageLinePen);
    DeleteObject(hPageLinePen);

    HPEN hPagePen = CreatePen(PS_SOLID, 2, RGB(56, 189, 248));
    HPEN hOldPagePen = (HPEN)SelectObject(hdcMem, hPagePen);
    for (int i = 0; i < 60; i++) {
        int px = plotLeft + (i * plotWidth) / 59;
        int py = plotBottom - (int)((g_PageHistory[i] * plotHeight) / 100.0f);
        if (i == 0) MoveToEx(hdcMem, px, py, NULL);
        else LineTo(hdcMem, px, py);
    }
    SelectObject(hdcMem, hOldPagePen);
    DeleteObject(hPagePen);
    DrawTextHelper(hdcMem, L"分頁檔趨勢", pageMiniChartX, pageTrendY - Scale(18), miniChartW, Scale(16), g_FontSmall, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

    // 卡片 3：碎片化歷史
    int fragMiniChartX = card3X + Scale(15);
    int fragMiniChartW = miniChartW;
    int fragBarY = card3Y + cardH - Scale(18);
    int fragTrendY = fragBarY - miniChartGap - miniChartH;
    DrawRoundedRect(hdcMem, fragMiniChartX, fragTrendY, fragMiniChartW, miniChartH, miniBorder, miniBorder, RGB(24, 30, 38), COLOR_CARD_BORDER, 1);
    HPEN hFragLinePen = CreatePen(PS_SOLID, 1, COLOR_CAUTION);
    HPEN hOldFragLinePen = (HPEN)SelectObject(hdcMem, hFragLinePen);
    plotLeft = fragMiniChartX + Scale(8);
    plotRight = fragMiniChartX + fragMiniChartW - Scale(8);
    plotBottom = fragTrendY + miniChartH - Scale(8);
    plotTop = fragTrendY + Scale(8);
    plotHeight = plotBottom - plotTop;
    plotWidth = plotRight - plotLeft;

    HPEN hFragAxisPen = CreatePen(PS_SOLID, 1, COLOR_TEXT_MUTED);
    HPEN hOldFragAxisPen = (HPEN)SelectObject(hdcMem, hFragAxisPen);
    MoveToEx(hdcMem, plotLeft, plotTop, NULL);
    LineTo(hdcMem, plotLeft, plotBottom);
    LineTo(hdcMem, plotRight, plotBottom);
    SelectObject(hdcMem, hOldFragAxisPen);
    DeleteObject(hFragAxisPen);

    HPEN hFragGridPen = CreatePen(PS_DOT, 1, RGB(64, 64, 72));
    HPEN hOldFragGridPen = (HPEN)SelectObject(hdcMem, hFragGridPen);
    for (int tick = 10; tick < 100; tick += 10) {
        int tickY = plotBottom - (plotHeight * tick) / 100;
        MoveToEx(hdcMem, plotLeft, tickY, NULL);
        LineTo(hdcMem, plotRight, tickY);
    }
    for (int sec = 0; sec <= 60; sec += 10) {
        int tickX = plotLeft + (plotWidth * sec) / 60;
        MoveToEx(hdcMem, tickX, plotTop, NULL);
        LineTo(hdcMem, tickX, plotBottom);
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%ds", sec);
        DrawTextHelper(hdcMem, tickLabel, tickX - Scale(12), plotBottom + Scale(2), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    SelectObject(hdcMem, hOldFragGridPen);
    DeleteObject(hFragGridPen);

    for (int tick = 0; tick <= 100; tick += 10) {
        WCHAR tickLabel[8];
        swprintf(tickLabel, 8, L"%d%%", tick);
        int tickY = plotBottom - (plotHeight * tick) / 100;
        DrawTextHelper(hdcMem, tickLabel, plotLeft - Scale(2), tickY - Scale(7), Scale(40), Scale(14), g_FontSmall, COLOR_TEXT_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdcMem, hOldFragLinePen);
    DeleteObject(hFragLinePen);

    HPEN hFragPen = CreatePen(PS_SOLID, 2, COLOR_CAUTION);
    HPEN hOldFragPen = (HPEN)SelectObject(hdcMem, hFragPen);
    for (int i = 0; i < 60; i++) {
        int px = plotLeft + (i * plotWidth) / 59;
        int py = plotBottom - (int)((g_FragHistory[i] * plotHeight) / 100.0f);
        if (i == 0) MoveToEx(hdcMem, px, py, NULL);
        else LineTo(hdcMem, px, py);
    }
    SelectObject(hdcMem, hOldFragPen);
    DeleteObject(hFragPen);
    DrawTextHelper(hdcMem, L"碎片化趨勢", fragMiniChartX, fragTrendY - Scale(18), fragMiniChartW, Scale(16), g_FontSmall, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

    // === 控制區域 (按鈕置中並隨寬度調整) ===
    int optW = g_OptW;
    int optH = g_OptH;
    int optX = g_OptX;
    int optY = g_OptY;

    COLORREF btnFill = g_IsOptimizing ? COLOR_CARD_BORDER : 
                      (g_OptimizeBtnActive ? COLOR_ACCENT_ACT : 
                      (g_OptimizeBtnHover ? COLOR_ACCENT_HOV : COLOR_ACCENT));
    DrawRoundedRect(hdcMem, optX, optY, optW, optH, Scale(21), Scale(21), btnFill, 0, 0);
    DrawTextHelper(hdcMem, g_IsOptimizing ? L"正在全力優化系統記憶體..." : L"立即進行一鍵加速與碎片整理", 
                   optX, optY, optW, optH, g_FontHeader, COLOR_TEXT_PRI, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 重新整理按鈕 (靠右)
    int refW = g_RefW;
    int refH = g_RefH;
    int refX = g_RefX;
    int refY = g_RefY;
    COLORREF refFill = g_RefreshBtnHover ? COLOR_LIST_HOVER : COLOR_CARD;
    DrawRoundedRect(hdcMem, refX, refY, refW, refH, Scale(8), Scale(8), refFill, COLOR_CARD_BORDER, 1);
    DrawTextHelper(hdcMem, L"↻", refX, refY - Scale(1), refW, refH, g_FontTitle, COLOR_TEXT_PRI, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 狀態橫幅 (動態寬度)
    DrawTextHelper(hdcMem, g_StatusMessage, g_Padding, g_ChartY + g_ChartH + Scale(12), g_LeftW, Scale(18), g_FontNormal, g_StatusColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // === 行程清單區段標題與分頁 ===
    HPEN hSepPen = CreatePen(PS_SOLID, 1, COLOR_LIST_SEP);
    HPEN hOldSepPen = (HPEN)SelectObject(hdcMem, hSepPen);
    MoveToEx(hdcMem, g_Padding, optY + optH + Scale(40), NULL);
    LineTo(hdcMem, g_Padding + g_AvailableW, optY + optH + Scale(40));
    SelectObject(hdcMem, hOldSepPen);
    DeleteObject(hSepPen);

    // 繪製篩選分頁 Tabs
    int tabY = g_TabY;
    int tabH = g_TabH;
    int tabW[4] = { g_TabWArr[0], g_TabWArr[1], g_TabWArr[2], g_TabWArr[3] };
    const WCHAR* tabText[4] = {L"全部行程", L"安全可關閉", L"建議保留", L"系統核心"};
    int tabX = g_ListX;

    for (int i = 0; i < 4; i++) {
        BOOL isActive = (g_Filter == i);
        BOOL isHover = (g_HoveredTab == i);
        
        COLORREF tabBg = isActive ? COLOR_ACCENT_BG : (isHover ? COLOR_LIST_HOVER : COLOR_BG);
        COLORREF tabBorder = isActive ? COLOR_ACCENT : COLOR_CARD_BORDER;
        COLORREF tabTextCol = isActive ? COLOR_TEXT_PRI : COLOR_TEXT_SEC;
        
        DrawRoundedRect(hdcMem, tabX, tabY, tabW[i], tabH, Scale(6), Scale(6), tabBg, tabBorder, 1);
        DrawTextHelper(hdcMem, tabText[i], tabX, tabY, tabW[i], tabH, g_FontNormal, tabTextCol, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        tabX += tabW[i] + Scale(8);
    }

    // 搜尋框背景與邊框
    int searchBoxY = g_TabY + (g_TabH - Scale(26)) / 2;
    COLORREF searchBg = RGB(48, 52, 64);
    DrawRoundedRect(hdcMem, g_SearchX, searchBoxY, g_SearchW, Scale(26), Scale(6), Scale(6), searchBg, COLOR_CARD_BORDER, 1);

    // === 行程清單標題列 ===
    int listHeaderY = g_ListY - Scale(26);
    int listHeaderH = Scale(18);
    // 分欄寬度按比例分配
    // 調整欄位比例，優先保留第一欄名稱空間
    int col1W = (int)(g_ListW * 0.36);
    int col2W = (int)(g_ListW * 0.12);
    int col3W = (int)(g_ListW * 0.10);
    int col4W = g_ListW - (col1W + col2W + col3W) - Scale(60); // 留一些空間給動作欄
    int col1X = g_ListX + Scale(10);
    int col2X = col1X + col1W + Scale(10);
    int col3X = col2X + col2W + Scale(10);
    int col4X = col3X + col3W + Scale(10);
    int actionX = g_ListX + g_ListW - Scale(80);

    DrawTextHelper(hdcMem, L"行程名稱 (PID)", col1X, listHeaderY, col1W, listHeaderH, g_FontSmall, COLOR_TEXT_MUTED, DT_LEFT | DT_SINGLELINE);
    DrawTextHelper(hdcMem, L"實體記憶體", col2X, listHeaderY, col2W, listHeaderH, g_FontSmall, COLOR_TEXT_MUTED, DT_LEFT | DT_SINGLELINE);
    DrawTextHelper(hdcMem, L"安全等級", col3X, listHeaderY, col3W, listHeaderH, g_FontSmall, COLOR_TEXT_MUTED, DT_LEFT | DT_SINGLELINE);
    DrawTextHelper(hdcMem, L"運行時長", col4X, listHeaderY, col4W, listHeaderH, g_FontSmall, COLOR_TEXT_MUTED, DT_LEFT | DT_SINGLELINE);
    DrawTextHelper(hdcMem, L"操作", actionX, listHeaderY, Scale(60), listHeaderH, g_FontSmall, COLOR_TEXT_MUTED, DT_CENTER | DT_SINGLELINE);

    // === 行程清單內容 ===
    int listX = g_ListX;
    int listY = g_ListY;
    int listW = g_ListW;
    int listH = g_ListH;
    int rowH = g_RowH;

    HRGN hRgn = CreateRectRgn(listX, listY, listX + listW, listY + listH);
    SelectClipRgn(hdcMem, hRgn);

    for (int i = 0; i < g_FilteredProcessCount; i++) {
        int rowY = listY + i * rowH - g_ScrollOffset;
        if (rowY + rowH < listY || rowY > listY + listH) continue;

        ProcessInfo* proc = &g_FilteredProcesses[i];
        BOOL isRowHover = (g_HoveredRow == i);

        COLORREF rowBg = isRowHover ? COLOR_LIST_HOVER : COLOR_BG;
        RECT rowRect = {listX, rowY, listX + listW, rowY + rowH};
        HBRUSH hRowBrush = CreateSolidBrush(rowBg);
        FillRect(hdcMem, &rowRect, hRowBrush);
        DeleteObject(hRowBrush);

        HPEN hRowPen = CreatePen(PS_SOLID, 1, COLOR_LIST_SEP);
        HPEN hOldRowPen = (HPEN)SelectObject(hdcMem, hRowPen);
        MoveToEx(hdcMem, listX, rowY + rowH - 1, NULL);
        LineTo(hdcMem, listX + listW, rowY + rowH - 1);
        SelectObject(hdcMem, hOldRowPen);
        DeleteObject(hRowPen);

        int iconD = Scale(26);
        int iconX = listX + Scale(10);
        int iconY = rowY + (rowH - iconD) / 2;
        COLORREF badgeBg = (proc->safety == SAFETY_CRITICAL) ? COLOR_CRITICAL_BG : 
                          ((proc->safety == SAFETY_PROTECTED) ? COLOR_PROTECT_BG :
                          ((proc->safety == SAFETY_CAUTION) ? COLOR_CAUTION_BG : COLOR_SAFE_BG));
        COLORREF badgeBorder = (proc->safety == SAFETY_CRITICAL) ? COLOR_CRITICAL : 
                             ((proc->safety == SAFETY_PROTECTED) ? COLOR_PROTECT :
                             ((proc->safety == SAFETY_CAUTION) ? COLOR_CAUTION : COLOR_SAFE));
        
        DrawRoundedRect(hdcMem, iconX, iconY, iconD, iconD, iconD, iconD, badgeBg, badgeBorder, 1);
        
        WCHAR initial[2] = {L'\0', L'\0'};
        if (proc->name[0] != L'\0') initial[0] = towupper(proc->name[0]);
        DrawTextHelper(hdcMem, initial, iconX, iconY - Scale(1), iconD, iconD, g_FontHeader, COLOR_TEXT_PRI, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        WCHAR nameBuf[256];
        swprintf(nameBuf, 256, L"%s", proc->name);
        int nameX = iconX + iconD + Scale(10);
        int nameW = col1W - (iconD + Scale(30));
        if (nameW < Scale(80)) nameW = Scale(80); // 最小寬度避免過度壓縮
        // 確保名稱欄不會超過欄位右邊界
        int nameMaxW = (col1X + col1W) - nameX - Scale(10);
        if (nameMaxW < Scale(40)) nameMaxW = Scale(40);
        if (nameW > nameMaxW) nameW = nameMaxW;
        DrawTextHelper(hdcMem, nameBuf, nameX, rowY + Scale(8), nameW, Scale(18), g_FontHeader, COLOR_TEXT_PRI, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        WCHAR pidBuf[64];
        swprintf(pidBuf, 64, L"PID: %u", proc->pid);
        DrawTextHelper(hdcMem, pidBuf, nameX, rowY + Scale(24), nameW, Scale(15), g_FontSmall, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE);

        double memMB = (double)proc->memoryUsage / (1024.0 * 1024.0);
        WCHAR memBuf[64];
        if (memMB >= 1024.0) swprintf(memBuf, 64, L"%.2f GB", memMB / 1024.0);
        else swprintf(memBuf, 64, L"%.1f MB", memMB);
        DrawTextHelper(hdcMem, memBuf, col2X + Scale(6), rowY + (rowH - Scale(18)) / 2, col2W - Scale(12), Scale(18), g_FontNormal, COLOR_TEXT_PRI, DT_LEFT | DT_SINGLELINE);

        COLORREF labelCol = badgeBorder;
        const WCHAR* labelText = (proc->safety == SAFETY_CRITICAL) ? L"● 核心不可關閉" : 
                                ((proc->safety == SAFETY_PROTECTED) ? L"✓ 安全保護中" :
                                ((proc->safety == SAFETY_CAUTION) ? L"▲ 建議保留" : L"✓ 安全可關閉"));
        DrawTextHelper(hdcMem, labelText, col3X + Scale(6), rowY + Scale(6), col3W - Scale(12), Scale(18), g_FontNormal, labelCol, DT_LEFT | DT_SINGLELINE);

        // 行程歷時顯示
        int durationW = col4W - Scale(12);
        if (durationW < Scale(40)) durationW = Scale(40);
        DrawTextHelper(hdcMem, proc->runDuration, col4X + Scale(6), rowY + Scale(6), durationW, Scale(18), g_FontNormal, COLOR_TEXT_SEC, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        int btnW = Scale(60);
        int btnH = Scale(24);
        int btnX = listX + listW - btnW - Scale(15);
        int btnY = rowY + (rowH - btnH) / 2;

        if (proc->safety == SAFETY_CRITICAL) {
            DrawTextHelper(hdcMem, L"系統鎖定", btnX, btnY, btnW, btnH, g_FontNormal, COLOR_TEXT_MUTED, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (proc->safety == SAFETY_PROTECTED) {
            DrawRoundedRect(hdcMem, btnX, btnY, btnW, btnH, Scale(6), Scale(6), COLOR_PROTECT_BG, COLOR_PROTECT, 1);
            DrawTextHelper(hdcMem, L"已保護", btnX, btnY - Scale(1), btnW, btnH, g_FontNormal, COLOR_PROTECT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            BOOL isBtnHover = (g_HoveredBtn && g_HoveredBtnRow == i);
            COLORREF borderCol = (proc->safety == SAFETY_CAUTION) ? COLOR_CAUTION : COLOR_SAFE;
            COLORREF fillCol = isBtnHover ? borderCol : COLOR_BG;
            COLORREF textCol = isBtnHover ? (proc->safety == SAFETY_CAUTION ? RGB(0,0,0) : COLOR_TEXT_PRI) : borderCol;
            DrawRoundedRect(hdcMem, btnX, btnY, btnW, btnH, Scale(6), Scale(6), fillCol, borderCol, 1);
            DrawTextHelper(hdcMem, L"結束", btnX, btnY - Scale(1), btnW, btnH, g_FontNormal, textCol, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    SelectClipRgn(hdcMem, NULL);
    DeleteObject(hRgn);

    int scrollBarX = listX + listW - Scale(8);
    int scrollBarW = Scale(6);
    DrawRoundedRect(hdcMem, scrollBarX, listY, scrollBarW, listH, Scale(3), Scale(3), COLOR_SCROLL_BAR, 0, 0);

    int totalListHeight = g_FilteredProcessCount * rowH;
    if (totalListHeight > listH) {
        int thumbH = (listH * listH) / totalListHeight;
        if (thumbH < Scale(20)) thumbH = Scale(20);
        int maxOffset = totalListHeight - listH;
        int thumbY = listY + (g_ScrollOffset * (listH - thumbH)) / maxOffset;

        COLORREF thumbCol = g_IsDraggingScroll ? COLOR_TEXT_PRI : COLOR_SCROLL_THM;
        DrawRoundedRect(hdcMem, scrollBarX, thumbY, scrollBarW, thumbH, Scale(3), Scale(3), thumbCol, 0, 0);
    }

    // 複製雙緩衝到螢幕
    BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

    // 釋放資源
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// 處理一鍵優化邏輯 (使用執行緒或短暫同步優化)
DWORD WINAPI OptimizeThreadFunc(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;
    
    // 獲取優化前的實體記憶體可用量
    MemoryStats beforeStats;
    GetSystemMemoryStats(&beforeStats);

    // 執行記憶體工作集與快取整理
    OptimizeSystemMemory(hwnd);

    // 短暫等待讓系統回收生效
    Sleep(500);

    // 獲取優化後的實體記憶體
    MemoryStats afterStats;
    GetSystemMemoryStats(&afterStats);

    LONGLONG freedBytes = (LONGLONG)afterStats.availPhys - (LONGLONG)beforeStats.availPhys;
    if (freedBytes < 0) freedBytes = 0;
    double freedMB = (double)freedBytes / (1024.0 * 1024.0);

    // 更新狀態訊息
    g_IsOptimizing = FALSE;
    if (freedMB > 50.0) {
        swprintf(g_StatusMessage, 256, L"加速成功！本次優化釋放了 %.1f MB 記憶體空間，碎片率已降低。", freedMB);
        g_StatusColor = COLOR_SAFE;
    } else {
        wcscpy(g_StatusMessage, L"系統已處於最優狀態，或部分應用程式工作集已保留。");
        g_StatusColor = COLOR_ACCENT;
    }

    // 重設清除狀態的計時器
    SetTimer(hwnd, TIMER_CLEAR_STATUS, 6000, NULL);

    // 重新載入行程與重繪
    PostMessage(hwnd, WM_COMMAND, 9999, 0); // 自訂重新整理代號

    return 0;
}

// 主視窗程序
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_Dpi = GetDpiForWindow(hwnd);
            UpdateFonts(hwnd);

            // 建立標準搜尋輸入方塊 (Edit Control)
            // 平移 70px 至 323
            g_hSearchEdit = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                0, 0, Scale(225), Scale(26),
                hwnd, (HMENU)IDC_SEARCH_EDIT, g_hInstance, NULL
            );

            // 設定搜尋編輯框的現代化字型
            SendMessageW(g_hSearchEdit, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

            // 使用系統 Cue Banner 增加搜尋提示文字
            SendMessageW(g_hSearchEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"🔍 輸入名稱模糊搜尋...");

            // 計算並初始化動態佈局
            UpdateLayout(hwnd);

            // 載入自訂白名單檔案
            LoadWhitelist();

            // 獲取初始資料
            GetSystemMemoryStats(&g_Stats);
            double pageFileLoad = 0.0;
            if (g_Stats.totalPageFile > 0) {
                pageFileLoad = ((double)(g_Stats.totalPageFile - g_Stats.availPageFile) / (double)g_Stats.totalPageFile) * 100.0;
            }
            for (int i = 0; i < 60; i++) {
                g_MemHistory[i] = (float)g_Stats.memoryLoad;
                g_PageHistory[i] = (float)pageFileLoad;
                g_FragHistory[i] = (float)g_Stats.fragmentationScore;
            }

            RefreshData(hwnd);

            // 設定統計更新計時器 (每秒)
            SetTimer(hwnd, TIMER_STATS_UPDATE, 1000, NULL);
            // 設定行程更新計時器 (每8秒，且滑鼠不在行程上時才觸發)
            SetTimer(hwnd, TIMER_PROC_UPDATE, 8000, NULL);

            return 0;
        }

        case WM_DPICHANGED: {
            g_Dpi = LOWORD(wParam);
            UpdateFonts(hwnd);
            // 重新計算佈局並調整控制項
            UpdateLayout(hwnd);
            RefreshData(hwnd);
            return 0;
        }

        // 修改輸入框背景與文字顏色 (配合深色主題)
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, COLOR_TEXT_PRI);
            SetBkColor(hdcEdit, COLOR_CARD);
            static HBRUSH hEditBg = NULL;
            if (hEditBg) DeleteObject(hEditBg);
            hEditBg = CreateSolidBrush(COLOR_CARD);
            return (INT_PTR)hEditBg;
        }

        case WM_COMMAND: {
            // 處理搜尋編輯框內容改變
            if (LOWORD(wParam) == IDC_SEARCH_EDIT && HIWORD(wParam) == EN_CHANGE) {
                GetWindowTextW(g_hSearchEdit, g_SearchQuery, 128);
                ApplyFilterAndSearch();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // 自訂代號：背景優化完成後重新整理
            if (LOWORD(wParam) == 9999) {
                RefreshData(hwnd);
                return 0;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == TIMER_STATS_UPDATE) {
                // 每秒靜態更新記憶體統計資料，並移位更新折線圖歷史
                GetSystemMemoryStats(&g_Stats);
                
                for (int i = 0; i < 59; i++) {
                    g_MemHistory[i] = g_MemHistory[i + 1];
                    g_PageHistory[i] = g_PageHistory[i + 1];
                    g_FragHistory[i] = g_FragHistory[i + 1];
                }
                g_MemHistory[59] = (float)g_Stats.memoryLoad;
                g_PageHistory[59] = (float)((g_Stats.totalPageFile > 0)
                    ? ((double)(g_Stats.totalPageFile - g_Stats.availPageFile) / (double)g_Stats.totalPageFile) * 100.0
                    : 0.0);
                g_FragHistory[59] = (float)g_Stats.fragmentationScore;
                
                // 只重繪卡片區域與折線圖卡片區，省去重繪整個視窗
                RECT client;
                GetClientRect(hwnd, &client);
                RECT redrawRect = {0, g_CardY, client.right, g_ChartY + g_ChartH};
                InvalidateRect(hwnd, &redrawRect, FALSE);
            }
            else if (wParam == TIMER_PROC_UPDATE) {
                // 若滑鼠不在行程清單上，才自動載入最新行程
                if (g_HoveredRow == -1 && !g_IsDraggingScroll) {
                    RefreshData(hwnd);
                }
            }
            else if (wParam == TIMER_CLEAR_STATUS) {
                // 清除狀態訊息，還原成預設提示
                KillTimer(hwnd, TIMER_CLEAR_STATUS);
                wcscpy(g_StatusMessage, L"系統狀態：就緒。建議在記憶體吃緊時進行一鍵加速。");
                g_StatusColor = COLOR_TEXT_SEC;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_SIZE: {
            // 重新計算佈局並重繪
            UpdateLayout(hwnd);
            ApplyFilterAndSearch();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // 1. 偵測一鍵加速按鈕
            BOOL inOpt = (x >= g_OptX && x <= g_OptX + g_OptW && y >= g_OptY && y <= g_OptY + g_OptH);
            if (inOpt != g_OptimizeBtnHover) {
                g_OptimizeBtnHover = inOpt;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // 2. 偵測重新整理按鈕 (平移 70px)
            BOOL inRef = (x >= g_RefX && x <= g_RefX + g_RefW && y >= g_RefY && y <= g_RefY + g_RefH);
            if (inRef != g_RefreshBtnHover) {
                g_RefreshBtnHover = inRef;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // 3. 偵測篩選分頁 Tabs (平移 70px)
            int tabY = g_TabY;
            int tabH = g_TabH;
            int tabW[4] = { g_TabWArr[0], g_TabWArr[1], g_TabWArr[2], g_TabWArr[3] };
            int tabX = g_ListX;
            int currentHoverTab = -1;

            for (int i = 0; i < 4; i++) {
                if (x >= tabX && x <= tabX + tabW[i] && y >= tabY && y <= tabY + tabH) {
                    currentHoverTab = i;
                    break;
                }
                tabX += tabW[i] + Scale(8);
            }

            if (currentHoverTab != g_HoveredTab) {
                g_HoveredTab = currentHoverTab;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            // 4. 偵測行程清單區域內的滑鼠懸停
            int listX = g_ListX;
            int listY = g_ListY;
            int listW = g_ListW;
            int listH = g_ListH;
            int rowH = g_RowH;

            int prevHoveredRow = g_HoveredRow;
            BOOL prevHoveredBtn = g_HoveredBtn;
            int prevHoveredBtnRow = g_HoveredBtnRow;

            g_HoveredRow = -1;
            g_HoveredBtn = FALSE;
            g_HoveredBtnRow = -1;

            if (x >= listX && x <= listX + listW && y >= listY && y <= listY + listH) {
                int relativeY = y - listY + g_ScrollOffset;
                int rowIdx = relativeY / rowH;
                if (rowIdx >= 0 && rowIdx < g_FilteredProcessCount) {
                    g_HoveredRow = rowIdx;

                    // 偵測是否懸停在「結束」按鈕上
                    int btnW = Scale(60);
                    int btnH = Scale(24);
                    int btnX = listX + listW - btnW - Scale(15);
                    int rowTopY = listY + rowIdx * rowH - g_ScrollOffset;
                    int btnY = rowTopY + (rowH - btnH) / 2;

                    // 只有非核心且非白名單保護的行程有可點擊的結束按鈕
                    if (g_FilteredProcesses[rowIdx].safety != SAFETY_CRITICAL && g_FilteredProcesses[rowIdx].safety != SAFETY_PROTECTED) {
                        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
                            g_HoveredBtn = TRUE;
                            g_HoveredBtnRow = rowIdx;
                        }
                    }
                }
            }

            // 若懸停狀態改變，重繪清單區域
            if (g_HoveredRow != prevHoveredRow || g_HoveredBtn != prevHoveredBtn || g_HoveredBtnRow != prevHoveredBtnRow) {
                RECT listRect = {listX, listY, listX + listW, listY + listH};
                InvalidateRect(hwnd, &listRect, FALSE);
            }

            // 5. 處理捲軸拖曳
            if (g_IsDraggingScroll) {
                int deltaY = y - g_DragStartY;
                int totalListHeight = g_FilteredProcessCount * rowH;
                int maxOffset = max(0, totalListHeight - listH);

                int thumbH = (listH * listH) / totalListHeight;
                if (thumbH < Scale(20)) thumbH = Scale(20);

                int scrollTrackH = listH - thumbH;
                if (scrollTrackH > 0) {
                    int newOffset = g_DragStartScrollOffset + (deltaY * maxOffset) / scrollTrackH;
                    if (newOffset < 0) newOffset = 0;
                    if (newOffset > maxOffset) newOffset = maxOffset;

                    if (g_ScrollOffset != newOffset) {
                        g_ScrollOffset = newOffset;
                        RECT listRect = {listX, listY, listX + listW, listY + listH};
                        InvalidateRect(hwnd, &listRect, FALSE);
                    }
                }
            }

            // 設定滑鼠離開視窗的追蹤
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            return 0;
        }

        case WM_MOUSELEAVE: {
            // 滑鼠離開，清除所有懸停高亮
            g_HoveredTab = -1;
            g_HoveredRow = -1;
            g_HoveredBtn = FALSE;
            g_HoveredBtnRow = -1;
            g_OptimizeBtnHover = FALSE;
            g_RefreshBtnHover = FALSE;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // 1. 按下一鍵優化 (平移 70px 至 Y: 240)
            if (g_OptimizeBtnHover && !g_IsOptimizing) {
                g_OptimizeBtnActive = TRUE;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // 2. 點擊篩選 Tab 分頁
            if (g_HoveredTab != -1) {
                g_Filter = g_HoveredTab;
                g_ScrollOffset = 0; // 重置滾動
                ApplyFilterAndSearch();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // 3. 點擊重新整理
            if (g_RefreshBtnHover) {
                RefreshData(hwnd);
                return 0;
            }

            // 4. 偵測並開始拖曳捲軸
            int listX = g_ListX;
            int listY = g_ListY;
            int listW = g_ListW;
            int listH = g_ListH;
            int rowH = g_RowH;
            int scrollBarX = listX + listW - Scale(8);
            int scrollBarW = Scale(6);

            int totalListHeight = g_FilteredProcessCount * rowH;

            if (totalListHeight > listH && x >= scrollBarX - Scale(2) && x <= scrollBarX + scrollBarW + Scale(2)) {
                int thumbH = (listH * listH) / totalListHeight;
                if (thumbH < Scale(20)) thumbH = Scale(20);
                int maxOffset = totalListHeight - listH;
                int thumbY = listY + (g_ScrollOffset * (listH - thumbH)) / maxOffset;

                if (y >= thumbY && y <= thumbY + thumbH) {
                    g_IsDraggingScroll = TRUE;
                    g_DragStartY = y;
                    g_DragStartScrollOffset = g_ScrollOffset;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            // 1. 放開一鍵加速按鈕
            if (g_OptimizeBtnActive) {
                g_OptimizeBtnActive = FALSE;
                if (g_OptimizeBtnHover) {
                    g_IsOptimizing = TRUE;
                    wcscpy(g_StatusMessage, L"正在進行背景清理，優化記憶體工作集與快取分頁...");
                    g_StatusColor = COLOR_ACCENT_HOV;
                    InvalidateRect(hwnd, NULL, FALSE);

                    // 開啟優化執行緒
                    CreateThread(NULL, 0, OptimizeThreadFunc, hwnd, 0, NULL);
                } else {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            // 2. 釋放捲軸拖曳
            if (g_IsDraggingScroll) {
                g_IsDraggingScroll = FALSE;
                ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // 3. 點擊結束行程按鈕
            if (g_HoveredBtn && g_HoveredBtnRow != -1) {
                ProcessInfo* proc = &g_FilteredProcesses[g_HoveredBtnRow];
                
                // 安全防護機制：若為黃色「建議保留」行程，彈出警告
                if (proc->safety == SAFETY_CAUTION) {
                    WCHAR msg[512];
                    swprintf(msg, 512, L"您即將強行結束行程 [%s] (PID: %u)。\n\n這是一個系統服務或 Windows 元件，強行結束可能導致 Windows 桌面重啟、部分系統服務失效。確定要繼續嗎？", proc->name, proc->pid);
                    if (MessageBoxW(hwnd, msg, L"系統安全警告", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDNO) {
                        return 0;
                    }
                }

                // 執行終止行程
                if (TerminateTargetProcess(proc->pid)) {
                    swprintf(g_StatusMessage, 256, L"成功結束行程 [%s] (PID: %u)，回收其佔用記憶體。", proc->name, proc->pid);
                    g_StatusColor = COLOR_SAFE;
                    
                    // 重新加載行程資料
                    RefreshData(hwnd);
                } else {
                    swprintf(g_StatusMessage, 256, L"無法結束行程 [%s] (PID: %u)，可能由於權限不足或行程已自行退出。", proc->name, proc->pid);
                    g_StatusColor = COLOR_CRITICAL;
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                SetTimer(hwnd, TIMER_CLEAR_STATUS, 5000, NULL);
                return 0;
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            int listH = g_ListH > 0 ? g_ListH : Scale(315);
            int rowH = g_RowH > 0 ? g_RowH : Scale(45);
            int totalListHeight = g_FilteredProcessCount * rowH;

            if (totalListHeight > listH) {
                int maxOffset = totalListHeight - listH;
                // 一次滾動 2 列
                g_ScrollOffset -= (zDelta / 120) * rowH * 2;
                if (g_ScrollOffset < 0) g_ScrollOffset = 0;
                if (g_ScrollOffset > maxOffset) g_ScrollOffset = maxOffset;

                // 只更新清單部分以防閃爍
                int listX = g_ListX;
                int listY = g_ListY;
                int listW = g_ListW;
                RECT listRect = {listX, listY, listX + listW, listY + listH};
                InvalidateRect(hwnd, &listRect, FALSE);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            // 清理 GDI 字型資源
            if (g_FontTitle) DeleteObject(g_FontTitle);
            if (g_FontHeader) DeleteObject(g_FontHeader);
            if (g_FontNormal) DeleteObject(g_FontNormal);
            if (g_FontSmall) DeleteObject(g_FontSmall);

            KillTimer(hwnd, TIMER_STATS_UPDATE);
            KillTimer(hwnd, TIMER_PROC_UPDATE);
            KillTimer(hwnd, TIMER_CLEAR_STATUS);

            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// 應用程式進入點
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // 註冊視窗類別
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = (HICON)LoadImageW(NULL, L"ramr.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    wcex.hIconSm       = (HICON)LoadImageW(NULL, L"ramr.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL; // 自行繪製背景，防止白閃爍
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = L"AntigravityMemoryOptimizerClass";

    if (!RegisterClassExW(&wcex)) {
        MessageBoxW(NULL, L"視窗類別註冊失敗！", L"錯誤", MB_ICONERROR);
        return 1;
    }

    // 為了計算真實的 client size，需使用 AdjustWindowRectEx（已加入 Scale 縮放支援）
    RECT wr = {0, 0, Scale(WIN_WIDTH), Scale(WIN_HEIGHT)};
    DWORD windowStyle = WS_OVERLAPPEDWINDOW; // 固定大小且沒有最大化按鈕
    /*
    DWORD windowStyle = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME; // 固定大小且沒有最大化按鈕
    */
    AdjustWindowRectEx(&wr, windowStyle, FALSE, 0);

    // 計算視窗寬高
    int winWidth = wr.right - wr.left;
    int winHeight = wr.bottom - wr.top;

    // 🚀 加強功能：計算螢幕中央座標，讓程式開啟時自動居中
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - winWidth) / 2;
    int posY = (screenHeight - winHeight) / 2;

    // 建立主視窗
    HWND hwnd = CreateWindowExW(
        0, L"AntigravityMemoryOptimizerClass", L"記憶體防護與碎片優化",
        windowStyle, posX, posY, // 👈 將原來的 CW_USEDEFAULT 替換為置中座標
        winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxW(NULL, L"主視窗建立失敗！", L"錯誤", MB_ICONERROR);
        return 1;
    }

    // 設定深色標題列 (DWM Immersive Dark Mode)
    // 適用於 Windows 11 與 Windows 10 Build 17763+
    BOOL useDarkMode = TRUE;
    #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
    #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
    #endif
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
    DwmSetWindowAttribute(hwnd, 19, &useDarkMode, sizeof(useDarkMode)); // 舊版 Win10 備用

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 主訊息迴圈
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
