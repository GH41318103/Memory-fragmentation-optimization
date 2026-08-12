#include "optimizer.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdio.h>

// 全域白名單狀態
static WCHAR g_Whitelist[64][128] = {0};
static int g_WhitelistCount = 0;

// 宣告動態載入的 IsProcessCritical 函數指標
typedef BOOL (WINAPI *pfnIsProcessCritical)(HANDLE, PBOOL);

// 判斷行程是否為核心關鍵行程
static BOOL CheckIfProcessIsCritical(HANDLE hProcess) {
    BOOL isCritical = FALSE;
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        pfnIsProcessCritical pIsCritical = (pfnIsProcessCritical)GetProcAddress(hKernel, "IsProcessCritical");
        if (pIsCritical) {
            pIsCritical(hProcess, &isCritical);
        }
    }
    return isCritical;
}

// 獲取系統記憶體詳細資訊
BOOL GetSystemMemoryStats(MemoryStats* stats) {
    if (!stats) return FALSE;

    // 獲取實體與虛擬記憶體狀態
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex)) {
        return FALSE;
    }

    stats->totalPhys = statex.ullTotalPhys;
    stats->availPhys = statex.ullAvailPhys;
    stats->totalPageFile = statex.ullTotalPageFile;
    stats->availPageFile = statex.ullAvailPageFile;
    stats->memoryLoad = statex.dwMemoryLoad;

    // 使用 Performance Info 計算碎片率與優化空間
    PERFORMANCE_INFORMATION perfInfo;
    perfInfo.cb = sizeof(perfInfo);
    if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
        // 碎片化程度估算公式：
        // 系統快取與核心分頁池在記憶體高負載下通常散落在實體記憶體中，形成碎片。
        // 當系統快取與核心分頁所佔比例越高，或者運作時間越長、行程越多，記憶體重新排列與整理的空間就越大。
        double cacheRatio = (double)(perfInfo.SystemCache * perfInfo.PageSize) / (double)stats->totalPhys;
        double kernelRatio = (double)(perfInfo.KernelPaged * perfInfo.PageSize) / (double)stats->totalPhys;
        
        // 碎片率 = (核心分頁比例 + 快取比例) * 100 + 行程數影響權重
        double score = (kernelRatio + cacheRatio * 0.4) * 100.0 + (perfInfo.ProcessCount * 0.05);
        if (score > 95.0) score = 95.0;
        if (score < 5.0) score = 5.0;
        
        // 扣除目前記憶體可用量比例的影響，可用記憶體越多，碎片化感知應越低
        double freeRatio = (double)stats->availPhys / (double)stats->totalPhys;
        score = score * (1.0 - freeRatio * 0.5);

        stats->fragmentationScore = score;
    } else {
        // 備份估算公式
        stats->fragmentationScore = (double)(stats->memoryLoad * 0.45 + 10.0);
    }

    return TRUE;
}

