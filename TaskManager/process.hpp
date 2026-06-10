#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <vector>

namespace process
{
    std::vector<PROCESSENTRY32> get_all_processes_sorted();

    void insert_process_into_listview(HWND listview, const PROCESSENTRY32& process);

    bool soft_kill_process_by_pid(DWORD pid);

    bool try_hard_kill_process_by_pid(DWORD pid);
}
