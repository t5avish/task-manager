#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <vector>

enum class ProcessStatus
{
    Running,
    Not_responding,
    Suspended
};

struct ProcessInfo
{
    PROCESSENTRY32 entry;
    ProcessStatus status;
};

namespace process
{
    std::vector<ProcessInfo> get_all_processes_sorted();

    void insert_process_into_listview(HWND listview, const ProcessInfo& process);

    bool soft_kill_process_by_pid(DWORD pid);

    bool try_hard_kill_process_by_pid(DWORD pid);
}