// 行程安全分類與描述生成
static void EvaluateProcessSafety(ProcessInfo* proc, DWORD sessionId, BOOL isKernelCritical) {
    // 預設為安全
    proc->safety = SAFETY_SAFE;
    wcscpy(proc->description, L"第三方應用程式。可以安全關閉以釋放記憶體。");

    // 1. 強制核心關鍵行程 (IsProcessCritical 回傳 TRUE)
    if (isKernelCritical) {
        proc->safety = SAFETY_CRITICAL;
        wcscpy(proc->description, L"系統核心關鍵行程！結束此行程會導致 Windows 立即藍屏 (BSOD)。");
        return;
    }

    // 2. 比對知名關鍵行程名稱
    const struct {
        const WCHAR* name;
        int safety;
        const WCHAR* desc;
    } knownProcesses[] = {
        { L"System", SAFETY_CRITICAL, L"Windows 系統核心核心，不可終止。" },
        { L"Idle", SAFETY_CRITICAL, L"系統空閒行程，用以量測 CPU 閒置容量。" },
        { L"smss.exe", SAFETY_CRITICAL, L"Session Manager Subsystem (對話管理子系統)，結束會立即藍屏！" },
        { L"csrss.exe", SAFETY_CRITICAL, L"Client Server Runtime Process (客戶端服務子系統)，結束會立即藍屏！" },
        { L"wininit.exe", SAFETY_CRITICAL, L"Windows 啟動初始化行程，結束會立即藍屏！" },
        { L"services.exe", SAFETY_CRITICAL, L"Windows 服務管理員，結束會引發系統崩潰與藍屏！" },
        { L"lsass.exe", SAFETY_CRITICAL, L"本地安全性授權子系統，結束會強制系統在一分鐘內重啟！" },
        { L"winlogon.exe", SAFETY_CRITICAL, L"Windows 登入行程，結束會導致使用者登出或重啟。" },
        { L"dwm.exe", SAFETY_CRITICAL, L"桌面視窗管理員 (DWM)，負責視窗圖形渲染，結束會閃屏重啟。" },
        
        { L"explorer.exe", SAFETY_CAUTION, L"Windows 資源管理器，結束後桌面與工作列會消失，但可重新執行。" },
        { L"svchost.exe", SAFETY_CAUTION, L"Windows 服務主機，承載多個關鍵後台系統服務，結束會使多個服務失效。" },
        { L"spoolsv.exe", SAFETY_CAUTION, L"列印多工緩衝處理器。若無使用印表機需求，可關閉釋放記憶體。" },
        { L"taskhostw.exe", SAFETY_CAUTION, L"Windows 工作主機，負責執行系統排程任務。建議保留。" },
        { L"ctfmon.exe", SAFETY_CAUTION, L"輸入法文字服務，結束後可能導致中文輸入法無法正常切換。" },
        { L"sihost.exe", SAFETY_CAUTION, L"Shell Infrastructure Host (殼層基礎結構主機)，不建議終止。" },
        { L"fontdrvhost.exe", SAFETY_CAUTION, L"使用者模式字型驅動程式主機。建議保留。" },
        { L"conhost.exe", SAFETY_CAUTION, L"主控台視窗主機，負責命令提示字元等的介面。建議保留。" },
        { L"SecurityHealthService.exe", SAFETY_CAUTION, L"Windows 安全中心服務行程。建議保留。" }
    };

    int knownCount = sizeof(knownProcesses) / sizeof(knownProcesses[0]);
    for (int i = 0; i < knownCount; i++) {
        if (_wcsicmp(proc->name, knownProcesses[i].name) == 0) {
            proc->safety = knownProcesses[i].safety;
            wcscpy(proc->description, knownProcesses[i].desc);
            return;
        }
    }

    // 3. 根據 Session ID 與執行路徑判斷
    // Session 0 代表 Windows 系統服務，非使用者直接開啟的程式
    if (sessionId == 0) {
        proc->safety = SAFETY_CAUTION;
        wcscpy(proc->description, L"背景系統服務。結束此服務可能會造成某些硬體或系統功能失效。");
        return;
    }

    // 4. 比對路徑是否位於 Windows 目錄
    WCHAR winDir[MAX_PATH];
    if (GetWindowsDirectoryW(winDir, MAX_PATH)) {
        size_t winDirLen = wcslen(winDir);
        if (_wcsnicmp(proc->path, winDir, winDirLen) == 0) {
            proc->safety = SAFETY_CAUTION;
            wcscpy(proc->description, L"位於 Windows 系統目錄下的系統工具或元件，建議保留。");
            return;
        }
    }

    // 5. 常見第三方軟體加速描述
    if (_wcsicmp(proc->name, L"chrome.exe") == 0) {
        wcscpy(proc->description, L"Google Chrome 瀏覽器。通常佔用極高記憶體，可安全關閉。");
    } else if (_wcsicmp(proc->name, L"msedge.exe") == 0) {
        wcscpy(proc->description, L"Microsoft Edge 瀏覽器。通常佔用大量記憶體，可安全關閉。");
    } else if (_wcsicmp(proc->name, L"spotify.exe") == 0) {
        wcscpy(proc->description, L"Spotify 音樂軟體。安全可關閉。");
    } else if (_wcsicmp(proc->name, L"discord.exe") == 0) {
        wcscpy(proc->description, L"Discord 語音社群軟體。安全可關閉。");
    } else if (_wcsicmp(proc->name, L"steam.exe") == 0) {
        wcscpy(proc->description, L"Steam 遊戲平台。安全可關閉。");
    } else if (_wcsicmp(proc->name, L"onedrive.exe") == 0) {
        wcscpy(proc->description, L"Microsoft OneDrive 雲端同步程式。安全可關閉。");
    }

    // 6. 白名單安全保護覆蓋
    if (proc->safety != SAFETY_CRITICAL && IsWhitelisted(proc->name)) {
        proc->safety = SAFETY_PROTECTED;
        wcscpy(proc->description, L"[已受白名單保護] 使用者自訂白名單行程。無法點選結束。");
    }
}

