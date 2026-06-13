#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include "process.hpp"
#include "button.hpp"
#include "listview.hpp"

#pragma comment(lib, "Comctl32.lib")

#define ID_END_TASK 1001
#define ID_MENU_REFRESH_AUTO_2   3001
#define ID_MENU_REFRESH_AUTO_5   3002
#define ID_MENU_REFRESH_AUTO_10  3003
#define ID_MENU_REFRESH_MANUAL   3004
#define ID_REFRESH_NOW 3005

#define WM_PROCESS_DATA_READY (WM_APP + 1)

static ListView process_list;
static Button end_task_button;
static DWORD selected_pid = 0;
static HACCEL haccel = nullptr;
static std::atomic<UINT> refresh_rate = 2000;
static bool is_refreshing = false;
static std::atomic<bool> running = true;
static std::atomic<bool> auto_refresh = true;

static std::vector<ProcessInfo> shared_processes;
static std::mutex processes_mutex;
static std::thread worker_thread;

static HANDLE refresh_event = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static RECT calculate_listview_rect(int width, int height)
{
    constexpr int MARGIN = 10;
    return RECT {
        MARGIN,
        MARGIN,
        width - MARGIN,
        height - end_task_button.height - end_task_button.margin_from_corner * 2 - MARGIN
    };
}

static void update_refresh_checkmarks(HWND hwnd, UINT active_id)
{
    HMENU menu_bar = GetMenu(hwnd);
    HMENU options = GetSubMenu(menu_bar, 0);
    HMENU refresh = GetSubMenu(options, 0);
    HMENU auto_menu = GetSubMenu(refresh, 0);

    if (active_id == 0) {
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_2, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_5, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(auto_menu, ID_MENU_REFRESH_AUTO_10, MF_BYCOMMAND | MF_UNCHECKED);
        return;
    }

    CheckMenuRadioItem(auto_menu, ID_MENU_REFRESH_AUTO_2, ID_MENU_REFRESH_AUTO_10, active_id, MF_BYCOMMAND);
}

