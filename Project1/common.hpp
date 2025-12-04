#pragma once
#include "scope.hpp"
#define dbg(...) OutputDebugStringW(std::format(__VA_ARGS__).c_str())

namespace common {

    using scope::scope_exit;

    inline void exception_guard(auto f)
    {
        try {
            f();
        }
        catch (const std::exception& e) {
            dbg(L"exception: {}", std::wstring(e.what(), e.what() + strlen(e.what())));
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
            item.mask = LVIF_TEXT;
            item.iItem = ListView_GetItemCount(listView_hwnd);
            item.iSubItem = 0;
            item.pszText = (LPWSTR)name.c_str();

            int index = ListView_InsertItem(listView_hwnd, &item);

            ListView_SetItemText(listView_hwnd, index, 1, (LPWSTR)pid.c_str());
            ListView_SetItemText(listView_hwnd, index, 2, (LPWSTR)status.c_str());
        }
    }
}