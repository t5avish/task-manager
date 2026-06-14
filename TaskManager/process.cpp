#include "process.hpp"
#include "handle.hpp"
#include "debug.hpp"

#include <algorithm>
#include <commctrl.h>
#include <string>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

typedef LONG(NTAPI* NtQueryInformationThread_t)(
    HANDLE ThreadHandle,
    LONG ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
    );

static NtQueryInformationThread_t NtQueryInformationThread =
(NtQueryInformationThread_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationThread");

static const LONG ThreadSuspendCount = 35; // value for querying thread suspend count

static void for_each_process(auto f)
{
    handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

    if (snapshot == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        throw std::runtime_error("CreateToolhelp32Snapshot failed, error: " + std::to_string(err));
    }

    PROCESSENTRY32 pe32{ .dwSize = sizeof(PROCESSENTRY32) };

    if (!Process32First(snapshot, &pe32)) {
        DWORD err = GetLastError();
        throw std::runtime_error("Process32First failed, error: " + std::to_string(err));
    }

    do
    {
        f(pe32);
    } while (Process32Next(snapshot, &pe32));

    DWORD err = GetLastError();
    if (err != ERROR_NO_MORE_FILES && err != 0) {
        throw std::runtime_error("Process32Next failed, error: " + std::to_string(err));
    }
}

static HWND find_process_main_window(DWORD pid)
{
    struct EnumWindowData
    {
        DWORD pid;
        HWND main_window_hwnd = nullptr;
    } data{ pid };

    auto enum_windows_proc = [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto &data = *reinterpret_cast<EnumWindowData*>(lParam);

        DWORD window_pid = 0;
        if (!GetWindowThreadProcessId(hwnd, &window_pid)) {
            return TRUE;
        }

        if (window_pid != data.pid) {
            return TRUE;
        }

        if (GetWindow(hwnd, GW_OWNER) != nullptr) {
            return TRUE;
        }

        data.main_window_hwnd = hwnd;
        return FALSE;
    };

    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&data));
    return data.main_window_hwnd;
}

static bool is_thread_suspended(HANDLE thread)
{
    ULONG suspend_count = 0;
    LONG status = NtQueryInformationThread(thread, ThreadSuspendCount, &suspend_count, sizeof(suspend_count), nullptr);
    return status == 0 && suspend_count > 0;
}

static bool is_process_suspended(DWORD pid)
{
    handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    THREADENTRY32 te32{ .dwSize = sizeof(THREADENTRY32) };
    if (!Thread32First(snapshot, &te32)) {
        return false;
    }

    bool found_any = false;

    do {
        if (te32.th32OwnerProcessID != pid) {
            continue;
        }

        handle thread(OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID));
        if (!thread) {
            continue;
        }

        found_any = true;

        if (!is_thread_suspended(thread)) {
            return false;
        }

    } while (Thread32Next(snapshot, &te32));

    return found_any;
}

static ProcessStatus get_process_status(DWORD pid)
{
    HWND hwnd = find_process_main_window(pid);

    if (!hwnd) {
        return ProcessStatus::Running;
    }

    if (is_process_suspended(pid)) {
        return ProcessStatus::Suspended;
    }

    if (IsHungAppWindow(hwnd)) {
        return ProcessStatus::Not_responding;
    }

    return ProcessStatus::Running;
}

static bool soft_kill_process_by_pid(DWORD pid)
{
    HWND hwnd = find_process_main_window(pid);
    if (!hwnd) {
        dbg(L"No main window found for PID {}", pid);
        return false;
    }

    dbg(L"Sending WM_CLOSE to PID {}, hwnd {}", pid, (void*)hwnd);
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return true;
}

static bool try_hard_kill_process_by_pid(DWORD pid)
{
    handle process(OpenProcess(PROCESS_TERMINATE, FALSE, pid));

    if (!process) {
        dbg(L"OpenProcess failed for PID {}", pid);
        return false;
    }

    BOOL ok = TerminateProcess(process, 1);

    if (!ok) {
        dbg(L"TerminateProcess failed for PID {}", pid);
        return false;
    }

    dbg(L"Hard-killed PID {}", pid);
    return true;
}

static SIZE_T get_process_memory(DWORD pid)
{
    handle process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!process) return 0;

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (!GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) return 0;

    return pmc.PrivateUsage;
}

namespace process
{
    std::vector<ProcessInfo> get_all_processes_sorted()
    {
        std::vector<ProcessInfo> processes;

        for_each_process([&](const auto& pe32) {
            ProcessStatus status = get_process_status(pe32.th32ProcessID);
            SIZE_T memory = get_process_memory(pe32.th32ProcessID);
            processes.push_back({ pe32, status, memory });
            });

        std::sort(processes.begin(), processes.end(),
            [](const auto& a, const auto& b) {
                return _wcsicmp(a.entry.szExeFile, b.entry.szExeFile) < 0;
            });

        return processes;
    }

    void insert_process_into_listview(HWND listView_hwnd, const ProcessInfo& p)
    {
        std::wstring pid = std::to_wstring(p.entry.th32ProcessID);

        std::wstring status;
        switch (p.status)
        {
        case ProcessStatus::Running:
            status = L"Running";
            break;
        case ProcessStatus::Not_responding:
            status = L"Not_responding";
            break;
        case ProcessStatus::Suspended:
            status = L"Suspended";
            break;
        default:
            status = L"";
            break;
        }

        std::wstring memory = std::to_wstring(p.memory_bytes / 1024) + L" KB";

        LVITEM item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(listView_hwnd);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(p.entry.szExeFile);
        item.lParam = p.entry.th32ProcessID;

        int index = ListView_InsertItem(listView_hwnd, &item);

        ListView_SetItemText(listView_hwnd, index, 1, const_cast<LPWSTR>(pid.c_str()));
        ListView_SetItemText(listView_hwnd, index, 2, const_cast<LPWSTR>(status.c_str()));
        ListView_SetItemText(listView_hwnd, index, 3, const_cast<LPWSTR>(memory.c_str()));
    }

    void end_task(const ProcessInfo& p)
    {
        if (p.status == ProcessStatus::Suspended) {
            try_hard_kill_process_by_pid(p.entry.th32ProcessID);
        }
        else if (!soft_kill_process_by_pid(p.entry.th32ProcessID)) {
            try_hard_kill_process_by_pid(p.entry.th32ProcessID);
        }
    }
}
