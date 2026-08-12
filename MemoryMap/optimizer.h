#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <windows.h>

// 安全級別定義
#define SAFETY_SAFE     0  // 綠色：安全可關閉 (一般使用者軟體)
#define SAFETY_CAUTION  1  // 黃色：建議保留 (系統服務或非關鍵 Windows 工具)
#define SAFETY_CRITICAL 2  // 紅色：核心系統 (絕對不可關閉，否則會藍屏)
#define SAFETY_PROTECTED 3 // 綠色鎖定：白名單安全保護中 (不可點擊關閉)

// 行程資訊結構體
typedef struct {
    DWORD pid;
    WCHAR name[128];
    WCHAR path[MAX_PATH];
    SIZE_T memoryUsage; // 記憶體使用量 (Byte)
    int safety;         // 安全分類 (SAFETY_*)
    WCHAR runDuration[32]; // 上線時長文字
    WCHAR description[256]; // 行程功能與安全說明
} ProcessInfo;

// 記憶體統計結構體
typedef struct {
    ULONGLONG totalPhys;
    ULONGLONG availPhys;
    ULONGLONG totalPageFile;
    ULONGLONG availPageFile;
    DWORD memoryLoad;           // 記憶體使用百分比 (0-100)
    double fragmentationScore;  // 碎片化程度估計值 (0-100)
} MemoryStats;

// 函式宣告
BOOL GetSystemMemoryStats(MemoryStats* stats);
int GetProcessList(ProcessInfo* list, int maxCount);
BOOL OptimizeSystemMemory(HWND hwndParent);
BOOL TerminateTargetProcess(DWORD pid);

// 白名單管理
BOOL IsWhitelisted(const WCHAR* name);
void AddRemoveWhitelist(const WCHAR* name, BOOL add);
void LoadWhitelist();
void SaveWhitelist();

#endif // OPTIMIZER_H