// 取得執行中行程清單
int GetProcessList(ProcessInfo* list, int maxCount) {
    if (!list || maxCount <= 0) return 0;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    int count = 0;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            // 跳過 PID 0 (Idle) 與 PID 4 (System) 以免無法獲取資訊，後續手動填寫或過濾
            if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) {
                // 手動新增 System 與 Idle 以確保完整性
                ProcessInfo* proc = &list[count];
                proc->pid = pe.th32ProcessID;
                wcscpy(proc->name, pe.szExeFile);
                proc->path[0] = L'\0';
                proc->memoryUsage = (pe.th32ProcessID == 4) ? (1024 * 1024 * 16) : 0; // 粗略數值
                EvaluateProcessSafety(proc, 0, TRUE);
                count++;
                if (count >= maxCount) break;
                continue;
            }

            // 開啟行程以獲取詳細資訊 (路徑、Session ID、記憶體大小、是否關鍵)
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            
            // 如果無法以 QUERY 權限開啟，嘗試以最低權限開啟
            if (!hProcess) {
                hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            }

            ProcessInfo* proc = &list[count];
            proc->pid = pe.th32ProcessID;
            wcscpy(proc->name, pe.szExeFile);
            proc->path[0] = L'\0';
            proc->memoryUsage = 0;
            proc->runDuration[0] = L'\0';

            DWORD sessionId = 1; // 預設使用者 Session
            BOOL isCritical = FALSE; // 預設非核心行程

            if (hProcess) {
                // 1. 取得執行檔完整路徑
                DWORD pathSize = MAX_PATH;
                QueryFullProcessImageNameW(hProcess, 0, proc->path, &pathSize);

                // 2. 取得工作集記憶體大小
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    proc->memoryUsage = pmc.WorkingSetSize;
                }

                // 3. 取得 Session ID
                ProcessIdToSessionId(pe.th32ProcessID, &sessionId);

                // 4. 取得啟動時間並計算上線時長
                FILETIME ftCreation, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                    FILETIME ftNow;
                    GetSystemTimeAsFileTime(&ftNow);
                    ULARGE_INTEGER startTime;
                    startTime.LowPart = ftCreation.dwLowDateTime;
                    startTime.HighPart = ftCreation.dwHighDateTime;
                    ULARGE_INTEGER nowTime;
                    nowTime.LowPart = ftNow.dwLowDateTime;
                    nowTime.HighPart = ftNow.dwHighDateTime;
                    unsigned long long elapsed100ns = 0ULL;
                    if (nowTime.QuadPart > startTime.QuadPart) {
                        elapsed100ns = nowTime.QuadPart - startTime.QuadPart;
                    }
                    unsigned long long elapsedSeconds = elapsed100ns / 10000000ULL;
                    unsigned long long hours = elapsedSeconds / 3600ULL;
                    unsigned long long minutes = (elapsedSeconds % 3600ULL) / 60ULL;
                    unsigned long long seconds = elapsedSeconds % 60ULL;
                    if (hours > 0ULL) {
                        swprintf(proc->runDuration, 32, L"%02llu:%02llu:%02llu", hours, minutes, seconds);
                    } else {
                        swprintf(proc->runDuration, 32, L"%02llu:%02llu", minutes, seconds);
                    }
                } else {
                    wcscpy(proc->runDuration, L"未知");
                }

                // 5. 判斷是否為 critical 行程

                // 4. 判斷是否為 critical 行程
                isCritical = CheckIfProcessIsCritical(hProcess);

                CloseHandle(hProcess);
            } else {
                // 如果無法開啟行程 (例如某些系統保護行程，如 Registry, Audiodg 等)
                // 這類行程通常是系統核心級別，安全評估設為系統核心或服務
                sessionId = 0;
            }

            // 5. 評估安全評級與說明
            EvaluateProcessSafety(proc, sessionId, isCritical);

            // 若無法讀取記憶體用量且不為 System，則嘗試用 GetProcessMemoryInfo 重試
            // 如果還是 0，則可能是受保護的系統服務，賦予一個基礎估計值或保持 0
            if (proc->memoryUsage == 0 && proc->safety == SAFETY_CRITICAL) {
                proc->memoryUsage = 1024 * 1024 * 4; // 預估 4MB
            }

            count++;
            if (count >= maxCount) break;

        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return count;
}

