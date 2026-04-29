#pragma once
#include "scope.hpp"
#include <format>
#define dbg(...) OutputDebugStringW(std::format(L"[TaskManager]: " __VA_ARGS__).c_str())

namespace common {

    using scope::scope_exit;

    inline void exception_guard(auto f)
    {
        try {
            f();
        }
        catch (const std::exception& e) {
            std::wstring msg(e.what(), e.what() + std::strlen(e.what()));
            dbg(L"exception: {}", msg);
        }
    }

    inline void for_each_process(auto f)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            throw std::exception("CreateToolhelp32Snapshot : INVALID_HANDLE_VALUE");
        }
        auto _ = scope_exit([=]() { CloseHandle(snapshot); });

        PROCESSENTRY32 pe32{ .dwSize = sizeof(PROCESSENTRY32) };

        if (!Process32First(snapshot, &pe32)) {
            throw std::exception("Process32First : failed");
        }

        do
        {
            f(pe32);
        } while (Process32Next(snapshot, &pe32));
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            throw std::exception("Process32Next : ERROR_NO_MORE_FILES");
        }
    }

    inline std::vector<PROCESSENTRY32> get_all_processes()
    {
        std::vector<PROCESSENTRY32> processes;
        exception_guard([&]() {
            for_each_process([&](const auto& pe32) {
                processes.push_back(pe32);
            });

            std::sort(processes.begin(), processes.end(),
                [](const auto& a, const auto& b) {
                    return _wcsicmp(a.szExeFile, b.szExeFile) < 0;
            });
        });

        return processes;
    }

    inline void insert_processes_into_grid(HWND& listView_hwnd)
    {
        auto processes = get_all_processes();

        for (const auto& p : processes)
        {
            std::wstring name = p.szExeFile;
            std::wstring pid = std::to_wstring(p.th32ProcessID);
            std::wstring status = L""; // *********** TO BE MADE

            LVITEM item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = ListView_GetItemCount(listView_hwnd);
            item.iSubItem = 0;
            item.pszText = (LPWSTR)name.c_str();
            item.lParam = p.th32ProcessID;

            int index = ListView_InsertItem(listView_hwnd, &item);

            ListView_SetItemText(listView_hwnd, index, 1, (LPWSTR)pid.c_str());
            ListView_SetItemText(listView_hwnd, index, 2, (LPWSTR)status.c_str());
        }
    }

    inline HWND find_process_main_window(DWORD pid)
    {
        struct EnumWindowData
        {
            DWORD pid;
            HWND main_window_hwnd = nullptr;
        } data{ pid };

        auto enum_windows_proc = [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto& data = *reinterpret_cast<EnumWindowData*>(lParam);

            DWORD window_pid = 0;
            GetWindowThreadProcessId(hwnd, &window_pid);
            if (window_pid != data.pid) {
                return TRUE;
            }

            if (GetWindow(hwnd, GW_OWNER) != nullptr) {
                return TRUE;
            }

            /*
            if (!IsWindowVisible(hwnd)) {
                return TRUE;
            }
            */

            data.main_window_hwnd = hwnd;
            return FALSE;
            };

        EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&data));
        return data.main_window_hwnd;
    }

    inline bool soft_kill_process_by_pid(DWORD pid)
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

    inline bool hard_kill_process_by_pid(DWORD pid)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!hProcess) {
            dbg(L"OpenProcess failed for PID {}", pid);
            return false;
        }

        BOOL ok = TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);

        if (!ok) {
            dbg(L"TerminateProcess failed for PID {}", pid);
            return false;
        }

        dbg(L"Hard-killed PID {}", pid);
        return true;
    }
}