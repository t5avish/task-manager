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
    SIZE_T memory_bytes;
};

namespace process
{
    std::vector<ProcessInfo> get_all_processes_sorted();

    void insert_process_into_listview(HWND listview, const ProcessInfo& process);

    void end_task(const ProcessInfo& p);
}
