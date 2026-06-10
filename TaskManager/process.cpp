#include "process.hpp"
#include "handle.hpp"
#include "debug.hpp"

#include <algorithm>
#include <commctrl.h>
#include <string>

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

    if (!EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&data))) {
        DWORD err = GetLastError();
        if (err != 0) {
            dbg(L"EnumWindows failed : error {} for PID {}", err, pid);
        }
        return nullptr;
    }
    return data.main_window_hwnd;
}

namespace process
{
    std::vector<PROCESSENTRY32> get_all_processes_sorted()
    {
        std::vector<PROCESSENTRY32> processes;

        for_each_process([&](const auto& pe32) {
            processes.push_back(pe32);
            });

        std::sort(processes.begin(), processes.end(),
            [](const auto& a, const auto& b) {
                return _wcsicmp(a.szExeFile, b.szExeFile) < 0;
            });

        return processes;
    }

    void insert_process_into_listview(HWND listView_hwnd, const PROCESSENTRY32& p)
    {
        std::wstring pid = std::to_wstring(p.th32ProcessID);

        // TODO: derive process status(running / suspended / unresponsive)
        // PROCESSENTRY32 does not provide this
        std::wstring status = L"";

        LVITEM item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(listView_hwnd);
        item.iSubItem = 0;

        item.pszText = const_cast<LPWSTR>(p.szExeFile);
        item.lParam = p.th32ProcessID;

        int index = ListView_InsertItem(listView_hwnd, &item);

        ListView_SetItemText(listView_hwnd, index, 1, const_cast<LPWSTR>(pid.c_str()));

        ListView_SetItemText(listView_hwnd, index, 2, const_cast<LPWSTR>(status.c_str()));
    }

    bool soft_kill_process_by_pid(DWORD pid)
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

    bool try_hard_kill_process_by_pid(DWORD pid)
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
}