static void refresh_ui()
{
    is_refreshing = true;

    DWORD prev_pid = selected_pid;
    int top_index = ListView_GetTopIndex(process_list.hwnd);
    RECT item_rect;
    ListView_GetItemRect(process_list.hwnd, 0, &item_rect, LVIR_BOUNDS);
    int row_height = item_rect.bottom - item_rect.top;

    SendMessage(process_list.hwnd, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(process_list.hwnd);

    {
        std::lock_guard<std::mutex> lock(processes_mutex);
        for (const auto& p : shared_processes) {
            process::insert_process_into_listview(process_list.hwnd, p);
        }
    }

    SendMessage(process_list.hwnd, LVM_SCROLL, 0, top_index * row_height);

    selected_pid = 0;
    EnableWindow(end_task_button.hwnd, FALSE);

    int count = ListView_GetItemCount(process_list.hwnd);
    for (int i = 0; i < count; i++) {
        LVITEM item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        ListView_GetItem(process_list.hwnd, &item);

        if (static_cast<DWORD>(item.lParam) == prev_pid) {
            ListView_SetItemState(process_list.hwnd, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            selected_pid = prev_pid;
            EnableWindow(end_task_button.hwnd, TRUE);
            break;
        }
    }

    SendMessage(process_list.hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(process_list.hwnd, NULL, TRUE);

    is_refreshing = false;
}

static void process_worker(HWND hwnd)
{
    while (running) {
        WaitForSingleObject(refresh_event, auto_refresh ? refresh_rate.load() : INFINITE);

        if (!running) break;

        auto processes = process::get_all_processes_sorted();
        {
            std::lock_guard<std::mutex> lock(processes_mutex);
            shared_processes = std::move(processes);
        }
        PostMessage(hwnd, WM_PROCESS_DATA_READY, 0, 0);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"Task Manager";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Task Manager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    ACCEL accels[] = {
    { FVIRTKEY, VK_F5, ID_REFRESH_NOW }
    };
    haccel = CreateAcceleratorTable(accels, 1);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(hwnd, haccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(haccel);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        running = false;
        SetEvent(refresh_event);

        if (worker_thread.joinable()) {
            worker_thread.join();
        }

        CloseHandle(refresh_event);
        PostQuitMessage(0);
        return 0;
    }

    case WM_CREATE:
    {
        HINSTANCE instance = ((LPCREATESTRUCT)lParam)->hInstance;

        RECT rc;
        GetClientRect(hwnd, &rc);

        HMENU menu_bar = CreateMenu();
        HMENU options_menu = CreateMenu();
        HMENU refresh_menu = CreateMenu();
        HMENU auto_menu = CreateMenu();

        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_2, L"2 seconds");
        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_5, L"5 seconds");
        AppendMenu(auto_menu, MF_STRING, ID_MENU_REFRESH_AUTO_10, L"10 seconds");

        AppendMenu(refresh_menu, MF_POPUP, (UINT_PTR)auto_menu, L"Auto");
        AppendMenu(refresh_menu, MF_STRING, ID_MENU_REFRESH_MANUAL, L"Manual (F5)");
        AppendMenu(options_menu, MF_POPUP, (UINT_PTR)refresh_menu, L"Refresh");

        AppendMenu(menu_bar, MF_POPUP, (UINT_PTR)options_menu, L"Options");

        SetMenu(hwnd, menu_bar);

        process_list.create(hwnd, instance, calculate_listview_rect(rc.right, rc.bottom));

        const std::wstring columns[] = { L"Name", L"PID", L"Status" };
        const int widths[] = { 200, 100, 100 };

        LVCOLUMN col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

        for (int i = 0; i < 3; i++) {
            col.pszText = const_cast<LPWSTR>(columns[i].c_str());
            col.cx = widths[i];
            ListView_InsertColumn(process_list.hwnd, i, &col);
        }

        end_task_button.create(hwnd, instance, L"End task", 0, 0, ID_END_TASK);
        end_task_button.position_bottom_right(rc.right, rc.bottom);
        
        refresh_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        worker_thread = std::thread(process_worker, hwnd);

        SetEvent(refresh_event);

        EnableWindow(end_task_button.hwnd, FALSE);

        update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_2);
        return 0;
    }

    case WM_SIZE:
    {
        int window_width = LOWORD(lParam);
        int window_height = HIWORD(lParam);

        end_task_button.position_bottom_right(window_width, window_height);
        process_list.resize(calculate_listview_rect(window_width, window_height));
        return 0;
    }

    case WM_NOTIFY:
    {
        if (is_refreshing) return 0;

        auto* hdr = reinterpret_cast<LPNMHDR>(lParam);

        if (hdr->hwndFrom != process_list.hwnd || hdr->code != LVN_ITEMCHANGED) {
            break;
        }

        auto* lv = reinterpret_cast<LPNMLISTVIEW>(lParam);

        if (!(lv->uChanged & LVIF_STATE)) {
            break;
        }

        const bool is_selected = (lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED);

        const bool is_deselected = !(lv->uNewState & LVIS_SELECTED) && (lv->uOldState & LVIS_SELECTED);

        if (is_selected) {
            LVITEM item{};
            item.mask = LVIF_PARAM;
            item.iItem = lv->iItem;

            ListView_GetItem(process_list.hwnd, &item);

            selected_pid = static_cast<DWORD>(item.lParam);
            EnableWindow(end_task_button.hwnd, TRUE);

        } else if (is_deselected) {
            selected_pid = 0;
            EnableWindow(end_task_button.hwnd, FALSE);
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {

        // END TASK BUTTON
        case ID_END_TASK:
        {
            if (HIWORD(wParam) == BN_CLICKED && selected_pid)
            {
                if (!process::soft_kill_process_by_pid(selected_pid)) {
                    process::try_hard_kill_process_by_pid(selected_pid);
                }

                HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, selected_pid);
                if (h) {
                    WaitForSingleObject(h, 2000);
                    CloseHandle(h);
                }

                refresh_ui();
            }
            return 0;
        }

        // MENU
        case ID_MENU_REFRESH_AUTO_2:
        {
            auto_refresh = true;
            refresh_rate = 2000;
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_2);
            SetEvent(refresh_event);
            return 0;
        }

        case ID_MENU_REFRESH_AUTO_5:
        {
            auto_refresh = true;
            refresh_rate = 5000;
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_5);
            SetEvent(refresh_event);
            return 0;
        }

        case ID_MENU_REFRESH_AUTO_10:
        {
            auto_refresh = true;
            refresh_rate = 10000;
            update_refresh_checkmarks(hwnd, ID_MENU_REFRESH_AUTO_10);
            SetEvent(refresh_event);
            return 0;
        }

        // MANUAL REFRESH
        case ID_MENU_REFRESH_MANUAL:
        {
            auto_refresh = false;
            update_refresh_checkmarks(hwnd, 0);
            return 0;
        }

        case ID_REFRESH_NOW:
        {
            SetEvent(refresh_event);
            return 0;
        }
        }

        break;
    }

    case WM_PROCESS_DATA_READY:
    {
        refresh_ui();
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