// 整理與最佳化記憶體
BOOL OptimizeSystemMemory(HWND hwndParent) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    BOOL success = FALSE;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            // 跳過 Idle, System 等關鍵系統核心，避免不必要的開銷
            if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4 || pe.th32ProcessID == GetCurrentProcessId()) {
                continue;
            }

            // 開啟行程並執行工作集釋放 (Working Set Trimming)
            // 需要 PROCESS_SET_QUOTA 權限
            HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProcess) {
                if (EmptyWorkingSet(hProcess)) {
                    success = TRUE;
                }
                CloseHandle(hProcess);
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);

    // 清空系統檔案快取 (如果為管理員運行)
    // 透過傳入 NULL 與指定大小，可以叫作業系統縮小快取尺寸
    SIZE_T minCache = (SIZE_T)-1;
    SIZE_T maxCache = (SIZE_T)-1;
    SetSystemFileCacheSize(minCache, maxCache, 0);

    return success;
}

// 結束目標行程
BOOL TerminateTargetProcess(DWORD pid) {
    // 預防機制：不能終止自己
    if (pid == GetCurrentProcessId()) return FALSE;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return FALSE;

    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result;
}

// 實作白名單管理與儲存讀取函式
BOOL IsWhitelisted(const WCHAR* name) {
    for (int i = 0; i < g_WhitelistCount; i++) {
        if (_wcsicmp(g_Whitelist[i], name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

void SaveWhitelist() {
    FILE* fp = _wfopen(L"whitelist.txt", L"w, ccs=UTF-8");
    if (!fp) return;
    for (int i = 0; i < g_WhitelistCount; i++) {
        fwprintf(fp, L"%s\n", g_Whitelist[i]);
    }
    fclose(fp);
}

void LoadWhitelist() {
    g_WhitelistCount = 0;
    FILE* fp = _wfopen(L"whitelist.txt", L"r, ccs=UTF-8");
    if (!fp) return;
    WCHAR line[128];
    while (fgetws(line, 128, fp)) {
        size_t len = wcslen(line);
        while (len > 0 && (line[len - 1] == L'\n' || line[len - 1] == L'\r')) {
            line[len - 1] = L'\0';
            len--;
        }
        if (len > 0 && g_WhitelistCount < 64) {
            wcscpy(g_Whitelist[g_WhitelistCount++], line);
        }
    }
    fclose(fp);
}

void AddRemoveWhitelist(const WCHAR* name, BOOL add) {
    if (add) {
        if (IsWhitelisted(name)) return;
        if (g_WhitelistCount < 64) {
            wcscpy(g_Whitelist[g_WhitelistCount++], name);
            SaveWhitelist();
        }
    } else {
        int index = -1;
        for (int i = 0; i < g_WhitelistCount; i++) {
            if (_wcsicmp(g_Whitelist[i], name) == 0) {
                index = i;
                break;
            }
        }
        if (index != -1) {
            for (int i = index; i < g_WhitelistCount - 1; i++) {
                wcscpy(g_Whitelist[i], g_Whitelist[i + 1]);
            }
            g_WhitelistCount--;
            SaveWhitelist();
        }
    }
}
